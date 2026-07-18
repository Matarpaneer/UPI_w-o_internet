#include "crypto/ServerKeyHolder.h"
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/x509.h>
#include <stdexcept>
#include <iostream>

namespace upimesh {
namespace crypto {

ServerKeyHolder::ServerKeyHolder() {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_PKEY_CTX for RSA");
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize keygen");
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("Failed to set RSA keygen bits");
    }

    if (EVP_PKEY_keygen(ctx, &keyPair_) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("Failed to generate RSA keypair");
    }
    
    EVP_PKEY_CTX_free(ctx);
    
    std::cout << "Server RSA keypair generated (2048-bit). Public key fingerprint: " 
              << getPublicKeyBase64().substr(0, 32) << "...\n";
}

ServerKeyHolder::~ServerKeyHolder() {
    if (keyPair_) {
        EVP_PKEY_free(keyPair_);
    }
}

std::string ServerKeyHolder::getPublicKeyBase64() const {
    if (!keyPair_) return "";
    
    // Write public key to DER format first (SubjectPublicKeyInfo/X.509)
    unsigned char* der = nullptr;
    int len = i2d_PUBKEY(keyPair_, &der);
    if (len < 0) {
        throw std::runtime_error("Failed to encode public key to DER");
    }
    
    // Now base64 encode the DER string
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    
    // Don't add newlines
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    
    BIO_write(b64, der, len);
    BIO_flush(b64);
    
    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    
    std::string b64Str(bptr->data, bptr->length);
    
    BIO_free_all(b64);
    OPENSSL_free(der);
    
    return b64Str;
}

} // namespace crypto
} // namespace upimesh
