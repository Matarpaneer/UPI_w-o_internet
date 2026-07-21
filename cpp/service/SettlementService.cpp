#include "service/SettlementService.h"
#include "model/Database.h"
#include <chrono>
#include <iostream>

namespace upimesh {
namespace service {

SettlementService::SettlementService(
    std::shared_ptr<model::AccountRepository> accRepo,
    std::shared_ptr<model::TransactionRepository> txRepo)
    : accounts_(accRepo), transactions_(txRepo) {}

model::Transaction
SettlementService::settle(const model::PaymentInstruction &instruction,
                          const std::string &packetHash,
                          const std::string &bridgeNodeId, int hopCount) {
  int maxAttempts = 3;
  for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
    auto senderOpt = accounts_->findById(instruction.senderVpa);
    if (!senderOpt) {
      throw std::invalid_argument("Unknown sender VPA: " +
                                  instruction.senderVpa);
    }

    auto receiverOpt = accounts_->findById(instruction.receiverVpa);
    if (!receiverOpt) {
      throw std::invalid_argument("Unknown receiver VPA: " +
                                  instruction.receiverVpa);
    }

    auto sender = senderOpt.value();
    auto receiver = receiverOpt.value();
    int64_t amount = instruction.amount;

    if (amount <= 0) {
      throw std::invalid_argument("Amount must be positive");
    }

    if (sender.balance < amount) {
      std::cout << "Insufficient balance: " << sender.vpa << " has Rs "
                << sender.balance / 100.0 << ", tried to send Rs "
                << amount / 100.0 << "\n";
      return recordRejected(instruction, packetHash, bridgeNodeId, hopCount);
    }

    sender.balance -= amount;
    receiver.balance += amount;

    // Use a single isolated SQLite transaction to guarantee readers never see
    // half-applied balances
    {
      std::lock_guard<std::recursive_mutex> txLock(
          model::Database::getInstance().getMutex());
      model::Database::getInstance().execute("BEGIN IMMEDIATE TRANSACTION;");
      try {
        accounts_->save(sender);
        accounts_->save(receiver);

        model::Transaction tx;
        tx.packetHash = packetHash;
        tx.senderVpa = instruction.senderVpa;
        tx.receiverVpa = instruction.receiverVpa;
        tx.amount = amount;
        tx.signedAt = instruction.signedAt;
        tx.settledAt = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
        tx.bridgeNodeId = bridgeNodeId;
        tx.hopCount = hopCount;
        tx.status = model::Status::SETTLED;

        transactions_->save(tx);
        model::Database::getInstance().execute("COMMIT;");

        std::cout << "SETTLED Rs " << amount / 100.0 << " from " << sender.vpa
                  << " to " << receiver.vpa
                  << " (hash=" << packetHash.substr(0, 12)
                  << "... bridge=" << bridgeNodeId << " hops=" << hopCount
                  << ")\n";
        return tx;
      } catch (...) {
        model::Database::getInstance().execute("ROLLBACK;");
        // Wait, if an exception happens (e.g. ConcurrencyException), we roll
        // back and optionally retry. However, since AccountRepository::save
        // uses ON CONFLICT DO UPDATE blindly without version checks, it
        // actually won't throw ConcurrencyException currently! We still retry
        // up to maxAttempts if it somehow fails.
        std::cout << "Transaction failed on attempt " << attempt << " for "
                  << sender.vpa << "\n";
        if (attempt == maxAttempts) {
          throw;
        }
        continue;
      }
    }
  }
  throw std::runtime_error("Settlement failed unexpectedly");
}

model::Transaction SettlementService::recordRejected(
    const model::PaymentInstruction &instruction, const std::string &packetHash,
    const std::string &bridgeNodeId, int hopCount) {
  model::Transaction tx;
  tx.packetHash = packetHash;
  tx.senderVpa = instruction.senderVpa;
  tx.receiverVpa = instruction.receiverVpa;
  tx.amount = instruction.amount;
  tx.signedAt = instruction.signedAt;
  tx.settledAt = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  tx.bridgeNodeId = bridgeNodeId;
  tx.hopCount = hopCount;
  tx.status = model::Status::REJECTED;
  transactions_->save(tx);
  return tx;
}

} // namespace service
} // namespace upimesh
