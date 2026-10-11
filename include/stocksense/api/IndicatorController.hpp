#pragma once

#include "stocksense/services/IndicatorEngine.hpp"
#include <crow.h>
#include <memory>

namespace stocksense::api {

class IndicatorController {
public:
  explicit IndicatorController(
      std::shared_ptr<services::IndicatorEngine> engine);
  void registerRoutes(crow::SimpleApp& app);

private:
  std::shared_ptr<services::IndicatorEngine> engine_;
};

}  // namespace stocksense::api
