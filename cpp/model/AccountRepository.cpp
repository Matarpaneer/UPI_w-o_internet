#include "model/AccountRepository.h"
#include "model/Database.h"
#include <sqlite3.h>

namespace upimesh {
namespace model {

std::optional<Account> AccountRepository::findById(const std::string& vpa) {
    sqlite3* db = Database::getInstance().getConnection();
    const char* sql = "SELECT holderName, balance, version FROM accounts WHERE vpa = ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare findById statement");
    }
    
    sqlite3_bind_text(stmt, 1, vpa.c_str(), -1, SQLITE_TRANSIENT);
    
    std::optional<Account> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Account acc;
        acc.vpa = vpa;
        acc.holderName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        acc.balance = sqlite3_column_int64(stmt, 1);
        acc.version = sqlite3_column_int64(stmt, 2);
        result = acc;
    }
    
    sqlite3_finalize(stmt);
    return result;
}

void AccountRepository::save(const Account& account) {
    sqlite3* db = Database::getInstance().getConnection();
    
    // Optimsitic locking: only update if the version matches what we read
    const char* sql = "UPDATE accounts SET balance = ?, version = version + 1 WHERE vpa = ? AND version = ?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare save statement");
    }
    
    sqlite3_bind_int64(stmt, 1, account.balance);
    sqlite3_bind_text(stmt, 2, account.vpa.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, account.version);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute save statement");
    }
    
    int rowsModified = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    
    if (rowsModified == 0) {
        // If 0 rows were updated, either the account doesn't exist OR the version mismatched.
        // In our closed system, it implies a version mismatch (Optimistic Lock Failure).
        throw ConcurrencyException("Optimistic lock failure for account: " + account.vpa);
    }
}

} // namespace model
} // namespace upimesh
