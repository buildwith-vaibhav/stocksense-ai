#include "stocksense/services/MlScorer.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdlib>
#include <fstream>

namespace stocksense::services {

namespace {
std::string defaultModelPath() {
  if (const char* p = std::getenv("STOCKSENSE_MODEL_PATH")) {
    if (*p) return std::string(p);
  }
  // Relative to cwd when running from project root or build/Release
  return "models/stocksense_lr.json";
}
}  // namespace

MlScorer::MlScorer(std::string modelPath)
    : path_(modelPath.empty() ? defaultModelPath() : std::move(modelPath)) {
  reload();
}

double MlScorer::sigmoid(double z) {
  if (z > 30) return 1.0;
  if (z < -30) return 0.0;
  return 1.0 / (1.0 + std::exp(-z));
}

bool MlScorer::reload() {
  loaded_ = false;
  coefficients_.clear();
  feature_names_.clear();
  version_ = "none";
  intercept_ = 0.0;

  std::ifstream in(path_);
  if (!in) {
    // Try relative to executable-friendly fallbacks
    const char* fallbacks[] = {
        "models/stocksense_lr.json",
        "../models/stocksense_lr.json",
        "../../models/stocksense_lr.json",
        "D:/stocksense-ai-stage7/models/stocksense_lr.json",
    };
    for (const char* fb : fallbacks) {
      in.open(fb);
      if (in) {
        path_ = fb;
        break;
      }
    }
  }
  if (!in) {
    spdlog::warn("ML model not found at '{}' — probability_up disabled", path_);
    return false;
  }

  try {
    nlohmann::json j;
    in >> j;
    version_ = j.value("version", "unknown");
    intercept_ = j.value("intercept", 0.0);
    if (j.contains("feature_names")) {
      for (const auto& n : j["feature_names"]) feature_names_.push_back(n.get<std::string>());
    }
    if (j.contains("coefficients")) {
      for (const auto& c : j["coefficients"]) coefficients_.push_back(c.get<double>());
    }
    if (coefficients_.empty()) {
      spdlog::error("ML model has no coefficients");
      return false;
    }
    loaded_ = true;
    spdlog::info("ML model loaded: {} ({} features) from {}", version_,
                 coefficients_.size(), path_);
    return true;
  } catch (const std::exception& e) {
    spdlog::error("ML model parse failed: {}", e.what());
    return false;
  }
}

std::vector<double> MlScorer::featuresFrom(
    const repositories::AnalysisSnapshot& snap) {
  const double price = snap.has_price ? snap.price : 0.0;
  const double denom = price > 1e-6 ? price : 1.0;
  const double rsi = snap.has_indicators ? snap.rsi_14 : 50.0;
  const double sma20 = snap.has_indicators ? snap.sma_20 : price;
  const double sma50 = snap.has_indicators ? snap.sma_50 : price;
  const double macd_h = snap.has_indicators ? snap.macd_hist : 0.0;
  const double atr = snap.has_indicators ? snap.atr_14 : 0.0;

  return {
      rsi / 100.0,
      (price - sma20) / denom,
      (price - sma50) / denom,
      (sma20 - sma50) / denom,
      std::tanh(macd_h / 20.0),
      atr / denom,
  };
}

MlScore MlScorer::score(const repositories::AnalysisSnapshot& snap) const {
  MlScore out;
  out.model_version = version_;
  if (!loaded_ || !snap.has_price) {
    out.probability_up = 0.5;
    out.confidence = "NONE";
    return out;
  }

  auto feats = featuresFrom(snap);
  const size_t n = std::min(feats.size(), coefficients_.size());
  double z = intercept_;
  for (size_t i = 0; i < n; ++i) z += coefficients_[i] * feats[i];
  out.probability_up = sigmoid(z);
  out.ok = true;

  const double edge = std::fabs(out.probability_up - 0.5);
  if (edge >= 0.20)
    out.confidence = "HIGH";
  else if (edge >= 0.10)
    out.confidence = "MEDIUM";
  else
    out.confidence = "LOW";

  return out;
}

}  // namespace stocksense::services
