#pragma once

#include <memory>
#include <string>

#ifdef STOCKSENSE_HAS_POSTGRES
#include <pqxx/pqxx>
#endif

namespace stocksense::db {

class Database {
public:
  explicit Database(std::string conninfo);
  ~Database() = default;

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  bool isConnected() const;
  std::string connectionInfo() const { return conninfo_; }

#ifdef STOCKSENSE_HAS_POSTGRES
  // Returns a short-lived connection (caller owns it)
  std::unique_ptr<pqxx::connection> connect() const;
#endif

private:
  std::string conninfo_;
  bool connected_ = false;
};

}  // namespace stocksense::db
