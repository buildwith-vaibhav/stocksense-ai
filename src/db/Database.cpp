#include "stocksense/db/Database.hpp"

#include <spdlog/spdlog.h>

namespace stocksense::db {

Database::Database(std::string conninfo) : conninfo_(std::move(conninfo)) {
#ifdef STOCKSENSE_HAS_POSTGRES
  try {
    auto c = connect();
    if (c && c->is_open()) {
      connected_ = true;
      spdlog::info("Database connected: {}", c->dbname());
    }
  } catch (const std::exception& e) {
    spdlog::error("Database connection failed: {}", e.what());
    connected_ = false;
  }
#else
  spdlog::warn("Built without PostgreSQL support");
  connected_ = false;
#endif
}

bool Database::isConnected() const {
  return connected_;
}

#ifdef STOCKSENSE_HAS_POSTGRES
std::unique_ptr<pqxx::connection> Database::connect() const {
  return std::make_unique<pqxx::connection>(conninfo_);
}
#endif

}  // namespace stocksense::db
