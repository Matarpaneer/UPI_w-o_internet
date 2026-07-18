#pragma once

#include <openssl/evp.h>
#include <string>

namespace upimesh {
namespace crypto {

/**
 * Holds the server's RSA keypair.
 *
 * In production, the private key would live in an HSM (Hardware Security Module)
 * or at least a KMS like AWS KMS / HashiCorp Vault. NEVER in the JAR or source.
 *
 * For this demo we generate a fresh keypair on every startup. The public key is
 * exposed via /api/server-key so the (simulated) sender devices can use it to
 * encrypt payloads.
 */
class ServerKeyHolder {
public:
    ServerKeyHolder();
    ~ServerKeyHolder();

    // Prevent copying
    ServerKeyHolder(const ServerKeyHolder&) = delete;
    ServerKeyHolder& operator=(const ServerKeyHolder&) = delete;

    EVP_PKEY* getPrivateKey() const { return keyPair_; }
    EVP_PKEY* getPublicKey() const { return keyPair_; }

    std::string getPublicKeyBase64() const;

private:
    EVP_PKEY* keyPair_ = nullptr;
};

} // namespace crypto
} // namespace upimesh
