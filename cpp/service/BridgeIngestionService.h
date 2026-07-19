#pragma once

#include "model/MeshPacket.h"
#include "model/Transaction.h"
#include "service/IdempotencyService.h"
#include "service/SettlementService.h"
#include "crypto/HybridCryptoService.h" // From Phase 1
#include <string>
#include <memory>
#include <optional>

namespace upimesh {
namespace service {

struct IngestResult {
    std::string outcome;
    std::string packetHash;
    std::string reason;
    int64_t transactionId = -1;
};

class BridgeIngestionService {
public:
    BridgeIngestionService(std::shared_ptr<crypto::HybridCryptoService> crypto,
                           std::shared_ptr<IdempotencyService> idempotency,
                           std::shared_ptr<SettlementService> settlement);

    IngestResult ingest(const model::MeshPacket& packet, const std::string& bridgeNodeId, int hopCount);

private:
    std::shared_ptr<crypto::HybridCryptoService> crypto_;
    std::shared_ptr<IdempotencyService> idempotency_;
    std::shared_ptr<SettlementService> settlement_;

    int64_t maxAgeSeconds_ = 86400; // 24 hours
};

} // namespace service
} // namespace upimesh
