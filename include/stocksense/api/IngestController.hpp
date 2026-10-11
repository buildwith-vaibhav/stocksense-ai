#pragma once

#include "stocksense/services/MarketDataIngester.hpp"
#include <crow.h>
#include <memory>

namespace stocksense::api {

class IngestController {
public:
  explicit IngestController(
      std::shared_ptr<services::MarketDataIngester> ingester);
  void registerRoutes(crow::SimpleApp& app);

private:
  std::shared_ptr<services::MarketDataIngester> ingester_;
};

}  // namespace stocksense::api
