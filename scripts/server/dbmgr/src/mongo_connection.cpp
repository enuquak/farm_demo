#include "mongo_connection.h"
#include "log_macros.h"

namespace farm {

MongoConnection::MongoConnection() {
    // 初始化 libmongoc（只需调用一次，但多次调用是安全的）
    mongoc_init();
}

MongoConnection::~MongoConnection() {
    disconnect();
}

bool MongoConnection::connect(const std::string& uri) {
    if (connected_) {
        SPDLOG_WARN("[MongoConnection]Already connected, disconnecting first");
        disconnect();
    }

    client_ = mongoc_client_new(uri.c_str());
    if (!client_) {
        SPDLOG_ERROR("[MongoConnection]Failed to create client from uri: {}", uri);
        return false;
    }

    // 测试连接：执行 ping 命令
    bson_t ping_cmd;
    bson_init(&ping_cmd);
    BSON_APPEND_INT32(&ping_cmd, "ping", 1);

    bson_t reply;
    bson_error_t error;
    bool ok = mongoc_client_command_simple(client_, "admin", &ping_cmd, nullptr, &reply, &error);
    bson_destroy(&ping_cmd);
    bson_destroy(&reply);

    if (!ok) {
        SPDLOG_ERROR("[MongoConnection]Connection test failed: {}", error.message);
        mongoc_client_destroy(client_);
        client_ = nullptr;
        return false;
    }

    connected_ = true;
    SPDLOG_INFO("[MongoConnection]Connected to MongoDB: {}", uri);
    return true;
}

void MongoConnection::disconnect() {
    if (client_) {
        mongoc_client_destroy(client_);
        client_ = nullptr;
    }
    connected_ = false;
    SPDLOG_INFO("[MongoConnection]Disconnected from MongoDB");
}

bool MongoConnection::is_connected() const {
    return connected_;
}

mongoc_client_t* MongoConnection::client() {
    return client_;
}

}  // namespace farm
