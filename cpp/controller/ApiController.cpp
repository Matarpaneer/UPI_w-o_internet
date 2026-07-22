#include "controller/ApiController.h"
#include "controller/AppContext.h"
#include "model/Database.h"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <openssl/sha.h>
#include <sqlite3.h>
#include <sstream>
#include <uuid/uuid.h> // For simple packet ID generation

using namespace drogon;

namespace upimesh {
namespace controller {

static AppContext &getCtx() { return AppContext::getInstance(); }

static std::string generateUuid() {
  uuid_t out;
  uuid_generate(out);
  char buf[37];
  uuid_unparse_lower(out, buf);
  return std::string(buf);
}

static std::string sha256Hex(const std::string &input) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, input.c_str(), input.size());
  SHA256_Final(hash, &sha256);

  std::stringstream ss;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
  }
  return ss.str();
}

void ApiController::getServerKey(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto ctx = getCtx();
  Json::Value ret;
  ret["publicKey"] = ctx.serverKey->getPublicKeyBase64();
  ret["algorithm"] = "RSA-2048 / OAEP-SHA256";
  ret["hybridScheme"] = "RSA-OAEP encrypts an AES-256-GCM session key";

  auto resp = HttpResponse::newHttpJsonResponse(ret);
  callback(resp);
}

void ApiController::demoSend(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto jsonPtr = req->getJsonObject();
  if (!jsonPtr) {
    Json::Value err;
    err["error"] = "bad_request";
    err["message"] = "Invalid JSON";
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  if (!jsonPtr->isMember("senderVpa") || !jsonPtr->isMember("receiverVpa") ||
      !jsonPtr->isMember("amount") || !jsonPtr->isMember("pin")) {
    Json::Value err;
    err["error"] = "bad_request";
    err["message"] = "Missing required fields";
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
    pi.amount = static_cast<int64_t>(std::round(amountDecimal * 100.0));

    pi.pinHash = sha256Hex((*jsonPtr)["pin"].asString());
    pi.nonce = generateUuid();
    pi.signedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();

    // Convert to nlohmann::json to match our crypto boundary
    nlohmann::json piNlohmann = pi;
    std::string ciphertext =
        ctx.crypto->encrypt(piNlohmann, ctx.serverKey->getPublicKey());

    model::MeshPacket pkt;
    pkt.packetId = generateUuid();
    pkt.ttl = jsonPtr->isMember("ttl") ? (*jsonPtr)["ttl"].asInt() : 5;
    pkt.createdAt = pi.signedAt;
    pkt.ciphertext = ciphertext;

    std::string startDevice = jsonPtr->isMember("startDevice")
                                  ? (*jsonPtr)["startDevice"].asString()
                                  : "phone-alice";
    ctx.mesh->inject(startDevice, pkt);

    Json::Value ret;
    ret["packetId"] = pkt.packetId;
    ret["ciphertextPreview"] = pkt.ciphertext.substr(0, 64) + "...";
    ret["fullCiphertext"] = pkt.ciphertext; // Added for testing purposes
    ret["ttl"] = pkt.ttl;
    ret["injectedAt"] = startDevice;

    callback(HttpResponse::newHttpJsonResponse(ret));
  } catch (const std::exception &e) {
    Json::Value err;
    err["error"] = "internal_error";
    err["message"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
  }
}

void ApiController::meshState(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto ctx = getCtx();
  Json::Value ret;
  Json::Value devices(Json::arrayValue);

  for (const auto &d : ctx.mesh->getDevices()) {
    Json::Value dev;
    dev["deviceId"] = d->deviceId;
    dev["hasInternet"] = d->hasInternet;
    dev["packetCount"] = d->packetCount();
    Json::Value pids(Json::arrayValue);
    for (const auto &pkt : d->getHeldPackets()) {
      pids.append(pkt.packetId.substr(0, 8));
    }
    dev["packetIds"] = pids;
    devices.append(dev);
  }

  ret["devices"] = devices;
  ret["idempotencyCacheSize"] = ctx.idempotency->size();

  callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::meshGossip(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto ctx = getCtx();
  auto result = ctx.mesh->gossipOnce();

  Json::Value ret;
  ret["transfers"] = result.transfers;
  Json::Value counts;
  for (const auto &[k, v] : result.deviceCounts) {
    counts[k] = v;
  }
  ret["deviceCounts"] = counts;

  callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::meshFlush(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto ctx = getCtx();
  auto uploads = ctx.mesh->collectBridgeUploads();

  Json::Value results(Json::arrayValue);

  // Process serially for the demo mapping, though the actual idempotency
  // handles concurrent cleanly
  for (const auto &up : uploads) {
    int hopCount = 5 - up.packet.ttl;
    auto r = ctx.bridge->ingest(up.packet, up.bridgeNodeId, hopCount);

    Json::Value res;
    res["bridgeNode"] = up.bridgeNodeId;
    res["packetId"] = up.packet.packetId.substr(0, 8);
    res["outcome"] = r.outcome;
    res["reason"] = r.reason.empty() ? "" : r.reason;
    res["transactionId"] =
        r.transactionId.has_value()
            ? static_cast<Json::Int64>(r.transactionId.value())
            : -1;
    results.append(res);
  }

  Json::Value ret;
  ret["uploadsAttempted"] = static_cast<int>(uploads.size());
  ret["results"] = results;

  callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::meshReset(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto ctx = getCtx();
  ctx.mesh->resetMesh();
  ctx.idempotency->clear();
  model::Database::getInstance().reset();

  Json::Value ret;
  ret["status"] = "mesh and idempotency cache cleared";
  callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::bridgeIngest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto jsonPtr = req->getJsonObject();
  if (!jsonPtr) {
    Json::Value err;
    err["error"] = "bad_request";
    err["message"] = "Invalid JSON";
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  if (!jsonPtr->isMember("packetId") || !jsonPtr->isMember("ttl") ||
      !jsonPtr->isMember("createdAt") || !jsonPtr->isMember("ciphertext")) {
    Json::Value err;
    err["error"] = "bad_request";
    err["message"] = "Missing required fields";
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  std::string bridgeNodeId = req->getHeader("X-Bridge-Node-Id");
  if (bridgeNodeId.empty())
    bridgeNodeId = "unknown";

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
  if (r.transactionId.has_value()) {
    ret["transactionId"] = static_cast<Json::Int64>(r.transactionId.value());
  } else {
    ret["transactionId"] = Json::Value::null;
  }

  callback(HttpResponse::newHttpJsonResponse(ret));
}

void ApiController::listAccounts(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto &ctx = AppContext::getInstance();
  auto accounts = ctx.accountRepo->findAll();

  nlohmann::json arr = nlohmann::json::array();
  for (const auto &acc : accounts) {
    nlohmann::json j;
    upimesh::to_json(j, acc);
    arr.push_back(j);
  }

  auto resp = HttpResponse::newHttpResponse();
  resp->setBody(arr.dump());
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  callback(resp);
}

void ApiController::listTransactions(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto &ctx = AppContext::getInstance();
  auto txs = ctx.txRepo->findTop20ByOrderByIdDesc();

  nlohmann::json arr = nlohmann::json::array();
  for (const auto &tx : txs) {
    nlohmann::json j;
    upimesh::to_json(j, tx);
    arr.push_back(j);
  }

  auto resp = HttpResponse::newHttpResponse();
  resp->setBody(arr.dump());
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  callback(resp);
}

} // namespace controller
} // namespace upimesh
