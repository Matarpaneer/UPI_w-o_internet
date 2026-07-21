#pragma once

#include <memory>
#include <mutex>
#include <sqlite3.h>
#include <stdexcept>

namespace upimesh {
namespace model {

class Database {
public:
  static Database &getInstance() {
    static Database instance;
    return instance;
  }

  // Non-copyable
  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  // Get the raw SQLite connection
  sqlite3 *getConnection() const { return db_; }

  // Expose a recursive mutex so services can wrap multiple repository calls in
  // one transaction
  std::recursive_mutex &getMutex() { return connectionMutex_; }

  void execute(const char *sql);

private:
  Database();
  ~Database();

  sqlite3 *db_ = nullptr;
  std::recursive_mutex connectionMutex_;

  void initSchema();
  void seedAccounts();
};

} // namespace model
} // namespace upimesh
