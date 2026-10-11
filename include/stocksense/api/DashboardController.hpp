#pragma once

#include "stocksense/repositories/StockRepository.hpp"
#include <crow.h>
#include <memory>

namespace stocksense::api {

class DashboardController {
public:
  explicit DashboardController(
      std::shared_ptr<repositories::StockRepository> repo);
  void registerRoutes(crow::SimpleApp& app);

private:
  std::shared_ptr<repositories::StockRepository> repo_;
  static crow::response htmlPage();
};

}  // namespace stocksense::api
