#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace upimesh {
namespace model {

struct MeshPacket {
    std::string packetId;
    int ttl;
    int64_t createdAt; // epoch millis
    std::string ciphertext;

    // Use nlohmann's intrusive macro since fields match JSON types perfectly
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshPacket, packetId, ttl, createdAt, ciphertext)
};

} // namespace model
} // namespace upimesh
