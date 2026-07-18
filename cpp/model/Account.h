#pragma once

#include <string>
#include <cstdint>

namespace upimesh {
namespace model {

struct Account {
    std::string vpa;       // PK
    std::string holderName;
    int64_t balance;       // paise
    int64_t version;       // optimistic locking
};

} // namespace model
} // namespace upimesh
