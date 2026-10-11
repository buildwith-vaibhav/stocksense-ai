#pragma once

#include "stocksense/services/AlertEngine.hpp"
#include "stocksense/repositories/StockRepository.hpp"
#include <crow.h>
#include <memory>

namespace stocksense::api {

class AlertController {
public:
  AlertController(std::shared_ptr<services::AlertEngine> engine,
                  std::shared_ptr<repositories::StockRepository> repo);
  void registerRoutes(crow::SimpleApp& app);

private:
  std::shared_ptr<services::AlertEngine> engine_;
  std::shared_ptr<repositories::StockRepository> repo_;
};

}  // namespace stocksense::api
