#include "controller/ApiController.h"
#include "controller/AppContext.h"
#include "model/Database.h"
#include <sqlite3.h>
#include <chrono>
#include <uuid/uuid.h> // For simple packet ID generation

using namespace drogon;

namespace upimesh {
namespace controller {

static AppContext& getCtx() {
    return AppContext::getInstance();
}

static std::string generateUuid() {
    uuid_t out;
    uuid_generate(out);
    char buf[37];
    uuid_unparse_lower(out, buf);
    return std::string(buf);
}

void ApiController::getServerKey(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    auto ctx = getCtx();
    Json::Value ret;
    ret["publicKey"] = ctx.serverKey->getPublicKeyBase64();
    ret["algorithm"] = "RSA-2048 / OAEP-SHA256";
    ret["hybridScheme"] = "RSA-OAEP encrypts an AES-256-GCM session key";
    
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);
}

void ApiController::demoSend(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
        Json::Value err; err["error"] = "bad_request"; err["message"] = "Invalid JSON";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }
    
    auto ctx = getCtx();
    try {
        model::PaymentInstruction pi;
        pi.senderVpa = (*jsonPtr)["senderVpa"].asString();
        pi.receiverVpa = (*jsonPtr)["receiverVpa"].asString();
        
        // Match JSON double -> int64 paise boundary
        double amountDecimal = (*jsonPtr)["amount"].asDouble();
        pi.amount = static_cast<int64_t>(amountDecimal * 100.0);
        
        pi.pinHash = (*jsonPtr)["pin"].asString(); // Simplified for demo
        pi.nonce = generateUuid();
        pi.signedAt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Convert to nlohmann::json to match our crypto boundary
        nlohmann::json piNlohmann = pi;
        std::string ciphertext = ctx.crypto->encrypt(piNlohmann, ctx.serverKey->getPublicKey());
        
        model::MeshPacket pkt;
        pkt.packetId = generateUuid();
        pkt.ttl = jsonPtr->isMember("ttl") ? (*jsonPtr)["ttl"].asInt() : 5;
        pkt.createdAt = pi.signedAt;
        pkt.ciphertext = ciphertext;
        
        std::string startDevice = jsonPtr->isMember("startDevice") ? (*jsonPtr)["startDevice"].asString() : "phone-alice";
        ctx.mesh->inject(startDevice, pkt);
        
        Json::Value ret;
        ret["packetId"] = pkt.packetId;
        ret["ciphertextPreview"] = pkt.ciphertext.substr(0, 64) + "...";
        ret["fullCiphertext"] = pkt.ciphertext; // Added for testing purposes
        ret["ttl"] = pkt.ttl;
        ret["injectedAt"] = startDevice;
        
        callback(HttpResponse::newHttpJsonResponse(ret));
    } catch (const std::exception& e) {
        Json::Value err; err["error"] = "internal_error"; err["message"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void ApiController::meshState(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    auto ctx = getCtx();
    Json::Value ret;
    Json::Value devices(Json::arrayValue);
    
    for (const auto& d : ctx.mesh->getDevices()) {
        Json::Value dev;
        dev["deviceId"] = d->deviceId;
        dev["hasInternet"] = d->hasInternet;
        dev["packetCount"] = d->packetCount();
        Json::Value pids(Json::arrayValue);
        for (const auto& pkt : d->getHeldPackets()) {
            pids.append(pkt.packetId.substr(0, 8));
        }
        dev["packetIds"] = pids;
        devices.append(dev);
    }
    
    ret["devices"] = devices;
    ret["idempotencyCacheSize"] = ctx.idempotency->size();
    
    callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::meshGossip(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    auto ctx = getCtx();
    auto result = ctx.mesh->gossipOnce();
    
    Json::Value ret;
    ret["transfers"] = result.transfers;
    Json::Value counts;
    for (const auto& [k, v] : result.deviceCounts) {
        counts[k] = v;
    }
    ret["deviceCounts"] = counts;
    
    callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::meshFlush(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    auto ctx = getCtx();
    auto uploads = ctx.mesh->collectBridgeUploads();
    
    Json::Value results(Json::arrayValue);
    
    // Process serially for the demo mapping, though the actual idempotency handles concurrent cleanly
    for (const auto& up : uploads) {
        int hopCount = 5 - up.packet.ttl;
        auto r = ctx.bridge->ingest(up.packet, up.bridgeNodeId, hopCount);
        
        Json::Value res;
        res["bridgeNode"] = up.bridgeNodeId;
        res["packetId"] = up.packet.packetId.substr(0, 8);
        res["outcome"] = r.outcome;
        res["reason"] = r.reason.empty() ? "" : r.reason;
        res["transactionId"] = r.transactionId;
        results.append(res);
    }
    
    Json::Value ret;
    ret["uploadsAttempted"] = static_cast<int>(uploads.size());
    ret["results"] = results;
    
    callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::meshReset(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    auto ctx = getCtx();
    ctx.mesh->resetMesh();
    ctx.idempotency->clear();
    
    Json::Value ret;
    ret["status"] = "mesh and idempotency cache cleared";
    callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::bridgeIngest(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
        Json::Value err; err["error"] = "bad_request"; err["message"] = "Invalid JSON";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }
    
    std::string bridgeNodeId = req->getHeader("X-Bridge-Node-Id");
    if (bridgeNodeId.empty()) bridgeNodeId = "unknown";
    
    std::string hopCountStr = req->getHeader("X-Hop-Count");
    int hopCount = hopCountStr.empty() ? 0 : std::stoi(hopCountStr);
    
    model::MeshPacket pkt;
    pkt.packetId = (*jsonPtr)["packetId"].asString();
    pkt.ttl = (*jsonPtr)["ttl"].asInt();
    pkt.createdAt = (*jsonPtr)["createdAt"].asInt64();
    pkt.ciphertext = (*jsonPtr)["ciphertext"].asString();
    
    auto ctx = getCtx();
    auto r = ctx.bridge->ingest(pkt, bridgeNodeId, hopCount);
    
    Json::Value ret;
    ret["outcome"] = r.outcome;
    ret["packetHash"] = r.packetHash;
    ret["reason"] = r.reason.empty() ? "" : r.reason;
    ret["transactionId"] = r.transactionId;
    
    callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::listAccounts(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    auto ctx = getCtx();
    
    // We don't have a findAll in AccountRepo, let's just loop over known demo VPAs or implement it
    // Wait, in Java we did. I'll just query the SQLite database directly here for the demo since it's a simple list
    sqlite3* db = model::Database::getInstance().getConnection();
    std::lock_guard<std::mutex> lock(model::Database::getInstance().getMutex());
    
    Json::Value arr(Json::arrayValue);
    const char* sql = "SELECT vpa, holderName, balance, version FROM accounts";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Json::Value acc;
            acc["vpa"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            acc["holderName"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            int64_t paise = sqlite3_column_int64(stmt, 2);
            acc["balance"] = static_cast<double>(paise) / 100.0;
            acc["version"] = sqlite3_column_int64(stmt, 3);
            arr.append(acc);
        }
    }
    sqlite3_finalize(stmt);
    
    callback(HttpResponse::newHttpJsonResponse(arr));
}

void ApiController::listTransactions(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    auto ctx = getCtx();
    auto txs = ctx.txRepo->findTop20ByOrderByIdDesc();
    
    Json::Value arr(Json::arrayValue);
    for (const auto& tx : txs) {
        Json::Value j;
        j["id"] = tx.id;
        j["packetHash"] = tx.packetHash;
        j["senderVpa"] = tx.senderVpa;
        j["receiverVpa"] = tx.receiverVpa;
        j["amount"] = static_cast<double>(tx.amount) / 100.0;
        j["signedAt"] = tx.signedAt;
        j["settledAt"] = tx.settledAt;
        j["bridgeNodeId"] = tx.bridgeNodeId;
        j["hopCount"] = tx.hopCount;
        j["status"] = model::statusToString(tx.status);
        arr.append(j);
    }
    
    callback(HttpResponse::newHttpJsonResponse(arr));
}

} // namespace controller
} // namespace upimesh
