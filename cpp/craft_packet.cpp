#include "crypto/HybridCryptoService.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <string>
#include <vector>

std::vector<unsigned char> base64DecodeEx(const std::string &b64) {
  BIO *b64Bio = BIO_new(BIO_f_base64());
  BIO_set_flags(b64Bio, BIO_FLAGS_BASE64_NO_NL);
  BIO *bmem = BIO_new_mem_buf(b64.data(), b64.length());
  bmem = BIO_push(b64Bio, bmem);
  std::vector<unsigned char> out(b64.length());
  int len = BIO_read(bmem, out.data(), b64.length());
  out.resize(len);
  BIO_free_all(bmem);
  return out;
}

int main(int argc, char **argv) {
  if (argc != 3)
    return 1;
  std::string pkB64 = argv[1];
  long signedAt = std::stol(argv[2]);

  auto der = base64DecodeEx(pkB64);
  const unsigned char *p = der.data();
  EVP_PKEY *pkey = d2i_PUBKEY(nullptr, &p, der.size());
  if (!pkey) {
    std::cerr << "Failed to parse public key" << std::endl;
    return 1;
  }

  upimesh::crypto::HybridCryptoService crypto(nullptr);

  nlohmann::json j;
  j["senderVpa"] = "alice@demo";
  j["receiverVpa"] = "bob@demo";
  j["amount"] = 250;
  j["pinHash"] =
      "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92";
  j["nonce"] = "direct-test-uuid-" + std::to_string(signedAt);
  j["ttl"] = 5;
  j["signedAt"] = signedAt;

  try {
    std::string ctx = crypto.encrypt(j, pkey);
    std::cout << ctx << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Encryption failed: " << e.what() << std::endl;
    EVP_PKEY_free(pkey);
    return 1;
  }
  EVP_PKEY_free(pkey);
  return 0;
}
