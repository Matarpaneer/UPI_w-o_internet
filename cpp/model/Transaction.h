#pragma once

#include <string>
#include <cstdint>

namespace upimesh {
namespace model {

enum class Status {
    SETTLED,
    REJECTED,
    DUPLICATE_DROPPED,
    INVALID
};

// Helper to convert Status to string for SQLite and JSON
inline std::string statusToString(Status s) {
    switch(s) {
        case Status::SETTLED: return "SETTLED";
        case Status::REJECTED: return "REJECTED";
        case Status::DUPLICATE_DROPPED: return "DUPLICATE_DROPPED";
        case Status::INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}

// Helper to parse string back to Status
inline Status parseStatus(const std::string& str) {
    if (str == "SETTLED") return Status::SETTLED;
    if (str == "REJECTED") return Status::REJECTED;
    if (str == "DUPLICATE_DROPPED") return Status::DUPLICATE_DROPPED;
    if (str == "INVALID") return Status::INVALID;
    return Status::INVALID; // Default fallback
}

struct Transaction {
    int64_t id;             // PK (auto-increment in SQLite, we don't set it initially)
    std::string packetHash; // UNIQUE INDEX
    std::string senderVpa;
    std::string receiverVpa;
    int64_t amount;         // paise
    int64_t signedAt;       // epoch millis
    int64_t settledAt;      // epoch millis
    std::string bridgeNodeId;
    int hopCount;
    Status status;
};

} // namespace model
} // namespace upimesh
