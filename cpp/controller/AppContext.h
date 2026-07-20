#pragma once

#include "crypto/ServerKeyHolder.h"
#include "crypto/HybridCryptoService.h"
#include "model/AccountRepository.h"
#include "model/TransactionRepository.h"
#include "service/IdempotencyService.h"
#include "service/SettlementService.h"
#include "service/BridgeIngestionService.h"
#include "service/MeshSimulatorService.h"
#include <memory>

namespace upimesh {
struct AppContext {
    static AppContext& getInstance() {
        static AppContext instance;
        return instance;
    }

    std::shared_ptr<crypto::ServerKeyHolder> serverKey;
    std::shared_ptr<crypto::HybridCryptoService> crypto;
    std::shared_ptr<model::AccountRepository> accountRepo;
    std::shared_ptr<model::TransactionRepository> txRepo;
    std::shared_ptr<service::IdempotencyService> idempotency;
    std::shared_ptr<service::SettlementService> settlement;
    std::shared_ptr<service::BridgeIngestionService> bridge;
    std::shared_ptr<service::MeshSimulatorService> mesh;
};
} // namespace upimesh
