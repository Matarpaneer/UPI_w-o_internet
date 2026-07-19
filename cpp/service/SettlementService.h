#pragma once

#include "model/PaymentInstruction.h"
#include "model/Transaction.h"
#include "model/AccountRepository.h"
#include "model/TransactionRepository.h"
#include <string>
#include <stdexcept>
#include <memory>

namespace upimesh {
namespace service {

class SettlementService {
public:
    SettlementService(std::shared_ptr<model::AccountRepository> accRepo, 
                      std::shared_ptr<model::TransactionRepository> txRepo);

    // Settles the payment.
    // If the sender or receiver is unknown, throws std::invalid_argument.
    // Writes a SETTLED or REJECTED Transaction record.
    // Implements bounded retry for optimistic lock failures.
    model::Transaction settle(const model::PaymentInstruction& instruction, 
                              const std::string& packetHash,
                              const std::string& bridgeNodeId, 
                              int hopCount);

private:
    std::shared_ptr<model::AccountRepository> accounts_;
    std::shared_ptr<model::TransactionRepository> transactions_;

    model::Transaction recordRejected(const model::PaymentInstruction& instruction, 
                                      const std::string& packetHash,
                                      const std::string& bridgeNodeId, 
                                      int hopCount);
};

} // namespace service
} // namespace upimesh
