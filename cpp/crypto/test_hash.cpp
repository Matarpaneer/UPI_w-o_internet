#include "crypto/HybridCryptoService.h"
#include <iostream>
#include <openssl/evp.h>
#include <string>

using namespace upimesh::crypto;

// The exact manual approach from Phase 1
std::string manualHash(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, input.c_str(), input.length());
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    EVP_DigestFinal_ex(ctx, hash, &length);
    EVP_MD_CTX_free(ctx);

    std::string hexStr;
    char buf[3];
    for (unsigned int i = 0; i < length; i++) {
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        hexStr.append(buf);
    }
    return hexStr;
}

int main() {
    std::shared_ptr<ServerKeyHolder> keyHolder = std::make_shared<ServerKeyHolder>();
    HybridCryptoService crypto(keyHolder);
    
    std::string testInput = "dummy_base64_ciphertext_for_testing";
    
    std::string serviceOutput = crypto.hashCiphertext(testInput);
    std::string manualOutput = manualHash(testInput);
    
    std::cout << "Service Hash: " << serviceOutput << "\n";
    std::cout << "Manual Hash:  " << manualOutput << "\n";
    
    if (serviceOutput == manualOutput) {
        std::cout << "Result: EXACT MATCH\n";
    } else {
        std::cout << "Result: MISMATCH\n";
    }
    return 0;
}
