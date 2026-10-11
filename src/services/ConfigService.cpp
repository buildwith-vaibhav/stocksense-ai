#include "stocksense/ConfigService.hpp"

#include <cstdlib>
#include <string>

namespace stocksense {

void ConfigService::loadFromEnv() {
  if (const char* p = std::getenv("STOCKSENSE_PORT")) {
    try { port_ = std::stoi(p); } catch (...) {}
  }
  if (const char* h = std::getenv("STOCKSENSE_HOST")) {
    host_ = h;
  }
  if (const char* l = std::getenv("STOCKSENSE_LOG_LEVEL")) {
    log_level_ = l;
  }
  if (const char* d = std::getenv("STOCKSENSE_DB_URL")) {
    db_url_ = d;
  }
}

}  // namespace stocksense
