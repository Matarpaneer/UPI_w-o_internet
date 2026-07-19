#include "service/IdempotencyService.h"
#include "service/SettlementService.h"
#include "model/Database.h"
#include "model/AccountRepository.h"
#include "model/TransactionRepository.h"

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>

using namespace upimesh;

void testIdempotencyConcurrency() {
    std::cout << "1. Idempotency Concurrency Test (100 threads, 1 hash)\n";
    service::IdempotencyService idempotency;
    std::string testHash = "hash_race_condition_test_123456";

    std::atomic<int> successCount{0};
    std::atomic<int> failCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 100; ++i) {
        threads.emplace_back([&]() {
            if (idempotency.claim(testHash)) {
                successCount++;
            } else {
                failCount++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "   - Success (Claimed): " << successCount << "\n";
    std::cout << "   - Failed (Duplicate dropped): " << failCount << "\n";
    if (successCount == 1 && failCount == 99) {
        std::cout << "   - [PASS] Exactly one thread claimed the hash.\n";
    } else {
        std::cout << "   - [FAIL] Idempotency violation!\n";
    }
}

void testSettlementConcurrency() {
    std::cout << "\n2. Settlement Optimistic Lock Retry Test (10 concurrent Txns on same account)\n";
    
    auto accRepo = std::make_shared<model::AccountRepository>();
    auto txRepo = std::make_shared<model::TransactionRepository>();
    service::SettlementService settlement(accRepo, txRepo);

    // Initial state check
    int64_t initialAliceBalance = accRepo->findById("alice@demo")->balance;
    int64_t initialBobBalance = accRepo->findById("bob@demo")->balance;
    
    std::vector<std::thread> threads;
    std::atomic<int> completedTxns{0};
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            model::PaymentInstruction pi;
            pi.senderVpa = "alice@demo";
            pi.receiverVpa = "bob@demo";
            pi.amount = 100; // 1 rupee
            pi.signedAt = 1000;
            
            std::string hash = "hash_multi_txn_" + std::to_string(i);
            try {
                settlement.settle(pi, hash, "bridge-1", 1);
                completedTxns++;
            } catch (const std::exception& e) {
                std::cout << "Thread " << i << " failed settlement: " << e.what() << "\n";
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    int64_t finalAliceBalance = accRepo->findById("alice@demo")->balance;
    int64_t finalBobBalance = accRepo->findById("bob@demo")->balance;
    
    std::cout << "   - Completed Settlements: " << completedTxns << "/10\n";
    std::cout << "   - Alice balance delta: " << (initialAliceBalance - finalAliceBalance) << " (Expected 1000)\n";
    std::cout << "   - Bob balance delta: " << (finalBobBalance - initialBobBalance) << " (Expected 1000)\n";

    if (completedTxns == 10 && (initialAliceBalance - finalAliceBalance == 1000)) {
        std::cout << "   - [PASS] Bounded retry successfully bypassed optimistic lock races!\n";
    } else {
        std::cout << "   - [FAIL] Lost updates or failed settlements detected.\n";
    }
}

int main() {
    std::cout << "--- Phase 3 Concurrency Verification ---\n\n";
    // Force DB instantiation
    model::Database::getInstance();
    
    testIdempotencyConcurrency();
    testSettlementConcurrency();
    
    std::cout << "\n--- End Verification ---\n";
    return 0;
}
