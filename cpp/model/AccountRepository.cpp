#include "model/AccountRepository.h"
#include "model/Database.h"
#include <iostream>

namespace upimesh {
namespace model {

std::optional<Account> AccountRepository::findById(const std::string &vpa) {
  std::lock_guard<std::recursive_mutex> lock(
      Database::getInstance().getMutex());
  sqlite3 *db = Database::getInstance().getConnection();
  const char *sql =
      "SELECT vpa, holderName, balance, version FROM accounts WHERE vpa = ?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, vpa.c_str(), -1, SQLITE_TRANSIENT);

  std::optional<Account> account = std::nullopt;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    Account acc;
    acc.vpa = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    acc.holderName =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    acc.balance = sqlite3_column_int64(stmt, 2);
    acc.version = sqlite3_column_int64(stmt, 3);
    account = acc;
  }

  sqlite3_finalize(stmt);
  return account;
}

void AccountRepository::save(const Account &account) {
  std::lock_guard<std::recursive_mutex> lock(
      Database::getInstance().getMutex());
  sqlite3 *db = Database::getInstance().getConnection();
  const char *sql =
      "INSERT INTO accounts (vpa, holderName, balance, version) VALUES (?, ?, "
      "?, ?) "
      "ON CONFLICT(vpa) DO UPDATE SET holderName=excluded.holderName, "
      "balance=excluded.balance, version=excluded.version";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, account.vpa.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, account.holderName.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, account.balance);
    sqlite3_bind_int64(stmt, 4, account.version);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);
}

std::vector<Account> AccountRepository::findAll() {
  std::lock_guard<std::recursive_mutex> lock(
      Database::getInstance().getMutex());
  sqlite3 *db = Database::getInstance().getConnection();
  std::vector<Account> accounts;
  const char *sql = "SELECT vpa, holderName, balance, version FROM accounts";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      Account acc;
      acc.vpa = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
      acc.holderName =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      acc.balance = sqlite3_column_int64(stmt, 2);
      acc.version = sqlite3_column_int64(stmt, 3);
      accounts.push_back(acc);
    }
  }
  sqlite3_finalize(stmt);
  return accounts;
}

} // namespace model
} // namespace upimesh
