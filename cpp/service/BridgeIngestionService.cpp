#include "service/BridgeIngestionService.h"
#include <chrono>
#include <iostream>

namespace upimesh {
namespace service {

BridgeIngestionService::BridgeIngestionService(
    std::shared_ptr<crypto::HybridCryptoService> crypto,
    std::shared_ptr<IdempotencyService> idempotency,
    std::shared_ptr<SettlementService> settlement)
    : crypto_(crypto), idempotency_(idempotency), settlement_(settlement) {}

IngestResult BridgeIngestionService::ingest(const model::MeshPacket &packet,
                                            const std::string &bridgeNodeId,
                                            int hopCount) {
  try {
    std::string packetHash = crypto_->hashCiphertext(packet.ciphertext);

    // ---- 1. Idempotency Gate ----
    if (!idempotency_->claim(packetHash)) {
      std::cout << "DUPLICATE packet " << packetHash.substr(0, 12)
                << "... from bridge " << bridgeNodeId << " - dropped\n";
      return {"DUPLICATE_DROPPED", packetHash, "", std::nullopt};
    }

    // ---- 2. Decrypt ----
    model::PaymentInstruction instruction;
    try {
      nlohmann::json j = crypto_->decrypt(packet.ciphertext);
      instruction = j.get<model::PaymentInstruction>();
    } catch (const std::exception &e) {
      std::cout << "Decryption failed for packet " << packetHash.substr(0, 12)
                << "...: " << e.what() << "\n";
      return {"INVALID", packetHash, "decryption_failed", std::nullopt};
    }

    // ---- 3. Freshness Check ----
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    int64_t ageSeconds = (now - instruction.signedAt) / 1000;

    if (ageSeconds > maxAgeSeconds_) {
      std::cout << "Packet " << packetHash.substr(0, 12) << "... too old ("
                << ageSeconds << "s), rejected\n";
      return {"INVALID", packetHash, "stale_packet", std::nullopt};
    }
    if (ageSeconds < -300) { // Small clock-skew tolerance
      return {"INVALID", packetHash, "future_dated", std::nullopt};
    }

    // ---- 4. Settle ----
    model::Transaction tx =
        settlement_->settle(instruction, packetHash, bridgeNodeId, hopCount);
    std::string outcome = model::statusToString(tx.status);
    return {outcome, packetHash, "", tx.id};

  } catch (const std::exception &e) {
    std::cout << "Ingestion error: " << e.what() << "\n";
    return {"INVALID", "?", std::string("internal_error: ") + e.what(),
            std::nullopt};
  }
}

} // namespace service
} // namespace upimesh
