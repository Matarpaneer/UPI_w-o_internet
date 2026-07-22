#include "model/Database.h"
#include <iostream>
#include <string>

namespace upimesh {
namespace model {

Database::Database() {
  // Open in-memory database
  if (sqlite3_open(":memory:", &db_) != SQLITE_OK) {
    std::string err = sqlite3_errmsg(db_);
    sqlite3_close(db_);
    throw std::runtime_error("Failed to open SQLite in-memory database: " +
                             err);
  }

  initSchema();
  seedAccounts();
}

Database::~Database() {
  if (db_) {
    sqlite3_close(db_);
  }
}

void Database::initSchema() {
  const char *schema = R"(
        CREATE TABLE accounts (
            vpa TEXT PRIMARY KEY,
            holderName TEXT NOT NULL,
            balance INTEGER NOT NULL,
            version INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE transactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            packetHash TEXT NOT NULL UNIQUE,
            senderVpa TEXT NOT NULL,
            receiverVpa TEXT NOT NULL,
            amount INTEGER NOT NULL,
            signedAt INTEGER NOT NULL,
            settledAt INTEGER NOT NULL,
            bridgeNodeId TEXT NOT NULL,
            hopCount INTEGER NOT NULL,
            status TEXT NOT NULL
        );
        
        CREATE INDEX idx_packet_hash ON transactions(packetHash);
    )";

  char *errMsg = nullptr;
  if (sqlite3_exec(db_, schema, nullptr, nullptr, &errMsg) != SQLITE_OK) {
    std::string err = errMsg;
    sqlite3_free(errMsg);
    throw std::runtime_error("Failed to initialize schema: " + err);
  }
}

void Database::seedAccounts() {
  // Exact seed data defined from DemoService, amounts shifted to paise (* 100)
  const char *seedSql = R"(
        INSERT INTO accounts (vpa, holderName, balance, version) VALUES
        ('alice@demo', 'Alice', 500000, 0),
        ('bob@demo', 'Bob', 100000, 0),
        ('carol@demo', 'Carol', 250000, 0),
        ('dave@demo', 'Dave', 50000, 0);
    )";

  char *errMsg = nullptr;
  if (sqlite3_exec(db_, seedSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
    std::string err = errMsg;
    sqlite3_free(errMsg);
    throw std::runtime_error("Failed to seed demo accounts: " + err);
  }
  std::cout
      << "Database initialized and 4 demo accounts seeded (SQLite :memory:)\n";
}

void Database::execute(const char *sql) {
  char *errMsg = nullptr;
  if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
    std::string errStr = errMsg;
    sqlite3_free(errMsg);
    throw std::runtime_error("SQLite execute failed: " + errStr);
  }
}

void Database::reset() {
  std::lock_guard<std::recursive_mutex> lock(connectionMutex_);
  execute("DELETE FROM transactions;");
  execute("DELETE FROM accounts;");
  seedAccounts();
}

} // namespace model
} // namespace upimesh
