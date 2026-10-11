#include "stocksense/api/IndicatorController.hpp"

#include <string>

namespace stocksense::api {

IndicatorController::IndicatorController(
    std::shared_ptr<services::IndicatorEngine> engine)
    : engine_(std::move(engine)) {}

void IndicatorController::registerRoutes(crow::SimpleApp& app) {
  // GET or POST /api/admin/indicators/compute?symbol=TCS (optional)
  CROW_ROUTE(app, "/api/admin/indicators/compute")
      .methods("POST"_method, "GET"_method)([this](const crow::request& req) {
        std::string symbol;
        if (auto q = req.url_params.get("symbol")) {
          symbol = q;
        }
        auto result = engine_->computeAll(symbol);
        crow::json::wvalue res;
        res["ok"] = result.ok;
        res["stocks_touched"] = result.stocks_touched;
        res["rows_written"] = result.rows_written;
        res["message"] = result.message;
        if (!symbol.empty()) res["symbol_filter"] = symbol;
        return crow::response(result.ok ? 200 : 500, res);
      });

  CROW_ROUTE(app, "/api/admin/indicators/status")
  ([]() {
    crow::json::wvalue res;
    res["stage"] = 4;
    res["endpoints"]["compute"] =
        "GET|POST /api/admin/indicators/compute?symbol=TCS";
    res["endpoints"]["analysis"] = "GET /api/stocks/{symbol}/analysis";
    res["note"] =
        "Run compute after ingesting enough bars (prefer days>=120 for SMA200).";
    return crow::response(200, res);
  });
}

}  // namespace stocksense::api
