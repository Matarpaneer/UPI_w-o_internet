#pragma once

#include "model/Account.h"
#include <optional>
#include <stdexcept>

namespace upimesh {
namespace model {

class ConcurrencyException : public std::runtime_error {
public:
    ConcurrencyException(const std::string& msg) : std::runtime_error(msg) {}
};

class AccountRepository {
public:
    std::optional<Account> findById(const std::string& vpa);
    
    // Updates balance and increments version. Throws ConcurrencyException if version mismatch.
    void save(const Account& account);
};

} // namespace model
} // namespace upimesh
