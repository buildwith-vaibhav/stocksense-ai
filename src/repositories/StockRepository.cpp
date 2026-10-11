#include "stocksense/repositories/StockRepository.hpp"

#include <spdlog/spdlog.h>

namespace stocksense::repositories {

StockRepository::StockRepository(db::Database& db) : db_(db) {}

std::vector<Stock> StockRepository::listAll() {
  std::vector<Stock> result;
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return result;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    auto rows = tx.exec(
        "SELECT id, symbol, exchange, name, COALESCE(sector, '') "
        "FROM stocks WHERE is_active = TRUE ORDER BY symbol");
    for (const auto& r : rows) {
      Stock s;
      s.id = r[0].as<int64_t>();
      s.symbol = r[1].as<std::string>();
      s.exchange = r[2].as<std::string>();
      s.name = r[3].as<std::string>();
      s.sector = r[4].as<std::string>();
      result.push_back(std::move(s));
    }
    tx.commit();
  } catch (const std::exception& e) {
    spdlog::error("listAll failed: {}", e.what());
  }
#endif
  return result;
}

std::optional<Stock> StockRepository::findBySymbol(const std::string& symbol,
                                                   const std::string& exchange) {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return std::nullopt;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    auto rows = tx.exec_params(
        "SELECT id, symbol, exchange, name, COALESCE(sector, '') "
        "FROM stocks WHERE symbol = $1 AND exchange = $2 AND is_active = TRUE",
        symbol, exchange);
    if (rows.empty()) return std::nullopt;
    const auto& r = rows[0];
    Stock s;
    s.id = r[0].as<int64_t>();
    s.symbol = r[1].as<std::string>();
    s.exchange = r[2].as<std::string>();
    s.name = r[3].as<std::string>();
    s.sector = r[4].as<std::string>();
    tx.commit();
    return s;
  } catch (const std::exception& e) {
    spdlog::error("findBySymbol failed: {}", e.what());
  }
#endif
  return std::nullopt;
}

std::optional<PriceBar> StockRepository::latestBar(int64_t stock_id) {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return std::nullopt;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    auto rows = tx.exec_params(
        "SELECT stock_id, ts::text, open, high, low, close, volume "
        "FROM price_bars WHERE stock_id = $1 "
        "ORDER BY ts DESC LIMIT 1",
        stock_id);
    if (rows.empty()) return std::nullopt;
    const auto& r = rows[0];
    PriceBar b;
    b.stock_id = r[0].as<int64_t>();
    b.ts = r[1].as<std::string>();
    b.open = r[2].as<double>();
    b.high = r[3].as<double>();
    b.low = r[4].as<double>();
    b.close = r[5].as<double>();
    b.volume = r[6].as<int64_t>();
    tx.commit();
    return b;
  } catch (const std::exception& e) {
    spdlog::error("latestBar failed: {}", e.what());
  }
#endif
  return std::nullopt;
}

std::vector<PriceBar> StockRepository::barsForSymbol(const std::string& symbol,
                                                     int limit) {
  std::vector<PriceBar> result;
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return result;
  try {
    auto stock = findBySymbol(symbol);
    if (!stock) return result;

    auto conn = db_.connect();
    pqxx::work tx(*conn);
    auto rows = tx.exec_params(
        "SELECT stock_id, ts::text, open, high, low, close, volume "
        "FROM price_bars WHERE stock_id = $1 "
        "ORDER BY ts DESC LIMIT $2",
        stock->id, limit);
    for (const auto& r : rows) {
      PriceBar b;
      b.stock_id = r[0].as<int64_t>();
      b.ts = r[1].as<std::string>();
      b.open = r[2].as<double>();
      b.high = r[3].as<double>();
      b.low = r[4].as<double>();
      b.close = r[5].as<double>();
      b.volume = r[6].as<int64_t>();
      result.push_back(std::move(b));
    }
    tx.commit();
  } catch (const std::exception& e) {
    spdlog::error("barsForSymbol failed: {}", e.what());
  }
#endif
  return result;
}

std::optional<AnalysisSnapshot> StockRepository::analysisForSymbol(
    const std::string& symbol) {
  auto stock = findBySymbol(symbol);
  if (!stock) return std::nullopt;

  AnalysisSnapshot snap;
  snap.symbol = stock->symbol;
  snap.exchange = stock->exchange;
  snap.data_freshness = "unavailable";

  auto bar = latestBar(stock->id);
  if (bar) {
    snap.price = bar->close;
    snap.data_timestamp = bar->ts;
    snap.data_freshness = "delayed_eod";
    snap.has_price = true;
  }

#ifdef STOCKSENSE_HAS_POSTGRES
  if (db_.isConnected()) {
    try {
      auto conn = db_.connect();
      pqxx::work tx(*conn);
      auto rows = tx.exec_params(
          "SELECT rsi_14, sma_20, sma_50, sma_200, macd, macd_signal, macd_hist, atr_14 "
          "FROM technical_indicators WHERE stock_id = $1 ORDER BY ts DESC LIMIT 1",
          stock->id);
      if (!rows.empty()) {
        const auto& r = rows[0];
        if (!r[0].is_null()) snap.rsi_14 = r[0].as<double>();
        if (!r[1].is_null()) snap.sma_20 = r[1].as<double>();
        if (!r[2].is_null()) snap.sma_50 = r[2].as<double>();
        if (!r[3].is_null()) snap.sma_200 = r[3].as<double>();
        if (!r[4].is_null()) snap.macd = r[4].as<double>();
        if (!r[5].is_null()) snap.macd_signal = r[5].as<double>();
        if (!r[6].is_null()) snap.macd_hist = r[6].as<double>();
        if (!r[7].is_null()) snap.atr_14 = r[7].as<double>();
        snap.has_indicators = true;

        // Rule-based signal
        int score = 0;
        if (snap.has_price && snap.sma_50 > 0 && snap.price > snap.sma_50) score++;
        if (snap.has_price && snap.sma_50 > 0 && snap.price < snap.sma_50) score--;
        if (snap.rsi_14 > 0 && snap.rsi_14 < 30) score++;
        if (snap.rsi_14 > 70) score--;
        if (snap.macd_hist > 0) score++;
        if (snap.macd_hist < 0) score--;

        if (score >= 2) snap.technical_signal = "POSITIVE";
        else if (score <= -2) snap.technical_signal = "NEGATIVE";
        else snap.technical_signal = "NEUTRAL";

        if (snap.rsi_14 > 75 || snap.rsi_14 < 25) snap.risk_level = "HIGH";
        else if (snap.rsi_14 > 65 || snap.rsi_14 < 35) snap.risk_level = "MODERATE";
        else snap.risk_level = "LOW";
      }
      tx.commit();
    } catch (const std::exception& e) {
      spdlog::error("analysis indicators failed: {}", e.what());
    }
  }
#endif

  return snap;
}

int StockRepository::upsertBars(const std::vector<PriceBar>& bars) {
  if (bars.empty()) return 0;
  int count = 0;
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return 0;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    for (const auto& b : bars) {
      tx.exec_params(
          "INSERT INTO price_bars (stock_id, ts, open, high, low, close, volume) "
          "VALUES ($1, $2::timestamptz, $3, $4, $5, $6, $7) "
          "ON CONFLICT (stock_id, ts) DO UPDATE SET "
          "  open = EXCLUDED.open, high = EXCLUDED.high, low = EXCLUDED.low, "
          "  close = EXCLUDED.close, volume = EXCLUDED.volume",
          b.stock_id, b.ts, b.open, b.high, b.low, b.close, b.volume);
      ++count;
    }
    tx.commit();
    spdlog::info("upsertBars: wrote {} bars", count);
  } catch (const std::exception& e) {
    spdlog::error("upsertBars failed: {}", e.what());
    return 0;
  }
#endif
  return count;
}

int64_t StockRepository::countBars(int64_t stock_id) {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return 0;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    pqxx::result rows;
    if (stock_id > 0) {
      rows = tx.exec_params(
          "SELECT COUNT(*) FROM price_bars WHERE stock_id = $1", stock_id);
    } else {
      rows = tx.exec("SELECT COUNT(*) FROM price_bars");
    }
    tx.commit();
    if (!rows.empty()) return rows[0][0].as<int64_t>();
  } catch (const std::exception& e) {
    spdlog::error("countBars failed: {}", e.what());
  }
#endif
  return 0;
}

int StockRepository::upsertIndicators(const std::vector<IndicatorRow>& rows) {
  if (rows.empty()) return 0;
  int count = 0;
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return 0;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    for (const auto& r : rows) {
      tx.exec_params(
          "INSERT INTO technical_indicators ("
          "  stock_id, ts, sma_20, sma_50, sma_200, ema_12, ema_26, "
          "  rsi_14, macd, macd_signal, macd_hist, atr_14, volume_sma_20) "
          "VALUES ($1, $2::timestamptz, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13) "
          "ON CONFLICT (stock_id, ts) DO UPDATE SET "
          "  sma_20 = EXCLUDED.sma_20, sma_50 = EXCLUDED.sma_50, "
          "  sma_200 = EXCLUDED.sma_200, ema_12 = EXCLUDED.ema_12, "
          "  ema_26 = EXCLUDED.ema_26, rsi_14 = EXCLUDED.rsi_14, "
          "  macd = EXCLUDED.macd, macd_signal = EXCLUDED.macd_signal, "
          "  macd_hist = EXCLUDED.macd_hist, atr_14 = EXCLUDED.atr_14, "
          "  volume_sma_20 = EXCLUDED.volume_sma_20",
          r.stock_id, r.ts, r.sma_20, r.sma_50, r.sma_200, r.ema_12, r.ema_26,
          r.rsi_14, r.macd, r.macd_signal, r.macd_hist, r.atr_14, r.volume_sma_20);
      ++count;
    }
    tx.commit();
    spdlog::info("upsertIndicators: wrote {} rows", count);
  } catch (const std::exception& e) {
    spdlog::error("upsertIndicators failed: {}", e.what());
    return 0;
  }
#endif
  return count;
}


std::vector<AlertRecord> StockRepository::listActiveAlerts() {
  std::vector<AlertRecord> result;
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return result;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    auto rows = tx.exec(
        "SELECT a.id, a.user_id, a.stock_id, s.symbol, a.alert_type, "
        "       COALESCE(a.threshold, 0), a.is_active, a.cooldown_minutes, "
        "       COALESCE(a.last_triggered_at::text, '') "
        "FROM alerts a JOIN stocks s ON s.id = a.stock_id "
        "WHERE a.is_active = TRUE "
        "  AND (a.last_triggered_at IS NULL "
        "       OR a.last_triggered_at < NOW() - make_interval(mins => a.cooldown_minutes)) "
        "ORDER BY a.id");
    for (const auto& r : rows) {
      AlertRecord a;
      a.id = r[0].as<int64_t>();
      a.user_id = r[1].as<int64_t>();
      a.stock_id = r[2].as<int64_t>();
      a.symbol = r[3].as<std::string>();
      a.alert_type = r[4].as<std::string>();
      a.threshold = r[5].as<double>();
      a.is_active = r[6].as<bool>();
      a.cooldown_minutes = r[7].as<int>();
      a.last_triggered_at = r[8].as<std::string>();
      result.push_back(std::move(a));
    }
    tx.commit();
  } catch (const std::exception& e) {
    spdlog::error("listActiveAlerts failed: {}", e.what());
  }
#endif
  return result;
}

std::vector<AlertRecord> StockRepository::listAlerts(int64_t user_id) {
  std::vector<AlertRecord> result;
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return result;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    pqxx::result rows;
    if (user_id > 0) {
      rows = tx.exec_params(
          "SELECT a.id, a.user_id, a.stock_id, s.symbol, a.alert_type, "
          "       COALESCE(a.threshold, 0), a.is_active, a.cooldown_minutes, "
          "       COALESCE(a.last_triggered_at::text, '') "
          "FROM alerts a JOIN stocks s ON s.id = a.stock_id "
          "WHERE a.user_id = $1 ORDER BY a.id",
          user_id);
    } else {
      rows = tx.exec(
          "SELECT a.id, a.user_id, a.stock_id, s.symbol, a.alert_type, "
          "       COALESCE(a.threshold, 0), a.is_active, a.cooldown_minutes, "
          "       COALESCE(a.last_triggered_at::text, '') "
          "FROM alerts a JOIN stocks s ON s.id = a.stock_id ORDER BY a.id");
    }
    for (const auto& r : rows) {
      AlertRecord a;
      a.id = r[0].as<int64_t>();
      a.user_id = r[1].as<int64_t>();
      a.stock_id = r[2].as<int64_t>();
      a.symbol = r[3].as<std::string>();
      a.alert_type = r[4].as<std::string>();
      a.threshold = r[5].as<double>();
      a.is_active = r[6].as<bool>();
      a.cooldown_minutes = r[7].as<int>();
      a.last_triggered_at = r[8].as<std::string>();
      result.push_back(std::move(a));
    }
    tx.commit();
  } catch (const std::exception& e) {
    spdlog::error("listAlerts failed: {}", e.what());
  }
#endif
  return result;
}

int64_t StockRepository::createAlert(int64_t user_id, int64_t stock_id,
                                     const std::string& alert_type,
                                     double threshold, int cooldown_minutes) {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return 0;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    auto rows = tx.exec_params(
        "INSERT INTO alerts (user_id, stock_id, alert_type, threshold, "
        "is_active, cooldown_minutes) "
        "VALUES ($1, $2, $3, $4, TRUE, $5) RETURNING id",
        user_id, stock_id, alert_type, threshold, cooldown_minutes);
    tx.commit();
    if (!rows.empty()) return rows[0][0].as<int64_t>();
  } catch (const std::exception& e) {
    spdlog::error("createAlert failed: {}", e.what());
  }
#endif
  return 0;
}

void StockRepository::markAlertTriggered(int64_t alert_id) {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    tx.exec_params(
        "UPDATE alerts SET last_triggered_at = NOW() WHERE id = $1", alert_id);
    tx.commit();
  } catch (const std::exception& e) {
    spdlog::error("markAlertTriggered failed: {}", e.what());
  }
#endif
}

void StockRepository::logNotification(int64_t alert_id,
                                      const std::string& channel,
                                      const std::string& status,
                                      const std::string& payload_json) {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    tx.exec_params(
        "INSERT INTO notification_logs (alert_id, channel, status, payload, sent_at) "
        "VALUES ($1, $2, $3, $4::jsonb, NOW())",
        alert_id, channel, status, payload_json);
    tx.commit();
  } catch (const std::exception& e) {
    spdlog::error("logNotification failed: {}", e.what());
  }
#endif
}

int64_t StockRepository::ensureDemoUser() {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return 0;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    auto rows = tx.exec(
        "SELECT id FROM users WHERE email = 'demo@stocksense.local' LIMIT 1");
    if (!rows.empty()) {
      int64_t id = rows[0][0].as<int64_t>();
      tx.commit();
      return id;
    }
    auto ins = tx.exec(
        "INSERT INTO users (email, password_hash, display_name, risk_profile) "
        "VALUES ('demo@stocksense.local', '$2b$12$placeholder', 'Demo User', "
        "'moderate') RETURNING id");
    tx.commit();
    if (!ins.empty()) return ins[0][0].as<int64_t>();
  } catch (const std::exception& e) {
    spdlog::error("ensureDemoUser failed: {}", e.what());
  }
#endif
  return 0;
}

int StockRepository::countAlerts() {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return 0;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    auto rows = tx.exec("SELECT COUNT(*) FROM alerts");
    tx.commit();
    if (!rows.empty()) return rows[0][0].as<int>();
  } catch (const std::exception& e) {
    spdlog::error("countAlerts failed: {}", e.what());
  }
#endif
  return 0;
}


int64_t StockRepository::ensureDefaultWatchlist(int64_t user_id) {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected() || user_id <= 0) return 0;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    auto rows = tx.exec_params(
        "SELECT id FROM watchlists WHERE user_id = $1 AND name = 'Default' LIMIT 1",
        user_id);
    if (!rows.empty()) {
      int64_t id = rows[0][0].as<int64_t>();
      tx.commit();
      return id;
    }
    auto ins = tx.exec_params(
        "INSERT INTO watchlists (user_id, name) VALUES ($1, 'Default') RETURNING id",
        user_id);
    tx.commit();
    if (!ins.empty()) return ins[0][0].as<int64_t>();
  } catch (const std::exception& e) {
    spdlog::error("ensureDefaultWatchlist failed: {}", e.what());
  }
#endif
  return 0;
}

std::vector<WatchlistInfo> StockRepository::listWatchlists(int64_t user_id) {
  std::vector<WatchlistInfo> result;
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return result;
  try {
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    pqxx::result wrows;
    if (user_id > 0) {
      wrows = tx.exec_params(
          "SELECT id, name FROM watchlists WHERE user_id = $1 ORDER BY id", user_id);
    } else {
      wrows = tx.exec("SELECT id, name FROM watchlists ORDER BY id");
    }
    for (const auto& wr : wrows) {
      WatchlistInfo w;
      w.id = wr[0].as<int64_t>();
      w.name = wr[1].as<std::string>();
      auto srows = tx.exec_params(
          "SELECT s.symbol FROM watchlist_stocks ws "
          "JOIN stocks s ON s.id = ws.stock_id "
          "WHERE ws.watchlist_id = $1 ORDER BY s.symbol",
          w.id);
      for (const auto& sr : srows) {
        w.symbols.push_back(sr[0].as<std::string>());
      }
      result.push_back(std::move(w));
    }
    tx.commit();
  } catch (const std::exception& e) {
    spdlog::error("listWatchlists failed: {}", e.what());
  }
#endif
  return result;
}

bool StockRepository::addToWatchlist(int64_t watchlist_id, const std::string& symbol) {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return false;
  try {
    auto stock = findBySymbol(symbol);
    if (!stock) return false;
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    tx.exec_params(
        "INSERT INTO watchlist_stocks (watchlist_id, stock_id) VALUES ($1, $2) "
        "ON CONFLICT DO NOTHING",
        watchlist_id, stock->id);
    tx.commit();
    return true;
  } catch (const std::exception& e) {
    spdlog::error("addToWatchlist failed: {}", e.what());
  }
#endif
  return false;
}

bool StockRepository::removeFromWatchlist(int64_t watchlist_id,
                                          const std::string& symbol) {
#ifdef STOCKSENSE_HAS_POSTGRES
  if (!db_.isConnected()) return false;
  try {
    auto stock = findBySymbol(symbol);
    if (!stock) return false;
    auto conn = db_.connect();
    pqxx::work tx(*conn);
    tx.exec_params(
        "DELETE FROM watchlist_stocks WHERE watchlist_id = $1 AND stock_id = $2",
        watchlist_id, stock->id);
    tx.commit();
    return true;
  } catch (const std::exception& e) {
    spdlog::error("removeFromWatchlist failed: {}", e.what());
  }
#endif
  return false;
}

std::vector<AnalysisSnapshot> StockRepository::dashboardRows() {
  std::vector<AnalysisSnapshot> rows;
  for (const auto& s : listAll()) {
    auto snap = analysisForSymbol(s.symbol);
    if (snap) rows.push_back(*snap);
  }
  return rows;
}

}  // namespace stocksense::repositories

