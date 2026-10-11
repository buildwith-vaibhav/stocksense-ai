#pragma once

#include "stocksense/repositories/StockRepository.hpp"

#include <optional>
#include <string>
#include <vector>

namespace stocksense::services {

struct MlScore {
  double probability_up = 0.5;
  std::string confidence = "NONE";  // LOW / MEDIUM / HIGH
  std::string model_version = "none";
  bool ok = false;
};

class MlScorer {
public:
  // Loads models/stocksense_lr.json (path from STOCKSENSE_MODEL_PATH or default)
  explicit MlScorer(std::string modelPath = "");

  bool isLoaded() const { return loaded_; }
  const std::string& modelVersion() const { return version_; }

  bool reload();
  MlScore score(const repositories::AnalysisSnapshot& snap) const;

  // Feature vector (same order as trainer)
  static std::vector<double> featuresFrom(const repositories::AnalysisSnapshot& snap);

private:
  bool loaded_ = false;
  std::string path_;
  std::string version_;
  std::vector<std::string> feature_names_;
  std::vector<double> coefficients_;
  double intercept_ = 0.0;

  static double sigmoid(double z);
};

}  // namespace stocksense::services
