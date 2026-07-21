#pragma once

#include "model/Transaction.h"
#include <vector>

namespace upimesh {
namespace model {

class TransactionRepository {
public:
  // Saves a new transaction. Returns the generated auto-increment ID.
  // Throws a runtime_error (constraint violation) if the packetHash already
  // exists.
  int64_t save(Transaction &tx);

  // Defense-in-depth: Checks if a transaction with the given packetHash already
  // exists.
  bool existsByPacketHash(const std::string &packetHash);

  // Returns the most recent 20 transactions
  std::vector<Transaction> findTop20ByOrderByIdDesc();
};

} // namespace model
} // namespace upimesh
