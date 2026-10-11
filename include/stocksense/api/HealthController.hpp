#pragma once

#include "stocksense/db/Database.hpp"
#include <crow.h>

namespace stocksense::api {

class HealthController {
public:
  explicit HealthController(db::Database* db = nullptr);
  void registerRoutes(crow::SimpleApp& app);

private:
  db::Database* db_;
};

}  // namespace stocksense::api
