#include "stocksense/services/MarketDataIngester.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace stocksense::services {

namespace {

// Simple seeded RNG so runs are reproducible for demos
std::mt19937& rng() {
  static std::mt19937 gen(42);
  return gen;
}

// Format a date as "YYYY-MM-DD 15:30:00+05:30" (NSE EOD close)
std::string formatEodTs(std::chrono::sys_days day) {
  const auto ymd = std::chrono::year_month_day{day};
  std::ostringstream oss;
  oss << static_cast<int>(ymd.year()) << '-'
      << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ymd.month()) << '-'
      << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ymd.day())
      << " 15:30:00+05:30";
  return oss.str();
}

std::vector<std::string> splitCsvLine(const std::string& line) {
  std::vector<std::string> fields;
  std::string field;
  std::istringstream ss(line);
  while (std::getline(ss, field, ',')) {
    // trim
    while (!field.empty() && (field.front() == ' ' || field.front() == '\t'))
      field.erase(field.begin());
    while (!field.empty() && (field.back() == ' ' || field.back() == '\t' ||
                              field.back() == '\r'))
      field.pop_back();
    fields.push_back(field);
  }
  return fields;
}

}  // namespace

MarketDataIngester::MarketDataIngester(
    std::shared_ptr<repositories::StockRepository> repo)
    : repo_(std::move(repo)) {}

double MarketDataIngester::nextPrice(double last, double volatility) {
  std::normal_distribution<double> dist(0.0, volatility);
  double ret = dist(rng());
  double next = last * (1.0 + ret);
  return next > 1.0 ? next : 1.0;
}

int64_t MarketDataIngester::randomVolume(int64_t base) {
  std::uniform_real_distribution<double> dist(0.6, 1.6);
  return static_cast<int64_t>(base * dist(rng()));
}

IngestResult MarketDataIngester::ingestSynthetic(int days) {
  IngestResult result;
  if (days < 1) days = 1;
  if (days > 365) days = 365;

  auto stocks = repo_->listAll();
  if (stocks.empty()) {
    result.message = "No active stocks in database. Run Stage 2 seed first.";
    return result;
  }

  // Starting prices roughly in the ballpark of real NSE levels (demo only)
  std::map<std::string, double> startPrice = {
      {"TCS", 3500.0},     {"RELIANCE", 2950.0}, {"INFY", 1850.0},
      {"HDFCBANK", 1650.0},{"ICICIBANK", 1250.0},{"SBIN", 820.0},
      {"BHARTIARTL", 1550.0},{"ITC", 470.0},     {"LT", 3600.0},
      {"WIPRO", 520.0}};
  std::map<std::string, int64_t> startVol = {
      {"TCS", 2000000}, {"RELIANCE", 5000000}, {"INFY", 4500000},
      {"HDFCBANK", 6000000}, {"ICICIBANK", 8000000}, {"SBIN", 12000000},
      {"BHARTIARTL", 4000000}, {"ITC", 10000000}, {"LT", 1500000},
      {"WIPRO", 3500000}};

  // Fully qualify chrono to avoid clashes with std::floor / chrono::last on MSVC
  const auto today = std::chrono::floor<std::chrono::days>(
      std::chrono::system_clock::now());

  std::vector<repositories::PriceBar> allBars;
  allBars.reserve(static_cast<size_t>(days) * stocks.size());

  for (const auto& stock : stocks) {
    double price = startPrice.count(stock.symbol) ? startPrice[stock.symbol] : 1000.0;
    int64_t baseVol = startVol.count(stock.symbol) ? startVol[stock.symbol] : 1000000;
    double vol = 0.012;  // ~1.2% daily vol

    // If we already have a latest bar, continue from it
    if (auto existing = repo_->latestBar(stock.id)) {
      price = existing->close;
    }

    for (int i = days - 1; i >= 0; --i) {
      const auto tradeDay = today - std::chrono::days{i};
      // Skip weekends
      const std::chrono::weekday wd{tradeDay};
      if (wd == std::chrono::Saturday || wd == std::chrono::Sunday) continue;

      double open = price;
      double close = nextPrice(open, vol);
      double high = std::max(open, close) * (1.0 + std::abs(nextPrice(0.002, 0.003) - 0.002));
      double low  = std::min(open, close) * (1.0 - std::abs(nextPrice(0.002, 0.003) - 0.002));
      if (low < 1.0) low = 1.0;
      if (high < low) std::swap(high, low);

      repositories::PriceBar bar;
      bar.stock_id = stock.id;
      bar.ts = formatEodTs(tradeDay);
      bar.open = std::round(open * 100.0) / 100.0;
      bar.high = std::round(high * 100.0) / 100.0;
      bar.low = std::round(low * 100.0) / 100.0;
      bar.close = std::round(close * 100.0) / 100.0;
      bar.volume = randomVolume(baseVol);

      allBars.push_back(bar);
      price = close;
    }
    ++result.stocks_touched;
  }

  result.bars_written = repo_->upsertBars(allBars);
  result.ok = result.bars_written > 0;
  result.message = result.ok
      ? "Synthetic EOD bars ingested successfully"
      : "No bars written (check database connection)";
  return result;
}

IngestResult MarketDataIngester::ingestFromCsv(const std::string& path) {
  IngestResult result;
  std::ifstream in(path);
  if (!in) {
    result.message = "Cannot open CSV file: " + path;
    return result;
  }

  auto stocks = repo_->listAll();
  std::map<std::string, int64_t> symbolToId;
  for (const auto& s : stocks) {
    symbolToId[s.symbol] = s.id;
  }

  std::vector<repositories::PriceBar> bars;
  std::string line;
  bool first = true;
  int lineNo = 0;

  while (std::getline(in, line)) {
    ++lineNo;
    if (line.empty() || line[0] == '#') continue;

    auto fields = splitCsvLine(line);
    if (fields.size() < 7) {
      spdlog::warn("CSV line {}: expected 7 columns, got {}", lineNo, fields.size());
      continue;
    }

    // Skip header
    if (first) {
      first = false;
      std::string lower = fields[0];
      for (auto& c : lower) c = static_cast<char>(std::tolower(c));
      if (lower == "symbol" || lower == "ticker") continue;
    }

    const std::string& symbol = fields[0];
    auto it = symbolToId.find(symbol);
    if (it == symbolToId.end()) {
      spdlog::warn("CSV line {}: unknown symbol {}", lineNo, symbol);
      continue;
    }

    repositories::PriceBar bar;
    bar.stock_id = it->second;
    // Accept YYYY-MM-DD or full timestamp; force EOD close if date only
    if (fields[1].size() == 10) {
      bar.ts = fields[1] + " 15:30:00+05:30";
    } else {
      bar.ts = fields[1];
    }
    try {
      bar.open = std::stod(fields[2]);
      bar.high = std::stod(fields[3]);
      bar.low = std::stod(fields[4]);
      bar.close = std::stod(fields[5]);
      bar.volume = std::stoll(fields[6]);
    } catch (...) {
      spdlog::warn("CSV line {}: parse error", lineNo);
      continue;
    }
    bars.push_back(bar);
  }

  result.bars_written = repo_->upsertBars(bars);
  result.stocks_touched = static_cast<int>(symbolToId.size());
  result.ok = result.bars_written > 0;
  result.message = result.ok
      ? "CSV bars ingested successfully"
      : "No valid bars found in CSV";
  return result;
}

}  // namespace stocksense::services
