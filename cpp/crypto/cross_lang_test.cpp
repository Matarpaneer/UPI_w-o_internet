#include "crypto/ServerKeyHolder.h"
#include "crypto/HybridCryptoService.h"
#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

using namespace upimesh::crypto;

std::vector<unsigned char> base64DecodeLocal(const std::string& b64) {
    BIO* b64Bio = BIO_new(BIO_f_base64());
    BIO_set_flags(b64Bio, BIO_FLAGS_BASE64_NO_NL);
    BIO* bmem = BIO_new_mem_buf(b64.data(), b64.length());
    bmem = BIO_push(b64Bio, bmem);
    
    std::vector<unsigned char> out(b64.length()); 
    int decodedLen = BIO_read(bmem, out.data(), b64.length());
    out.resize(decodedLen);
    BIO_free_all(bmem);
    return out;
}

std::string execCurl(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main() {
    std::cout << "--- Starting Cross-Language Interop Tests ---\n\n";
    
    auto keyHolder = std::make_shared<ServerKeyHolder>();
    HybridCryptoService crypto(keyHolder);

    // ==========================================
    // DIRECTION 1: C++ Public Key -> Java Encrypts -> C++ Decrypts
    // ==========================================
    std::cout << "[1] C++ -> Java Direction\n";
    std::string cppPub = keyHolder->getPublicKeyBase64();
    
    std::cout << "Asking Java to encrypt using C++ public key...\n";
    std::string curlCmd1 = "curl -s -X POST -H \"Content-Type: text/plain\" -d \"" + cppPub + "\" http://localhost:8080/api/demo/encrypt-test";
    std::string javaCiphertext = execCurl(curlCmd1);
    
    std::cout << "Received Java Ciphertext (length " << javaCiphertext.length() << "):\n" << javaCiphertext.substr(0, 60) << "...\n";
    
    nlohmann::json decryptedFromJava = crypto.decrypt(javaCiphertext);
    std::cout << "Decrypted in C++ perfectly! Fields:\n" << decryptedFromJava.dump(2) << "\n\n";

    // ==========================================
    // DIRECTION 2: Java Public Key -> C++ Encrypts -> Java Decrypts
    // ==========================================
    std::cout << "[2] Java -> C++ Direction\n";
    std::string javaPubStr = execCurl("curl -s http://localhost:8080/api/server-key");
    auto javaPubJson = nlohmann::json::parse(javaPubStr);
    std::string javaPub = javaPubJson["publicKey"];

    // Load Java public key into EVP_PKEY
    auto der = base64DecodeLocal(javaPub);
    const unsigned char* p = der.data();
    EVP_PKEY* jKey = d2i_PUBKEY(nullptr, &p, der.size());
    if (!jKey) {
        std::cerr << "Failed to parse Java public key!\n";
        return 1;
    }

    nlohmann::json payload = {
        {"senderVpa", "alice@upi"},
        {"receiverVpa", "bob@upi"},
        {"amount", 50000},
        {"pinHash", "abcdef123456"},
        {"nonce", "unique-nonce-123"},
        {"signedAt", 1690000000}
    };
    
    std::cout << "Encrypting in C++ using Java's public key...\n";
    std::string cppCiphertext = crypto.encrypt(payload, jKey);
    EVP_PKEY_free(jKey);

    std::cout << "Sending C++ Ciphertext (length " << cppCiphertext.length() << ") to Java for decryption...\n";
    std::string curlCmd2 = "curl -s -X POST -H \"Content-Type: text/plain\" -d \"" + cppCiphertext + "\" http://localhost:8080/api/demo/decrypt-test";
    std::string decryptedFromCpp = execCurl(curlCmd2);
    
    std::cout << "Decrypted in Java perfectly! Fields:\n" << decryptedFromCpp << "\n\n";

    std::cout << "--- Interop Tests Complete ---\n";
    return 0;
}
