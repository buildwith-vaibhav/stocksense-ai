#pragma once

#include <string>

namespace stocksense {

class ConfigService {
public:
  void loadFromEnv();

  int port() const { return port_; }
  const std::string& host() const { return host_; }
  const std::string& logLevel() const { return log_level_; }
  const std::string& dbUrl() const { return db_url_; }

private:
  int port_ = 8080;
  std::string host_ = "0.0.0.0";
  std::string log_level_ = "info";
  std::string db_url_ = "postgresql://stocksense:stocksense@127.0.0.1:5432/stocksense";
};

}  // namespace stocksense
