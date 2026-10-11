#include "stocksense/ConfigService.hpp"
#include "stocksense/db/Database.hpp"
#include "stocksense/repositories/StockRepository.hpp"
#include "stocksense/services/MarketDataIngester.hpp"
#include "stocksense/services/IndicatorEngine.hpp"
#include "stocksense/services/TelegramNotifier.hpp"
#include "stocksense/services/AlertEngine.hpp"
#include "stocksense/services/MlScorer.hpp"
#include "stocksense/services/ExplanationService.hpp"
#include "stocksense/api/HealthController.hpp"
#include "stocksense/api/StockController.hpp"
#include "stocksense/api/IngestController.hpp"
#include "stocksense/api/IndicatorController.hpp"
#include "stocksense/api/AlertController.hpp"
#include "stocksense/api/DashboardController.hpp"

#include <crow.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>

int main() {
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  spdlog::info("StockSense AI — Stage 8 starting...");

  stocksense::ConfigService config;
  config.loadFromEnv();

  auto database = std::make_shared<stocksense::db::Database>(config.dbUrl());
  if (!database->isConnected()) {
    spdlog::warn("Running without database connection.");
  }

  auto stockRepo =
      std::make_shared<stocksense::repositories::StockRepository>(*database);
  auto ingester =
      std::make_shared<stocksense::services::MarketDataIngester>(stockRepo);
  auto indicators =
      std::make_shared<stocksense::services::IndicatorEngine>(stockRepo);
  stocksense::services::TelegramNotifier telegram;
  auto alerts = std::make_shared<stocksense::services::AlertEngine>(
      stockRepo, telegram);
  auto ml = std::make_shared<stocksense::services::MlScorer>();
  auto explainer = std::make_shared<stocksense::services::ExplanationService>();

  crow::SimpleApp app;

  stocksense::api::HealthController health(database.get());
  health.registerRoutes(app);

  stocksense::api::StockController stocks(stockRepo, ml, explainer);
  stocks.registerRoutes(app);

  stocksense::api::IngestController ingest(ingester);
  ingest.registerRoutes(app);

  stocksense::api::IndicatorController ind(indicators);
  ind.registerRoutes(app);

  stocksense::api::AlertController alertCtrl(alerts, stockRepo);
  alertCtrl.registerRoutes(app);

  stocksense::api::DashboardController dash(stockRepo);
  dash.registerRoutes(app);

  CROW_ROUTE(app, "/")([]() {
    crow::json::wvalue res;
    res["name"] = "StockSense AI";
    res["version"] = "0.8.0-stage8";
    res["status"] = "running";
    res["message"] = "Stage 8 — explanations (rules + optional LLM)";
    res["dashboard"] = "http://localhost:8080/dashboard";
    res["features"][0] = "GET /api/stocks/{symbol}/analysis → explanation";
    res["features"][1] = "Set STOCKSENSE_LLM_API_KEY for LLM augment";
    return crow::response(200, res);
  });

  spdlog::info("Listening on {}:{}", config.host(), config.port());
  spdlog::info("Dashboard: http://127.0.0.1:{}/dashboard", config.port());
  app.bindaddr(config.host()).port(config.port()).multithreaded().run();
  return 0;
}
