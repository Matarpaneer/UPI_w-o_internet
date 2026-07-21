#pragma once

#include <cmath>
#include <nlohmann/json.hpp>
#include <string>

namespace upimesh {
namespace model {

struct PaymentInstruction {
  std::string senderVpa;
  std::string receiverVpa;
  int64_t amount; // Internally represented in paise to avoid precision loss
  std::string pinHash;
  std::string nonce;
  int64_t signedAt; // epoch millis

  // Equality operator for verification checks
  bool operator==(const PaymentInstruction &o) const {
    return senderVpa == o.senderVpa && receiverVpa == o.receiverVpa &&
           amount == o.amount && pinHash == o.pinHash && nonce == o.nonce &&
           signedAt == o.signedAt;
  }
};

// Custom JSON serialization boundary
inline void to_json(nlohmann::json &j, const PaymentInstruction &p) {
  j = nlohmann::json{{"senderVpa", p.senderVpa},
                     {"receiverVpa", p.receiverVpa},
                     // Convert internal int64_t (paise) -> JSON double (rupees)
                     {"amount", static_cast<double>(p.amount) / 100.0},
                     {"pinHash", p.pinHash},
                     {"nonce", p.nonce},
                     {"signedAt", p.signedAt}};
}

inline void from_json(const nlohmann::json &j, PaymentInstruction &p) {
  j.at("senderVpa").get_to(p.senderVpa);
  j.at("receiverVpa").get_to(p.receiverVpa);

  // Convert JSON double (rupees) -> internal int64_t (paise)
  double amtDecimal;
  j.at("amount").get_to(amtDecimal);
  p.amount = static_cast<int64_t>(std::round(amtDecimal * 100.0));

  j.at("pinHash").get_to(p.pinHash);
  j.at("nonce").get_to(p.nonce);
  j.at("signedAt").get_to(p.signedAt);
}

} // namespace model
} // namespace upimesh
