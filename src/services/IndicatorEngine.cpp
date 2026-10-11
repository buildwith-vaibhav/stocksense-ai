#include "stocksense/services/IndicatorEngine.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace stocksense::services {

IndicatorEngine::IndicatorEngine(
    std::shared_ptr<repositories::StockRepository> repo)
    : repo_(std::move(repo)) {}

double IndicatorEngine::sma(const std::vector<double>& closes, size_t endIdx,
                            int period) {
  if (endIdx + 1 < static_cast<size_t>(period)) return 0.0;
  double sum = 0.0;
  for (int i = 0; i < period; ++i) {
    sum += closes[endIdx - static_cast<size_t>(i)];
  }
  return sum / period;
}

double IndicatorEngine::rsi(const std::vector<double>& closes, size_t endIdx,
                            int period) {
  if (endIdx < static_cast<size_t>(period)) return 50.0;
  double gain = 0.0, loss = 0.0;
  for (int i = static_cast<int>(endIdx) - period + 1; i <= static_cast<int>(endIdx);
       ++i) {
    double diff = closes[static_cast<size_t>(i)] - closes[static_cast<size_t>(i) - 1];
    if (diff >= 0)
      gain += diff;
    else
      loss -= diff;
  }
  gain /= period;
  loss /= period;
  if (loss < 1e-12) return 100.0;
  double rs = gain / loss;
  return 100.0 - (100.0 / (1.0 + rs));
}

double IndicatorEngine::emaAt(const std::vector<double>& closes, size_t endIdx,
                              int period, double /*prevEma*/, bool /*hasPrev*/) {
  // Simple one-shot EMA from the window ending at endIdx
  if (endIdx + 1 < static_cast<size_t>(period)) return closes[endIdx];
  double k = 2.0 / (period + 1.0);
  // Seed with SMA of first `period` samples in the window
  size_t start = endIdx + 1 - static_cast<size_t>(period);
  double ema = 0.0;
  for (size_t i = start; i <= endIdx; ++i) {
    if (i == start)
      ema = closes[i];
    else
      ema = closes[i] * k + ema * (1.0 - k);
  }
  // Walk from beginning of series for better quality when enough history
  if (endIdx + 1 >= static_cast<size_t>(period) * 2) {
    ema = closes[0];
    for (size_t i = 1; i <= endIdx; ++i) {
      ema = closes[i] * k + ema * (1.0 - k);
    }
  }
  return ema;
}

void IndicatorEngine::macd(const std::vector<double>& closes, size_t endIdx,
                           double& macdLine, double& signal, double& hist) {
  if (endIdx < 26) {
    macdLine = signal = hist = 0.0;
    return;
  }
  double ema12 = emaAt(closes, endIdx, 12, 0, false);
  double ema26 = emaAt(closes, endIdx, 26, 0, false);
  macdLine = ema12 - ema26;

  // Build MACD series for signal (9-period EMA of MACD)
  std::vector<double> macdSeries;
  macdSeries.reserve(endIdx + 1);
  for (size_t i = 0; i <= endIdx; ++i) {
    if (i < 26) {
      macdSeries.push_back(0.0);
    } else {
      double e12 = emaAt(closes, i, 12, 0, false);
      double e26 = emaAt(closes, i, 26, 0, false);
      macdSeries.push_back(e12 - e26);
    }
  }
  signal = emaAt(macdSeries, endIdx, 9, 0, false);
  hist = macdLine - signal;
}

double IndicatorEngine::atr(const std::vector<repositories::PriceBar>& bars,
                            size_t endIdx, int period) {
  if (endIdx < static_cast<size_t>(period) || endIdx == 0) return 0.0;
  double sum = 0.0;
  for (int i = 0; i < period; ++i) {
    size_t idx = endIdx - static_cast<size_t>(i);
    double tr = bars[idx].high - bars[idx].low;
    if (idx > 0) {
      tr = std::max(tr, std::abs(bars[idx].high - bars[idx - 1].close));
      tr = std::max(tr, std::abs(bars[idx].low - bars[idx - 1].close));
    }
    sum += tr;
  }
  return sum / period;
}

IndicatorComputeResult IndicatorEngine::computeAll(
    const std::string& symbolFilter) {
  IndicatorComputeResult result;

  auto stocks = repo_->listAll();
  if (stocks.empty()) {
    result.message = "No active stocks. Seed Stage 2 data first.";
    return result;
  }

  std::vector<repositories::IndicatorRow> rows;

  for (const auto& stock : stocks) {
    if (!symbolFilter.empty() && stock.symbol != symbolFilter) continue;

    // Need enough history for SMA200 / MACD
    auto bars = repo_->barsForSymbol(stock.symbol, 260);
    if (bars.size() < 15) {
      spdlog::warn("Skip {}: only {} bars", stock.symbol, bars.size());
      continue;
    }

    // barsForSymbol returns newest-first; reverse to oldest-first
    std::reverse(bars.begin(), bars.end());

    std::vector<double> closes;
    closes.reserve(bars.size());
    for (const auto& b : bars) closes.push_back(b.close);

    const size_t end = closes.size() - 1;

    repositories::IndicatorRow row;
    row.stock_id = stock.id;
    row.ts = bars[end].ts;
    row.sma_20 = sma(closes, end, 20);
    row.sma_50 = sma(closes, end, 50);
    row.sma_200 = sma(closes, end, 200);
    row.rsi_14 = rsi(closes, end, 14);
    row.atr_14 = atr(bars, end, 14);
    row.volume_sma_20 = 0.0;
    if (bars.size() >= 20) {
      double vsum = 0.0;
      for (size_t i = end; i > end - 20; --i) vsum += static_cast<double>(bars[i].volume);
      row.volume_sma_20 = vsum / 20.0;
    }
    macd(closes, end, row.macd, row.macd_signal, row.macd_hist);
    row.ema_12 = emaAt(closes, end, 12, 0, false);
    row.ema_26 = emaAt(closes, end, 26, 0, false);

    rows.push_back(row);
    ++result.stocks_touched;
  }

  result.rows_written = repo_->upsertIndicators(rows);
  result.ok = result.rows_written > 0;
  result.message = result.ok ? "Indicators computed and stored"
                             : "No indicator rows written";
  return result;
}

}  // namespace stocksense::services
