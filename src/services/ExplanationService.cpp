#include "stocksense/services/ExplanationService.hpp"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace stocksense::services {

ExplanationService::ExplanationService() {
  if (const char* u = std::getenv("STOCKSENSE_LLM_URL")) llmUrl_ = u;
  if (const char* k = std::getenv("STOCKSENSE_LLM_API_KEY")) llmKey_ = k;
  if (const char* m = std::getenv("STOCKSENSE_LLM_MODEL")) llmModel_ = m;
  if (llmModel_.empty()) llmModel_ = "gpt-4o-mini";
  if (llmUrl_.empty()) {
    llmUrl_ = "https://api.openai.com/v1/chat/completions";
  }
  llmConfigured_ = !llmKey_.empty();
  if (llmConfigured_) {
    spdlog::info("LLM explanations enabled (model={})", llmModel_);
  } else {
    spdlog::info("LLM not configured — using rule-based explanations only");
  }
}

ExplanationResult ExplanationService::fromRules(
    const repositories::AnalysisSnapshot& snap, const MlScore& ml) {
  ExplanationResult r;
  r.source = "rules";
  r.ok = true;

  std::ostringstream sum;
  std::ostringstream det;

  sum << snap.symbol << " is showing a " << snap.technical_signal
      << " technical bias";
  if (ml.ok) {
    sum << " with model P(up)=" << static_cast<int>(ml.probability_up * 100)
        << "% (" << ml.confidence << " confidence)";
  }
  sum << ". Risk level: " << snap.risk_level << ".";

  det << "Price context: ";
  if (snap.has_price) {
    det << "last ~" << snap.price;
  } else {
    det << "unavailable";
  }
  if (snap.has_indicators) {
    det << ". RSI(14)=" << snap.rsi_14;
    if (snap.rsi_14 >= 70)
      det << " (overbought zone)";
    else if (snap.rsi_14 <= 30)
      det << " (oversold zone)";
    else
      det << " (neutral band)";
    det << "; SMA20=" << snap.sma_20 << ", SMA50=" << snap.sma_50;
    if (snap.price > snap.sma_50 && snap.sma_50 > 0)
      det << " (price above SMA50)";
    else if (snap.sma_50 > 0)
      det << " (price below SMA50)";
    det << "; MACD hist=" << snap.macd_hist;
    if (snap.macd_hist > 0)
      det << " (positive momentum)";
    else if (snap.macd_hist < 0)
      det << " (negative momentum)";
  } else {
    det << ". Indicators not computed yet — run /api/admin/indicators/compute.";
  }
  det << " This is automated commentary, not investment advice.";

  r.summary = sum.str();
  r.detail = det.str();
  return r;
}

ExplanationResult ExplanationService::tryLlm(
    const repositories::AnalysisSnapshot& snap, const MlScore& ml,
    const ExplanationResult& rules) const {
  ExplanationResult out = rules;
  if (!llmConfigured_) return out;

  std::ostringstream prompt;
  prompt << "You are a cautious market assistant. In 2-3 short sentences, "
            "explain this stock snapshot for a retail user. "
            "Do not give buy/sell orders. Not investment advice.\n"
         << "Symbol: " << snap.symbol
         << " Signal: " << snap.technical_signal
         << " Risk: " << snap.risk_level
         << " Price: " << snap.price
         << " RSI: " << snap.rsi_14
         << " SMA50: " << snap.sma_50
         << " MACD_hist: " << snap.macd_hist
         << " P_up: " << (ml.ok ? ml.probability_up : -1.0)
         << "\nRule draft: " << rules.summary;

  // Minimal JSON body (escape quotes in prompt lightly)
  std::string p = prompt.str();
  for (char& c : p) {
    if (c == '"' || c == '\\') c = ' ';
    if (c == '\n') c = ' ';
  }

  std::ostringstream body;
  body << "{\"model\":\"" << llmModel_
       << "\",\"messages\":[{\"role\":\"user\",\"content\":\"" << p
       << "\"}],\"max_tokens\":120}";

  std::string tmpIn = "stocksense_llm_req.json";
  std::string tmpOut = "stocksense_llm_resp.json";
  {
    FILE* f = std::fopen(tmpIn.c_str(), "wb");
    if (!f) return out;
    const std::string b = body.str();
    std::fwrite(b.data(), 1, b.size(), f);
    std::fclose(f);
  }

  std::ostringstream cmd;
#ifdef _WIN32
  cmd << "curl.exe -s -X POST \"" << llmUrl_ << "\" "
      << "-H \"Content-Type: application/json\" "
      << "-H \"Authorization: Bearer " << llmKey_ << "\" "
      << "--data-binary \"@" << tmpIn << "\" -o \"" << tmpOut << "\"";
  FILE* pipe = _popen(cmd.str().c_str(), "r");
  if (pipe) _pclose(pipe);
#else
  cmd << "curl -s -X POST '" << llmUrl_ << "' "
      << "-H 'Content-Type: application/json' "
      << "-H 'Authorization: Bearer " << llmKey_ << "' "
      << "--data-binary @" << tmpIn << " -o " << tmpOut;
  FILE* pipe = popen(cmd.str().c_str(), "r");
  if (pipe) pclose(pipe);
#endif

  FILE* rf = std::fopen(tmpOut.c_str(), "rb");
  if (!rf) return out;
  std::string resp;
  char buf[512];
  while (std::fgets(buf, sizeof(buf), rf)) resp += buf;
  std::fclose(rf);

  // Very small extract of "content":"..."
  auto key = resp.find("\"content\"");
  if (key == std::string::npos) {
    spdlog::warn("LLM response missing content");
    return out;
  }
  auto colon = resp.find(':', key);
  auto q1 = resp.find('"', colon + 1);
  if (q1 == std::string::npos) return out;
  std::string content;
  for (size_t i = q1 + 1; i < resp.size(); ++i) {
    if (resp[i] == '\\' && i + 1 < resp.size()) {
      content.push_back(resp[i + 1]);
      ++i;
      continue;
    }
    if (resp[i] == '"') break;
    content.push_back(resp[i]);
  }
  if (content.size() < 8) return out;

  out.summary = content;
  out.detail = rules.detail + " LLM note applied.";
  out.source = "rules+llm";
  out.ok = true;
  return out;
}

ExplanationResult ExplanationService::explain(
    const repositories::AnalysisSnapshot& snap, const MlScore& ml) const {
  auto rules = fromRules(snap, ml);
  if (!llmConfigured_) return rules;
  return tryLlm(snap, ml, rules);
}

}  // namespace stocksense::services
