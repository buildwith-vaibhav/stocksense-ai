#include "stocksense/api/StockController.hpp"

#include <string>

namespace stocksense::api {

StockController::StockController(
    std::shared_ptr<repositories::StockRepository> repo,
    std::shared_ptr<services::MlScorer> ml,
    std::shared_ptr<services::ExplanationService> explainer)
    : repo_(std::move(repo)), ml_(std::move(ml)), explainer_(std::move(explainer)) {}

void StockController::registerRoutes(crow::SimpleApp& app) {
  CROW_ROUTE(app, "/api/stocks")
  ([this]() {
    auto stocks = repo_->listAll();
    crow::json::wvalue res;
    res["count"] = static_cast<int>(stocks.size());
    for (size_t i = 0; i < stocks.size(); ++i) {
      res["stocks"][i]["id"] = stocks[i].id;
      res["stocks"][i]["symbol"] = stocks[i].symbol;
      res["stocks"][i]["exchange"] = stocks[i].exchange;
      res["stocks"][i]["name"] = stocks[i].name;
      res["stocks"][i]["sector"] = stocks[i].sector;
    }
    return crow::response(200, res);
  });

  CROW_ROUTE(app, "/api/stocks/<string>")
  ([this](const std::string& symbol) {
    auto stock = repo_->findBySymbol(symbol);
    if (!stock) {
      crow::json::wvalue err;
      err["error"] = "stock_not_found";
      err["symbol"] = symbol;
      return crow::response(404, err);
    }
    crow::json::wvalue res;
    res["id"] = stock->id;
    res["symbol"] = stock->symbol;
    res["exchange"] = stock->exchange;
    res["name"] = stock->name;
    res["sector"] = stock->sector;

    auto bar = repo_->latestBar(stock->id);
    if (bar) {
      res["latest_price"] = bar->close;
      res["as_of"] = bar->ts;
      res["volume"] = bar->volume;
    } else {
      res["latest_price"] = nullptr;
      res["as_of"] = nullptr;
    }
    res["bar_count"] = static_cast<int>(repo_->countBars(stock->id));
    return crow::response(200, res);
  });

  // Stage 3 — historical bars
  CROW_ROUTE(app, "/api/stocks/<string>/bars")
  ([this](const crow::request& req, const std::string& symbol) {
    int limit = 30;
    if (auto q = req.url_params.get("limit")) {
      try { limit = std::stoi(q); } catch (...) {}
      if (limit < 1) limit = 1;
      if (limit > 500) limit = 500;
    }

    auto bars = repo_->barsForSymbol(symbol, limit);
    if (bars.empty()) {
      auto stock = repo_->findBySymbol(symbol);
      if (!stock) {
        crow::json::wvalue err;
        err["error"] = "stock_not_found";
        err["symbol"] = symbol;
        return crow::response(404, err);
      }
    }

    crow::json::wvalue res;
    res["symbol"] = symbol;
    res["count"] = static_cast<int>(bars.size());
    // Return oldest → newest for charting convenience
    for (size_t i = 0; i < bars.size(); ++i) {
      size_t idx = bars.size() - 1 - i;
      res["bars"][i]["ts"] = bars[idx].ts;
      res["bars"][i]["open"] = bars[idx].open;
      res["bars"][i]["high"] = bars[idx].high;
      res["bars"][i]["low"] = bars[idx].low;
      res["bars"][i]["close"] = bars[idx].close;
      res["bars"][i]["volume"] = bars[idx].volume;
    }
    return crow::response(200, res);
  });

  CROW_ROUTE(app, "/api/stocks/<string>/analysis")
  ([this](const std::string& symbol) {
    auto snap = repo_->analysisForSymbol(symbol);
    if (!snap) {
      crow::json::wvalue err;
      err["error"] = "stock_not_found";
      err["symbol"] = symbol;
      return crow::response(404, err);
    }

    crow::json::wvalue res;
    res["symbol"] = snap->symbol;
    res["exchange"] = snap->exchange;

    if (snap->has_price) {
      res["price"] = snap->price;
      res["data_timestamp"] = snap->data_timestamp;
      res["data_freshness"] = snap->data_freshness;
    } else {
      res["price"] = nullptr;
      res["data_timestamp"] = nullptr;
      res["data_freshness"] = "unavailable";
    }

    res["technical_signal"] = snap->technical_signal;
    res["risk_level"] = snap->risk_level;

    auto ml = ml_->score(*snap);
    if (ml.ok) {
      res["probability_up"] = ml.probability_up;
      res["confidence"] = ml.confidence;
      res["model_version"] = ml.model_version;
      res["status"] = "ML_SCORED";
    } else {
      res["probability_up"] = nullptr;
      res["confidence"] = "NONE";
      res["model_version"] = ml.model_version;
      res["status"] = snap->has_indicators
                          ? "INDICATORS_READY"
                          : (snap->has_price ? "DATA_AVAILABLE" : "NO_PRICE_DATA");
    }

    if (snap->has_indicators) {
      res["indicators"]["rsi_14"] = snap->rsi_14;
      res["indicators"]["sma_20"] = snap->sma_20;
      res["indicators"]["sma_50"] = snap->sma_50;
      res["indicators"]["sma_200"] = snap->sma_200;
      res["indicators"]["macd"] = snap->macd;
      res["indicators"]["macd_signal"] = snap->macd_signal;
      res["indicators"]["macd_hist"] = snap->macd_hist;
      res["indicators"]["atr_14"] = snap->atr_14;
    }

    auto expl = explainer_->explain(*snap, ml);
    res["explanation"]["summary"] = expl.summary;
    res["explanation"]["detail"] = expl.detail;
    res["explanation"]["source"] = expl.source;

    res["disclaimer"] =
        "Not investment advice. Automated ML + explanation output. "
        "Stage 8 — probability_up + short commentary.";
    return crow::response(200, res);
  });

  CROW_ROUTE(app, "/api/admin/ml/status")
  ([this]() {
    crow::json::wvalue res;
    res["stage"] = 7;
    res["loaded"] = ml_->isLoaded();
    res["model_version"] = ml_->modelVersion();
    res["reload"] = "POST /api/admin/ml/reload";
    res["analysis"] = "GET /api/stocks/{symbol}/analysis → probability_up";
    return crow::response(200, res);
  });

  CROW_ROUTE(app, "/api/admin/ml/reload")
      .methods("POST"_method, "GET"_method)([this]() {
        bool ok = ml_->reload();
        crow::json::wvalue res;
        res["ok"] = ok;
        res["loaded"] = ml_->isLoaded();
        res["model_version"] = ml_->modelVersion();
        return crow::response(ok ? 200 : 500, res);
      });
}


}  // namespace stocksense::api
