#pragma once

#include "crypto/ServerKeyHolder.h"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace upimesh {
namespace crypto {

/**
 * Hybrid encryption — the same pattern used by TLS, PGP, Signal, etc.
 *
 * Why hybrid? RSA can only encrypt small data (~245 bytes for a 2048-bit key).
 * Our payment instruction (JSON) might be ~300 bytes, and in real use we might
 * include device certificates and signatures pushing it well over.
 *
 * Solution: generate a fresh AES key per packet, encrypt the JSON with AES-GCM
 * (fast + authenticated), then encrypt JUST the AES key with RSA-OAEP.
 *
 * Wire format (after base64 encoding):
 *   [ 256 bytes RSA-encrypted AES key ][ 12 bytes GCM IV ][ ciphertext + 16-byte tag ]
 *
 * AES-GCM is authenticated encryption: any single-bit tampering with the ciphertext
 * causes decryption to fail with an exception. This is what makes it safe for
 * untrusted intermediates to hold.
 */
class HybridCryptoService {
public:
    HybridCryptoService(std::shared_ptr<ServerKeyHolder> serverKey);

    // Encrypts a JSON object simulating the sender device behavior
    std::string encrypt(const nlohmann::json& instruction, EVP_PKEY* serverPublicKey);

    // Decrypts the ciphertext using the server private key
    nlohmann::json decrypt(const std::string& base64Ciphertext);

private:
    std::shared_ptr<ServerKeyHolder> serverKey_;

    // Helpers
    std::string base64Encode(const std::vector<unsigned char>& data);
    std::vector<unsigned char> base64Decode(const std::string& b64);
};

} // namespace crypto
} // namespace upimesh
