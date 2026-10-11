#include "stocksense/api/HealthController.hpp"

namespace stocksense::api {

HealthController::HealthController(db::Database* db) : db_(db) {}

void HealthController::registerRoutes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/health")
  ([]() {
    crow::json::wvalue res;
    res["status"] = "ok";
    res["service"] = "stocksense-api";
    res["version"] = "0.3.0-stage3";
    return crow::response(200, res);
  });

  CROW_ROUTE(app, "/ready")
  ([this]() {
    crow::json::wvalue res;
    bool db_ok = db_ && db_->isConnected();
    res["ready"] = db_ok;
    res["database"] = db_ok ? "ok" : "unavailable";
    res["version"] = "0.3.0-stage3";
    if (!db_ok) {
      return crow::response(503, res);
    }
    return crow::response(200, res);
  });
}

}  // namespace stocksense::api
