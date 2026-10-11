#include "stocksense/api/IngestController.hpp"

#include <string>

namespace stocksense::api {

IngestController::IngestController(
    std::shared_ptr<services::MarketDataIngester> ingester)
    : ingester_(std::move(ingester)) {}

void IngestController::registerRoutes(crow::SimpleApp& app) {
  // GET /api/admin/ingest/status — always available for discovery
  CROW_ROUTE(app, "/api/admin/ingest/status")
  ([]() {
    crow::json::wvalue res;
    res["stage"] = 3;
    res["endpoints"]["status"] = "GET  /api/admin/ingest/status";
    res["endpoints"]["synthetic"] = "POST /api/admin/ingest/synthetic?days=30";
    res["endpoints"]["csv"] = "POST /api/admin/ingest/csv?path=...";
    res["endpoints"]["bars"] = "GET  /api/stocks/{symbol}/bars?limit=30";
    res["note"] =
        "Synthetic generator is offline-first (no API key). "
        "CSV format: symbol,date,open,high,low,close,volume";
    return crow::response(200, res);
  });

  // POST/GET /api/admin/ingest/synthetic?days=30
  // GET allowed so browser and simple PowerShell tests work
  CROW_ROUTE(app, "/api/admin/ingest/synthetic")
      .methods("POST"_method, "GET"_method)([this](const crow::request& req) {
        int days = 30;
        if (auto q = req.url_params.get("days")) {
          try {
            days = std::stoi(q);
          } catch (...) {
          }
        }
        auto result = ingester_->ingestSynthetic(days);
        crow::json::wvalue res;
        res["ok"] = result.ok;
        res["bars_written"] = result.bars_written;
        res["stocks_touched"] = result.stocks_touched;
        res["message"] = result.message;
        res["days_requested"] = days;
        return crow::response(result.ok ? 200 : 500, res);
      });

  // POST/GET /api/admin/ingest/csv?path=...
  CROW_ROUTE(app, "/api/admin/ingest/csv")
      .methods("POST"_method, "GET"_method)([this](const crow::request& req) {
        std::string path;
        if (auto q = req.url_params.get("path")) {
          path = q;
        }
        if (path.empty() && !req.body.empty()) {
          try {
            auto body = crow::json::load(req.body);
            if (body.has("path")) path = body["path"].s();
          } catch (...) {
          }
        }
        if (path.empty()) {
          crow::json::wvalue err;
          err["ok"] = false;
          err["error"] = "missing_path";
          err["hint"] =
              "Provide ?path=... query param or JSON body {\"path\":\"...\"}";
          return crow::response(400, err);
        }

        auto result = ingester_->ingestFromCsv(path);
        crow::json::wvalue res;
        res["ok"] = result.ok;
        res["bars_written"] = result.bars_written;
        res["message"] = result.message;
        res["path"] = path;
        return crow::response(result.ok ? 200 : 500, res);
      });
}

}  // namespace stocksense::api
