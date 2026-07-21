#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace upimesh {
namespace model {

struct Account {
  std::string vpa; // PK
  std::string holderName;
  int64_t balance; // paise
  int64_t version; // optimistic locking
};

} // namespace model

inline void to_json(nlohmann::json &j, const model::Account &a) {
  j = nlohmann::json{{"vpa", a.vpa},
                     {"holderName", a.holderName},
                     {"balance", static_cast<double>(a.balance) / 100.0},
                     {"version", a.version}};
}

} // namespace upimesh
