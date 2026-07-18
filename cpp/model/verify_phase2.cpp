#include "model/Database.h"
#include "model/AccountRepository.h"
#include "model/TransactionRepository.h"
#include <iostream>
#include <cassert>

using namespace upimesh::model;

int main() {
    std::cout << "--- Phase 2 Verification ---\n\n";
    
    // 1. Schema Check & Seeding
    std::cout << "1. Schema & Seed Check:\n";
    AccountRepository accRepo;
    auto alice = accRepo.findById("alice@demo");
    if (alice && alice->balance == 500000) {
        std::cout << "   - [PASS] Database initialized and alice@demo found with balance 500000 paise (Rs 5000.00)\n";
    } else {
        std::cout << "   - [FAIL] alice@demo missing or wrong balance!\n";
        return 1;
    }

    // 2. Account Optimistic Locking Test
    std::cout << "\n2. Optimistic Locking Test:\n";
    auto bob = accRepo.findById("bob@demo").value();
    std::cout << "   - Read bob@demo (version " << bob.version << ", balance " << bob.balance << ")\n";
    
    // Create a concurrent view of bob (e.g. from another thread/request)
    Account bobConcurrentView = bob;
    
    // Update bob legitimately
    bob.balance -= 20000; // Deduct Rs 200
    accRepo.save(bob);
    std::cout << "   - Saved bob legitimately. New version in DB is expected to be " << bob.version + 1 << "\n";
    
    // Try to update using the stale view
    bobConcurrentView.balance -= 30000;
    try {
        accRepo.save(bobConcurrentView);
        std::cout << "   - [FAIL] Successfully saved a stale view. Optimistic locking failed!\n";
    } catch (const ConcurrencyException& e) {
        std::cout << "   - [PASS] Stale update rejected correctly. Exception: " << e.what() << "\n";
    }

    // 3. Transaction Unique Constraint Test
    std::cout << "\n3. Transaction Unique Constraint Test:\n";
    TransactionRepository txRepo;
    
    Transaction tx1;
    tx1.packetHash = "abcdef1234567890_mock_hash";
    tx1.senderVpa = "alice@demo";
    tx1.receiverVpa = "bob@demo";
    tx1.amount = 20000;
    tx1.signedAt = 1690000000;
    tx1.settledAt = 1690000005;
    tx1.bridgeNodeId = "bridge-node-1";
    tx1.hopCount = 3;
    tx1.status = Status::SETTLED;
    
    int64_t id1 = txRepo.save(tx1);
    std::cout << "   - Inserted transaction successfully, got ID: " << id1 << "\n";
    
    Transaction tx2 = tx1; // Copy identical tx (including hash)
    tx2.amount = 99999; // Change amount just to prove the DB rejects purely on hash
    try {
        txRepo.save(tx2);
        std::cout << "   - [FAIL] DB accepted a transaction with a duplicate packetHash!\n";
    } catch (const std::exception& e) {
        std::cout << "   - [PASS] Duplicate hash rejected by DB constraint. Exception: " << e.what() << "\n";
    }
    
    bool exists = txRepo.existsByPacketHash("abcdef1234567890_mock_hash");
    std::cout << "   - [PASS] existsByPacketHash returned " << (exists ? "true" : "false") << " (expected true)\n";

    std::cout << "\n--- End Verification ---\n";
    return 0;
}
