#include "controller/AppContext.h"
#include "model/Database.h"
#include <drogon/drogon.h>

using namespace upimesh;

int main() {
  // 1. Force Database initialization and seed demo data
  model::Database::getInstance();

  // 2. Initialize Repositories
  auto accRepo = std::make_shared<model::AccountRepository>();
  auto txRepo = std::make_shared<model::TransactionRepository>();

  // 3. Initialize Crypto Services
  auto serverKey = std::make_shared<crypto::ServerKeyHolder>();
  auto cryptoSvc = std::make_shared<crypto::HybridCryptoService>(serverKey);

  // 4. Initialize Business Logic Services
  auto idempotency = std::make_shared<service::IdempotencyService>();
  auto settlement =
      std::make_shared<service::SettlementService>(accRepo, txRepo);
  auto bridge = std::make_shared<service::BridgeIngestionService>(
      cryptoSvc, idempotency, settlement);
  auto mesh = std::make_shared<service::MeshSimulatorService>();

  // 5. Populate AppContext Singleton
  auto &ctx = AppContext::getInstance();
  ctx.serverKey = serverKey;
  ctx.crypto = cryptoSvc;
  ctx.accountRepo = accRepo;
  ctx.txRepo = txRepo;
  ctx.idempotency = idempotency;
  ctx.settlement = settlement;
  ctx.bridge = bridge;
  ctx.mesh = mesh;

  // 6. CORS support for future front-end
  drogon::app().registerPostHandlingAdvice(
      [](const drogon::HttpRequestPtr &req,
         const drogon::HttpResponsePtr &resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers",
                        "Content-Type, X-Bridge-Node-Id, X-Hop-Count");
      });

  // 8. Start the Drogon Web Server
  std::cout << "Starting UPIMesh C++ Backend on http://0.0.0.0:8080 ...\n";
  drogon::app().addListener("0.0.0.0", 8080).setThreadNum(16).run();

  return 0;
}
