#include "model/PaymentInstruction.h"
#include <iostream>

using namespace upimesh::model;

void testAmount(int64_t paise) {
  PaymentInstruction pi;
  pi.senderVpa = "alice@demo";
  pi.receiverVpa = "bob@demo";
  pi.amount = paise;
  pi.pinHash = "abc";
  pi.nonce = "123";
  pi.signedAt = 1000;

  nlohmann::json j = pi;
  std::cout << "Paise: " << paise << "\n  -> JSON dump: " << j.dump() << "\n\n";
}

int main() {
  std::cout << "--- Testing Amount JSON Serialization (Before Fix) ---\n\n";
  testAmount(500000);
  testAmount(123456);
  testAmount(50);
  testAmount(999999999);
  return 0;
}
