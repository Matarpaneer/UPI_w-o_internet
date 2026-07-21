#include "model/TransactionRepository.h"
#include "model/Database.h"
#include <iostream>
#include <sqlite3.h>
#include <stdexcept>

namespace upimesh {
namespace model {

int64_t TransactionRepository::save(Transaction &tx) {
  std::lock_guard<std::recursive_mutex> lock(
      Database::getInstance().getMutex());
  sqlite3 *db = Database::getInstance().getConnection();
  const char *sql =
      "INSERT INTO transactions (packetHash, senderVpa, receiverVpa, amount, "
      "signedAt, settledAt, bridgeNodeId, hopCount, status) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(
        std::string("Failed to prepare tx save statement: ") +
        sqlite3_errmsg(db));
  }

  std::string statusStr = statusToString(tx.status);

  sqlite3_bind_text(stmt, 1, tx.packetHash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, tx.senderVpa.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, tx.receiverVpa.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 4, tx.amount);
  sqlite3_bind_int64(stmt, 5, tx.signedAt);
  sqlite3_bind_int64(stmt, 6, tx.settledAt);
  sqlite3_bind_text(stmt, 7, tx.bridgeNodeId.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 8, tx.hopCount);
  sqlite3_bind_text(stmt, 9, statusStr.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    std::string err = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    throw std::runtime_error("Failed to execute tx save: " + err);
  }

  tx.id = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  return tx.id;
}

bool TransactionRepository::existsByPacketHash(const std::string &packetHash) {
  std::lock_guard<std::recursive_mutex> lock(
      Database::getInstance().getMutex());
  sqlite3 *db = Database::getInstance().getConnection();
  const char *sql = "SELECT 1 FROM transactions WHERE packetHash = ?";
  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare exists statement");
  }

  sqlite3_bind_text(stmt, 1, packetHash.c_str(), -1, SQLITE_TRANSIENT);

  bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
  sqlite3_finalize(stmt);
  return exists;
}

std::vector<Transaction> TransactionRepository::findTop20ByOrderByIdDesc() {
  std::lock_guard<std::recursive_mutex> lock(
      Database::getInstance().getMutex());
  sqlite3 *db = Database::getInstance().getConnection();
  const char *sql = "SELECT id, packetHash, senderVpa, receiverVpa, amount, "
                    "signedAt, settledAt, bridgeNodeId, hopCount, status "
                    "FROM transactions ORDER BY id DESC LIMIT 20";
  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error("Failed to prepare findTop20 statement");
  }

  std::vector<Transaction> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Transaction tx;
    tx.id = sqlite3_column_int64(stmt, 0);
    tx.packetHash =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    tx.senderVpa = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    tx.receiverVpa =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    tx.amount = sqlite3_column_int64(stmt, 4);
    tx.signedAt = sqlite3_column_int64(stmt, 5);
    tx.settledAt = sqlite3_column_int64(stmt, 6);
    tx.bridgeNodeId =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
    tx.hopCount = sqlite3_column_int(stmt, 8);
    tx.status = parseStatus(
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9)));
    results.push_back(tx);
  }

  sqlite3_finalize(stmt);
  return results;
}

} // namespace model
} // namespace upimesh
