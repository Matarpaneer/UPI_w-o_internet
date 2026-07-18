#pragma once

#include <sqlite3.h>
#include <stdexcept>
#include <mutex>
#include <memory>

namespace upimesh {
namespace model {

class Database {
public:
    static Database& getInstance() {
        static Database instance;
        return instance;
    }
    
    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Get the raw SQLite connection
    sqlite3* getConnection() const {
        return db_;
    }

private:
    Database();
    ~Database();

    sqlite3* db_ = nullptr;
    
    void initSchema();
    void seedAccounts();
};

} // namespace model
} // namespace upimesh
