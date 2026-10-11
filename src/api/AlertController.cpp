#include "stocksense/api/AlertController.hpp"

#include <string>

namespace stocksense::api {

AlertController::AlertController(
    std::shared_ptr<services::AlertEngine> engine,
    std::shared_ptr<repositories::StockRepository> repo)
    : engine_(std::move(engine)), repo_(std::move(repo)) {}

void AlertController::registerRoutes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/api/admin/alerts/status")
  ([]() {
    crow::json::wvalue res;
    res["stage"] = 5;
    res["endpoints"]["list"] = "GET /api/alerts";
    res["endpoints"]["seed"] = "GET|POST /api/admin/alerts/seed";
    res["endpoints"]["evaluate"] = "GET|POST /api/admin/alerts/evaluate";
    res["endpoints"]["create"] =
        "POST /api/alerts/create  JSON {symbol, alert_type, threshold}";
    res["telegram"] =
        "Set STOCKSENSE_TELEGRAM_BOT_TOKEN and STOCKSENSE_TELEGRAM_CHAT_ID";
    res["alert_types"][0] = "price_above";
    res["alert_types"][1] = "price_below";
    res["alert_types"][2] = "rsi_overbought";
    res["alert_types"][3] = "rsi_oversold";
    res["alert_types"][4] = "ma_crossover";
    return crow::response(200, res);
  });

  CROW_ROUTE(app, "/api/alerts")
  ([this]() {
    auto alerts = repo_->listAlerts();
    crow::json::wvalue res;
    res["count"] = static_cast<int>(alerts.size());
    for (size_t i = 0; i < alerts.size(); ++i) {
      res["alerts"][i]["id"] = alerts[i].id;
      res["alerts"][i]["symbol"] = alerts[i].symbol;
      res["alerts"][i]["alert_type"] = alerts[i].alert_type;
      res["alerts"][i]["threshold"] = alerts[i].threshold;
      res["alerts"][i]["is_active"] = alerts[i].is_active;
      res["alerts"][i]["cooldown_minutes"] = alerts[i].cooldown_minutes;
      res["alerts"][i]["last_triggered_at"] = alerts[i].last_triggered_at;
    }
    return crow::response(200, res);
  });

  CROW_ROUTE(app, "/api/admin/alerts/seed")
      .methods("POST"_method, "GET"_method)([this]() {
        int n = engine_->ensureDemoAlerts();
        crow::json::wvalue res;
        res["ok"] = true;
        res["created"] = n;
        res["message"] = n > 0 ? "Demo alerts created"
                               : "Alerts already exist (or seed failed)";
        res["total"] = repo_->countAlerts();
        return crow::response(200, res);
      });

  CROW_ROUTE(app, "/api/admin/alerts/evaluate")
      .methods("POST"_method, "GET"_method)([this]() {
        auto result = engine_->evaluateAll();
        crow::json::wvalue res;
        res["ok"] = result.ok;
        res["checked"] = result.checked;
        res["triggered"] = result.triggered;
        res["notified"] = result.notified;
        res["suppressed_cooldown"] = result.suppressed_cooldown;
        res["message"] = result.message;
        for (size_t i = 0; i < result.details.size(); ++i) {
          res["details"][i] = result.details[i];
        }
        return crow::response(result.ok ? 200 : 500, res);
      });

  CROW_ROUTE(app, "/api/alerts/create")
      .methods("POST"_method, "GET"_method)([this](const crow::request& req) {
        crow::json::wvalue err;
        std::string symbol;
        std::string alert_type;
        double threshold = 0.0;
        int cooldown = 60;

        if (auto q = req.url_params.get("symbol")) symbol = q;
        if (auto q = req.url_params.get("alert_type")) alert_type = q;
        if (auto q = req.url_params.get("threshold")) {
          try {
            threshold = std::stod(q);
          } catch (...) {
          }
        }

        if (!req.body.empty()) {
          try {
            auto body = crow::json::load(req.body);
            if (body.has("symbol")) symbol = body["symbol"].s();
            if (body.has("alert_type")) alert_type = body["alert_type"].s();
            if (body.has("threshold")) threshold = body["threshold"].d();
            if (body.has("cooldown_minutes"))
              cooldown = static_cast<int>(body["cooldown_minutes"].i());
          } catch (...) {
          }
        }

        if (symbol.empty() || alert_type.empty()) {
          err["error"] = "missing_fields";
          err["hint"] =
              "Provide symbol and alert_type as query params or JSON body";
          return crow::response(400, err);
        }

        auto stock = repo_->findBySymbol(symbol);
        if (!stock) {
          err["error"] = "stock_not_found";
          err["symbol"] = symbol;
          return crow::response(404, err);
        }
        int64_t userId = repo_->ensureDemoUser();
        int64_t id =
            repo_->createAlert(userId, stock->id, alert_type, threshold, cooldown);
        if (id == 0) {
          err["error"] = "create_failed";
          return crow::response(500, err);
        }
        crow::json::wvalue res;
        res["ok"] = true;
        res["id"] = id;
        res["symbol"] = symbol;
        res["alert_type"] = alert_type;
        res["threshold"] = threshold;
        return crow::response(201, res);
      });
}

}  // namespace stocksense::api
