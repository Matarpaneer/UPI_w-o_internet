#include "service/SettlementService.h"
#include <iostream>
#include <chrono>

namespace upimesh {
namespace service {

SettlementService::SettlementService(std::shared_ptr<model::AccountRepository> accRepo, 
                                     std::shared_ptr<model::TransactionRepository> txRepo)
    : accounts_(accRepo), transactions_(txRepo) {}

model::Transaction SettlementService::settle(const model::PaymentInstruction& instruction, 
                                             const std::string& packetHash,
                                             const std::string& bridgeNodeId, 
                                             int hopCount) {
    int maxAttempts = 3;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        auto senderOpt = accounts_->findById(instruction.senderVpa);
        if (!senderOpt) {
            throw std::invalid_argument("Unknown sender VPA: " + instruction.senderVpa);
        }

        auto receiverOpt = accounts_->findById(instruction.receiverVpa);
        if (!receiverOpt) {
            throw std::invalid_argument("Unknown receiver VPA: " + instruction.receiverVpa);
        }

        auto sender = senderOpt.value();
        auto receiver = receiverOpt.value();
        int64_t amount = instruction.amount;

        if (amount <= 0) {
            throw std::invalid_argument("Amount must be positive");
        }

        if (sender.balance < amount) {
            std::cout << "Insufficient balance: " << sender.vpa << " has Rs " << sender.balance/100.0 
                      << ", tried to send Rs " << amount/100.0 << "\n";
            return recordRejected(instruction, packetHash, bridgeNodeId, hopCount);
        }

        sender.balance -= amount;
        receiver.balance += amount;

        try {
            // Because our repository uses a shared SQLite connection without explicit 
            // multi-statement transaction management yet, we save sequentially.
            // In a true clustered DB, this needs a transaction wrapper.
            accounts_->save(sender);
            accounts_->save(receiver);
        } catch (const model::ConcurrencyException& e) {
            std::cout << "Optimistic lock failed on attempt " << attempt << " for " << sender.vpa << "\n";
            if (attempt == maxAttempts) {
                // Exhausted retries, bubble up failure
                throw; 
            }
            // Continue to next attempt to re-read and retry
            continue;
        }

        model::Transaction tx;
        tx.packetHash = packetHash;
        tx.senderVpa = instruction.senderVpa;
        tx.receiverVpa = instruction.receiverVpa;
        tx.amount = amount;
        tx.signedAt = instruction.signedAt;
        tx.settledAt = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        tx.bridgeNodeId = bridgeNodeId;
        tx.hopCount = hopCount;
        tx.status = model::Status::SETTLED;
        
        transactions_->save(tx);

        std::cout << "SETTLED Rs " << amount/100.0 << " from " << sender.vpa 
                  << " to " << receiver.vpa << " (hash=" << packetHash.substr(0,12) 
                  << "... bridge=" << bridgeNodeId << " hops=" << hopCount << ")\n";

        return tx;
    }
    throw std::runtime_error("Settlement failed unexpectedly");
}

model::Transaction SettlementService::recordRejected(const model::PaymentInstruction& instruction, 
                                                     const std::string& packetHash,
                                                     const std::string& bridgeNodeId, 
                                                     int hopCount) {
    model::Transaction tx;
    tx.packetHash = packetHash;
    tx.senderVpa = instruction.senderVpa;
    tx.receiverVpa = instruction.receiverVpa;
    tx.amount = instruction.amount;
    tx.signedAt = instruction.signedAt;
    tx.settledAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    tx.bridgeNodeId = bridgeNodeId;
    tx.hopCount = hopCount;
    tx.status = model::Status::REJECTED;
    transactions_->save(tx);
    return tx;
}

} // namespace service
} // namespace upimesh
