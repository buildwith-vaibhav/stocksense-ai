#pragma once

#include "stocksense/db/Database.hpp"

#include <optional>
#include <string>
#include <vector>

namespace stocksense::repositories {

struct Stock {
  int64_t id = 0;
  std::string symbol;
  std::string exchange;
  std::string name;
  std::string sector;
};

struct PriceBar {
  int64_t stock_id = 0;
  std::string ts;
  double open = 0;
  double high = 0;
  double low = 0;
  double close = 0;
  int64_t volume = 0;
};

struct IndicatorRow {
  int64_t stock_id = 0;
  std::string ts;
  double sma_20 = 0;
  double sma_50 = 0;
  double sma_200 = 0;
  double ema_12 = 0;
  double ema_26 = 0;
  double rsi_14 = 0;
  double macd = 0;
  double macd_signal = 0;
  double macd_hist = 0;
  double atr_14 = 0;
  double volume_sma_20 = 0;
};

struct AnalysisSnapshot {
  std::string symbol;
  std::string exchange;
  double price = 0;
  std::string data_timestamp;
  std::string data_freshness;
  bool has_price = false;
  bool has_indicators = false;

  double rsi_14 = 0;
  double sma_20 = 0;
  double sma_50 = 0;
  double sma_200 = 0;
  double macd = 0;
  double macd_signal = 0;
  double macd_hist = 0;
  double atr_14 = 0;

  std::string technical_signal = "NONE";
  std::string risk_level = "UNKNOWN";
};

struct WatchlistInfo {
  int64_t id = 0;
  std::string name;
  std::vector<std::string> symbols;
};

struct AlertRecord {
  int64_t id = 0;
  int64_t user_id = 0;
  int64_t stock_id = 0;
  std::string symbol;
  std::string alert_type;
  double threshold = 0;
  bool is_active = true;
  int cooldown_minutes = 60;
  std::string last_triggered_at;  // empty if null
};

class StockRepository {
public:
  explicit StockRepository(db::Database& db);

  std::vector<Stock> listAll();
  std::optional<Stock> findBySymbol(const std::string& symbol,
                                    const std::string& exchange = "NSE");
  std::optional<PriceBar> latestBar(int64_t stock_id);
  std::vector<PriceBar> barsForSymbol(const std::string& symbol, int limit = 30);
  std::optional<AnalysisSnapshot> analysisForSymbol(const std::string& symbol);

  int upsertBars(const std::vector<PriceBar>& bars);
  int64_t countBars(int64_t stock_id = 0);
  int upsertIndicators(const std::vector<IndicatorRow>& rows);

  // Stage 5 — alerts
  std::vector<AlertRecord> listActiveAlerts();
  std::vector<AlertRecord> listAlerts(int64_t user_id = 0);
  int64_t createAlert(int64_t user_id, int64_t stock_id,
                      const std::string& alert_type, double threshold,
                      int cooldown_minutes = 60);
  void markAlertTriggered(int64_t alert_id);
  void logNotification(int64_t alert_id, const std::string& channel,
                       const std::string& status, const std::string& payload_json);
  int64_t ensureDemoUser();
  int countAlerts();

  // Stage 6 — watchlists
  int64_t ensureDefaultWatchlist(int64_t user_id);
  std::vector<WatchlistInfo> listWatchlists(int64_t user_id = 0);
  bool addToWatchlist(int64_t watchlist_id, const std::string& symbol);
  bool removeFromWatchlist(int64_t watchlist_id, const std::string& symbol);
  std::vector<AnalysisSnapshot> dashboardRows();

private:
  db::Database& db_;
};

}  // namespace stocksense::repositories
