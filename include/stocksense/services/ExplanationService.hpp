#pragma once

#include "stocksense/repositories/StockRepository.hpp"
#include "stocksense/services/MlScorer.hpp"

#include <string>

namespace stocksense::services {

struct ExplanationResult {
  std::string summary;
  std::string detail;
  std::string source;  // "rules" | "llm" | "rules+llm"
  bool ok = false;
};

class ExplanationService {
public:
  ExplanationService();

  // Always builds a rule-based explanation; optionally augments via LLM HTTP API
  ExplanationResult explain(const repositories::AnalysisSnapshot& snap,
                            const MlScore& ml) const;

private:
  bool llmConfigured_ = false;
  std::string llmUrl_;
  std::string llmKey_;
  std::string llmModel_;

  static ExplanationResult fromRules(const repositories::AnalysisSnapshot& snap,
                                     const MlScore& ml);
  ExplanationResult tryLlm(const repositories::AnalysisSnapshot& snap,
                           const MlScore& ml,
                           const ExplanationResult& rules) const;
};

}  // namespace stocksense::services
