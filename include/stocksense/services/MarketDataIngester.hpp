#pragma once

#include "stocksense/repositories/StockRepository.hpp"

#include <memory>
#include <string>
#include <vector>

namespace stocksense::services {

struct IngestResult {
  int bars_written = 0;
  int stocks_touched = 0;
  std::string message;
  bool ok = false;
};

/**
 * Stage 3 — Market data ingestion.
 *
 * Two offline-friendly sources (no external API key required):
 *   1. Synthetic random-walk EOD bars for all active stocks
 *   2. CSV file: symbol,date,open,high,low,close,volume
 *
 * Live vendor APIs (Yahoo, NSE, Alpha Vantage, …) can be plugged in later
 * behind the same interface without changing the repository layer.
 */
class MarketDataIngester {
public:
  explicit MarketDataIngester(
      std::shared_ptr<repositories::StockRepository> repo);

  // Generate `days` of synthetic EOD bars ending "today" (IST close 15:30)
  IngestResult ingestSynthetic(int days = 30);

  // Load bars from a CSV file. Header optional.
  // Expected columns: symbol,date,open,high,low,close,volume
  // date formats accepted: YYYY-MM-DD or YYYY-MM-DD HH:MM:SS
  IngestResult ingestFromCsv(const std::string& path);

private:
  std::shared_ptr<repositories::StockRepository> repo_;

  static double nextPrice(double last, double volatility);
  static int64_t randomVolume(int64_t base);
};

}  // namespace stocksense::services
