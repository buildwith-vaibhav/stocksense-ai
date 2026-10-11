#pragma once

#include "stocksense/repositories/StockRepository.hpp"

#include <memory>
#include <string>
#include <vector>

namespace stocksense::services {

struct IndicatorComputeResult {
  bool ok = false;
  int stocks_touched = 0;
  int rows_written = 0;
  std::string message;
};

class IndicatorEngine {
public:
  explicit IndicatorEngine(std::shared_ptr<repositories::StockRepository> repo);

  // Recompute indicators for all active stocks (or one symbol if non-empty)
  IndicatorComputeResult computeAll(const std::string& symbolFilter = "");

private:
  std::shared_ptr<repositories::StockRepository> repo_;

  static double sma(const std::vector<double>& closes, size_t endIdx, int period);
  static double rsi(const std::vector<double>& closes, size_t endIdx, int period);
  static void macd(const std::vector<double>& closes, size_t endIdx,
                   double& macdLine, double& signal, double& hist);
  static double atr(const std::vector<repositories::PriceBar>& bars,
                    size_t endIdx, int period);
  static double emaAt(const std::vector<double>& closes, size_t endIdx,
                      int period, double prevEma, bool hasPrev);
};

}  // namespace stocksense::services
