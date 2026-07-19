#include "crypto/HybridCryptoService.h"
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace upimesh {
namespace crypto {

constexpr int AES_KEY_BYTES = 32; // 256 bits
constexpr int GCM_IV_BYTES = 12;
constexpr int GCM_TAG_BYTES = 16;
constexpr int RSA_ENCRYPTED_KEY_BYTES = 256; // 2048-bit RSA

HybridCryptoService::HybridCryptoService(std::shared_ptr<ServerKeyHolder> serverKey)
    : serverKey_(std::move(serverKey)) {}

std::string HybridCryptoService::encrypt(const nlohmann::json& instruction, EVP_PKEY* serverPublicKey) {
    std::string plaintext = instruction.dump();
    
    // 1. Generate one-time AES key
    std::vector<unsigned char> aesKey(AES_KEY_BYTES);
    if (RAND_bytes(aesKey.data(), AES_KEY_BYTES) != 1) {
        throw std::runtime_error("Failed to generate random AES key");
    }
    
    // 2. Generate IV
    std::vector<unsigned char> iv(GCM_IV_BYTES);
    if (RAND_bytes(iv.data(), GCM_IV_BYTES) != 1) {
        throw std::runtime_error("Failed to generate random IV");
    }

    // 3. AES-GCM Encrypt
    EVP_CIPHER_CTX* aesCtx = EVP_CIPHER_CTX_new();
    if (!aesCtx) throw std::runtime_error("Failed to create AES context");

    if (EVP_EncryptInit_ex(aesCtx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(aesCtx);
        throw std::runtime_error("Failed to init AES-GCM");
    }

    if (EVP_EncryptInit_ex(aesCtx, nullptr, nullptr, aesKey.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(aesCtx);
        throw std::runtime_error("Failed to set AES key and IV");
    }

    std::vector<unsigned char> ciphertext(plaintext.length() + EVP_MAX_BLOCK_LENGTH);
    int len;
    if (EVP_EncryptUpdate(aesCtx, ciphertext.data(), &len, 
                          reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.length()) != 1) {
        EVP_CIPHER_CTX_free(aesCtx);
        throw std::runtime_error("Failed during AES-GCM encrypt update");
    }
    int ciphertextLen = len;

    if (EVP_EncryptFinal_ex(aesCtx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(aesCtx);
        throw std::runtime_error("Failed during AES-GCM encrypt final");
    }
    ciphertextLen += len;
    ciphertext.resize(ciphertextLen);

    // Get the TAG
    std::vector<unsigned char> tag(GCM_TAG_BYTES);
    if (EVP_CIPHER_CTX_ctrl(aesCtx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_BYTES, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(aesCtx);
        throw std::runtime_error("Failed to get AES-GCM tag");
    }
    EVP_CIPHER_CTX_free(aesCtx);

    // 4. RSA-OAEP encrypt the AES key
    EVP_PKEY_CTX* rsaCtx = EVP_PKEY_CTX_new(serverPublicKey, nullptr);
    if (!rsaCtx) throw std::runtime_error("Failed to create RSA context");

    if (EVP_PKEY_encrypt_init(rsaCtx) <= 0) {
        EVP_PKEY_CTX_free(rsaCtx);
        throw std::runtime_error("Failed to init RSA encrypt");
    }

    if (EVP_PKEY_CTX_set_rsa_padding(rsaCtx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
        EVP_PKEY_CTX_set_rsa_oaep_md(rsaCtx, EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set_rsa_mgf1_md(rsaCtx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(rsaCtx);
        throw std::runtime_error("Failed to set RSA-OAEP parameters");
    }

    size_t outlen;
    if (EVP_PKEY_encrypt(rsaCtx, nullptr, &outlen, aesKey.data(), aesKey.size()) <= 0) {
        EVP_PKEY_CTX_free(rsaCtx);
        throw std::runtime_error("Failed to determine RSA buffer size");
    }

    std::vector<unsigned char> rsaCiphertext(outlen);
    if (EVP_PKEY_encrypt(rsaCtx, rsaCiphertext.data(), &outlen, aesKey.data(), aesKey.size()) <= 0) {
        EVP_PKEY_CTX_free(rsaCtx);
        throw std::runtime_error("Failed to encrypt with RSA-OAEP");
    }
    rsaCiphertext.resize(outlen);
    EVP_PKEY_CTX_free(rsaCtx);

    // 5. Pack: [RSA cipher][IV][AES cipher + tag]
    std::vector<unsigned char> packed;
    packed.reserve(rsaCiphertext.size() + iv.size() + ciphertext.size() + tag.size());
    packed.insert(packed.end(), rsaCiphertext.begin(), rsaCiphertext.end());
    packed.insert(packed.end(), iv.begin(), iv.end());
    packed.insert(packed.end(), ciphertext.begin(), ciphertext.end());
    packed.insert(packed.end(), tag.begin(), tag.end());

    return base64Encode(packed);
}

nlohmann::json HybridCryptoService::decrypt(const std::string& base64Ciphertext) {
    std::vector<unsigned char> all = base64Decode(base64Ciphertext);
    
    if (all.size() < RSA_ENCRYPTED_KEY_BYTES + GCM_IV_BYTES + GCM_TAG_BYTES) {
        throw std::invalid_argument("Ciphertext too short");
    }

    // Unpack
    auto it = all.begin();
    std::vector<unsigned char> rsaCiphertext(it, it + RSA_ENCRYPTED_KEY_BYTES);
    it += RSA_ENCRYPTED_KEY_BYTES;
    
    std::vector<unsigned char> iv(it, it + GCM_IV_BYTES);
    it += GCM_IV_BYTES;
    
    // Remaining is AES ciphertext + Tag (last 16 bytes)
    size_t aesCipherLen = all.end() - it - GCM_TAG_BYTES;
    std::vector<unsigned char> aesCiphertext(it, it + aesCipherLen);
    it += aesCipherLen;
    
    std::vector<unsigned char> tag(it, all.end());

    // 1. RSA decrypt the AES key
    EVP_PKEY_CTX* rsaCtx = EVP_PKEY_CTX_new(serverKey_->getPrivateKey(), nullptr);
    if (!rsaCtx) throw std::runtime_error("Failed to create RSA context");

    if (EVP_PKEY_decrypt_init(rsaCtx) <= 0) {
        EVP_PKEY_CTX_free(rsaCtx);
        throw std::runtime_error("Failed to init RSA decrypt");
    }

    if (EVP_PKEY_CTX_set_rsa_padding(rsaCtx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
        EVP_PKEY_CTX_set_rsa_oaep_md(rsaCtx, EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set_rsa_mgf1_md(rsaCtx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(rsaCtx);
        throw std::runtime_error("Failed to set RSA-OAEP parameters");
    }

    size_t aesKeyLen;
    if (EVP_PKEY_decrypt(rsaCtx, nullptr, &aesKeyLen, rsaCiphertext.data(), rsaCiphertext.size()) <= 0) {
        EVP_PKEY_CTX_free(rsaCtx);
        throw std::runtime_error("Failed to determine AES key buffer size (decryption failure)");
    }

    std::vector<unsigned char> aesKey(aesKeyLen);
    if (EVP_PKEY_decrypt(rsaCtx, aesKey.data(), &aesKeyLen, rsaCiphertext.data(), rsaCiphertext.size()) <= 0) {
        EVP_PKEY_CTX_free(rsaCtx);
        throw std::runtime_error("RSA decryption failed (tampered key?)");
    }
    aesKey.resize(aesKeyLen);
    EVP_PKEY_CTX_free(rsaCtx);

    // 2. AES-GCM decrypt the payload
    EVP_CIPHER_CTX* aesCtx = EVP_CIPHER_CTX_new();
    if (!aesCtx) throw std::runtime_error("Failed to create AES context");

    if (EVP_DecryptInit_ex(aesCtx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(aesCtx);
        throw std::runtime_error("Failed to init AES-GCM decrypt");
    }

    if (EVP_DecryptInit_ex(aesCtx, nullptr, nullptr, aesKey.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(aesCtx);
        throw std::runtime_error("Failed to set AES key and IV");
    }

    std::vector<unsigned char> plaintext(aesCiphertext.size());
    int len;
    if (EVP_DecryptUpdate(aesCtx, plaintext.data(), &len, aesCiphertext.data(), aesCiphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(aesCtx);
        throw std::runtime_error("Failed during AES-GCM decrypt update");
    }
    int plaintextLen = len;

    // Set expected tag value
    if (EVP_CIPHER_CTX_ctrl(aesCtx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_BYTES, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(aesCtx);
        throw std::runtime_error("Failed to set expected AES-GCM tag");
    }

    int ret = EVP_DecryptFinal_ex(aesCtx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(aesCtx);
    
    if (ret <= 0) {
        throw std::runtime_error("AES-GCM Authentication failed (tampered ciphertext or tag)");
    }
    plaintextLen += len;
    plaintext.resize(plaintextLen);

    std::string jsonStr(plaintext.begin(), plaintext.end());
    return nlohmann::json::parse(jsonStr);
}

std::string HybridCryptoService::base64Encode(const std::vector<unsigned char>& data) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data.data(), data.size());
    BIO_flush(b64);
    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string b64Str(bptr->data, bptr->length);
    BIO_free_all(b64);
    return b64Str;
}

std::vector<unsigned char> HybridCryptoService::base64Decode(const std::string& b64) {
    BIO* b64Bio = BIO_new(BIO_f_base64());
    BIO_set_flags(b64Bio, BIO_FLAGS_BASE64_NO_NL);
    BIO* bmem = BIO_new_mem_buf(b64.data(), b64.length());
    bmem = BIO_push(b64Bio, bmem);
    
    std::vector<unsigned char> out(b64.length()); // decoded will be smaller
    int decodedLen = BIO_read(bmem, out.data(), b64.length());
    if (decodedLen < 0) {
        BIO_free_all(bmem);
        throw std::runtime_error("Failed to decode base64");
    }
    out.resize(decodedLen);
    BIO_free_all(bmem);
    return out;
}

std::string HybridCryptoService::hashCiphertext(const std::string& base64Ciphertext) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to create MD ctx");

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, base64Ciphertext.c_str(), base64Ciphertext.length()) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Failed to hash ciphertext");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &length) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize hash");
    }
    EVP_MD_CTX_free(ctx);

    std::string hexStr;
    char buf[3];
    for (unsigned int i = 0; i < length; i++) {
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        hexStr.append(buf);
    }
    return hexStr;
}

} // namespace crypto
} // namespace upimesh
