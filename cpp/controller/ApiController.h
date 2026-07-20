#pragma once
#include <drogon/HttpController.h>

namespace upimesh {
namespace controller {

class ApiController : public drogon::HttpController<ApiController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ApiController::getServerKey, "/api/server-key", drogon::Get);
    ADD_METHOD_TO(ApiController::demoSend, "/api/demo/send", drogon::Post);
    ADD_METHOD_TO(ApiController::meshState, "/api/mesh/state", drogon::Get);
    ADD_METHOD_TO(ApiController::meshGossip, "/api/mesh/gossip", drogon::Post);
    ADD_METHOD_TO(ApiController::meshFlush, "/api/mesh/flush", drogon::Post);
    ADD_METHOD_TO(ApiController::meshReset, "/api/mesh/reset", drogon::Post);
    ADD_METHOD_TO(ApiController::bridgeIngest, "/api/bridge/ingest", drogon::Post);
    ADD_METHOD_TO(ApiController::listAccounts, "/api/accounts", drogon::Get);
    ADD_METHOD_TO(ApiController::listTransactions, "/api/transactions", drogon::Get);
    METHOD_LIST_END

    void getServerKey(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void demoSend(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void meshState(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void meshGossip(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void meshFlush(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void meshReset(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void bridgeIngest(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void listAccounts(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void listTransactions(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace controller
} // namespace upimesh
