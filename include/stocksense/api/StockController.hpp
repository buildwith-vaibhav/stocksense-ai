#pragma once

#include "stocksense/repositories/StockRepository.hpp"
#include "stocksense/services/MlScorer.hpp"
#include "stocksense/services/ExplanationService.hpp"
#include <crow.h>
#include <memory>

namespace stocksense::api {

class StockController {
public:
  StockController(std::shared_ptr<repositories::StockRepository> repo,
                  std::shared_ptr<services::MlScorer> ml,
                  std::shared_ptr<services::ExplanationService> explainer);
  void registerRoutes(crow::SimpleApp& app);

private:
  std::shared_ptr<repositories::StockRepository> repo_;
  std::shared_ptr<services::MlScorer> ml_;
  std::shared_ptr<services::ExplanationService> explainer_;
};

}  // namespace stocksense::api
