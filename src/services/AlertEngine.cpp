#include "stocksense/services/AlertEngine.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <sstream>

namespace stocksense::services {

AlertEngine::AlertEngine(std::shared_ptr<repositories::StockRepository> repo,
                         TelegramNotifier notifier)
    : repo_(std::move(repo)), telegram_(std::move(notifier)) {}

bool AlertEngine::evaluateOne(const repositories::AlertRecord& a,
                              const repositories::AnalysisSnapshot& snap,
                              std::string& reason) const {
  if (!snap.has_price && a.alert_type.find("price") != std::string::npos) {
    reason = "no_price";
    return false;
  }
  if (!snap.has_indicators &&
      (a.alert_type.find("rsi") != std::string::npos ||
       a.alert_type.find("ma") != std::string::npos ||
       a.alert_type.find("macd") != std::string::npos)) {
    reason = "no_indicators";
    return false;
  }

  if (a.alert_type == "price_above") {
    if (snap.price >= a.threshold) {
      reason = "price " + std::to_string(snap.price) + " >= " +
               std::to_string(a.threshold);
      return true;
    }
  } else if (a.alert_type == "price_below") {
    if (snap.price <= a.threshold) {
      reason = "price " + std::to_string(snap.price) + " <= " +
               std::to_string(a.threshold);
      return true;
    }
  } else if (a.alert_type == "rsi_overbought") {
    double th = a.threshold > 0 ? a.threshold : 70.0;
    if (snap.rsi_14 >= th) {
      reason = "RSI " + std::to_string(snap.rsi_14) + " >= " + std::to_string(th);
      return true;
    }
  } else if (a.alert_type == "rsi_oversold") {
    double th = a.threshold > 0 ? a.threshold : 30.0;
    if (snap.rsi_14 <= th) {
      reason = "RSI " + std::to_string(snap.rsi_14) + " <= " + std::to_string(th);
      return true;
    }
  } else if (a.alert_type == "ma_crossover") {
    // Price above SMA50 treated as bullish crossover proxy
    if (snap.sma_50 > 0 && snap.price > snap.sma_50) {
      reason = "price above SMA50";
      return true;
    }
  }

  reason = "condition_not_met";
  return false;
}

int AlertEngine::ensureDemoAlerts() {
  if (repo_->countAlerts() > 0) return 0;

  int64_t userId = repo_->ensureDemoUser();
  if (userId == 0) return 0;

  int created = 0;
  auto tcs = repo_->findBySymbol("TCS");
  auto rel = repo_->findBySymbol("RELIANCE");
  auto infy = repo_->findBySymbol("INFY");

  // Low thresholds so synthetic data can trigger for demo
  if (tcs) {
    if (repo_->createAlert(userId, tcs->id, "rsi_overbought", 60.0, 30)) ++created;
    if (repo_->createAlert(userId, tcs->id, "rsi_oversold", 40.0, 30)) ++created;
    if (repo_->createAlert(userId, tcs->id, "price_above", 1000.0, 30)) ++created;
  }
  if (rel) {
    if (repo_->createAlert(userId, rel->id, "ma_crossover", 0.0, 60)) ++created;
  }
  if (infy) {
    if (repo_->createAlert(userId, infy->id, "price_below", 50000.0, 60)) ++created;
  }
  spdlog::info("Created {} demo alerts", created);
  return created;
}

AlertEvalResult AlertEngine::evaluateAll() {
  AlertEvalResult result;
  auto alerts = repo_->listActiveAlerts();
  if (alerts.empty()) {
    result.message = "No active alerts. Call /api/admin/alerts/seed first.";
    result.ok = true;
    return result;
  }

  for (const auto& a : alerts) {
    ++result.checked;

    // Cooldown is enforced in listActiveAlerts SQL (last_triggered + interval).

    auto snap = repo_->analysisForSymbol(a.symbol);
    if (!snap) {
      result.details.push_back(a.symbol + " stock_not_found");
      continue;
    }

    std::string reason;
    if (!evaluateOne(a, *snap, reason)) {
      result.details.push_back(a.symbol + " " + a.alert_type + " " + reason);
      continue;
    }

    ++result.triggered;
    repo_->markAlertTriggered(a.id);

    std::ostringstream msg;
    msg << "StockSense Alert\n"
        << "Symbol: " << a.symbol << "\n"
        << "Type: " << a.alert_type << "\n"
        << "Reason: " << reason << "\n"
        << "Price: " << snap->price << "\n"
        << "Signal: " << snap->technical_signal << "\n"
        << "Risk: " << snap->risk_level << "\n"
        << "(Not investment advice)";

    std::ostringstream payload;
    payload << "{\"symbol\":\"" << a.symbol << "\",\"type\":\"" << a.alert_type
            << "\",\"reason\":\"" << reason << "\"}";

    auto tg = telegram_.sendMessage(msg.str());
    if (tg.ok) {
      ++result.notified;
      repo_->logNotification(a.id, "telegram", "sent", payload.str());
      result.details.push_back(a.symbol + " " + a.alert_type + " TRIGGERED+SENT");
    } else if (!telegram_.isConfigured()) {
      repo_->logNotification(a.id, "telegram", "queued", payload.str());
      result.details.push_back(a.symbol + " " + a.alert_type +
                               " TRIGGERED (telegram not configured — logged)");
    } else {
      repo_->logNotification(a.id, "telegram", "failed", payload.str());
      result.details.push_back(a.symbol + " " + a.alert_type +
                               " TRIGGERED but telegram failed: " + tg.message);
    }
  }

  result.ok = true;
  result.message = "Evaluation complete";
  return result;
}

}  // namespace stocksense::services
