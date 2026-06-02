#include "mongo_replica_set.h"
#include "log_macros.h"

#include <cstring>

namespace farm {

MongoReplicaSetConnection::MongoReplicaSetConnection(const MongoReplicaSetConfig& config)
    : config_(config)
{
    // 初始化 libmongoc（多次调用是安全的）
    mongoc_init();
}

MongoReplicaSetConnection::~MongoReplicaSetConnection()
{
    disconnect();
}

bool MongoReplicaSetConnection::connect()
{
    if (state_ == ConnectionState::CONNECTED && client_) {
        SPDLOG_WARN("[MongoReplicaSet]Already connected, disconnecting first");
        disconnect();
    }

    state_ = ConnectionState::CONNECTING;
    SPDLOG_INFO("[MongoReplicaSet]Connecting to MongoDB Replica Set: {}", config_.uri);

    // 创建 client
    client_ = mongoc_client_new(config_.uri.c_str());
    if (!client_) {
        SPDLOG_ERROR("[MongoReplicaSet]Failed to create client from uri: {}", config_.uri);
        state_ = ConnectionState::FAILED;
        return false;
    }

    // 配置超时
    bson_t* opts = bson_new();
    BSON_APPEND_INT32(opts, "connectTimeoutMS", config_.connect_timeout_ms);
    BSON_APPEND_INT32(opts, "socketTimeoutMS", config_.socket_timeout_ms);
    mongoc_client_set_appname(client_, "farm_dbmgr");
    bson_destroy(opts);

    // 应用初始 read preference
    apply_read_preference(config_.read_preference);

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
        SPDLOG_ERROR("[MongoReplicaSet]Connection test failed: {}", error.message);
        mongoc_client_destroy(client_);
        client_ = nullptr;
        state_ = ConnectionState::FAILED;
        return false;
    }

    connected_ = true;
    state_ = ConnectionState::CONNECTED;
    saved_read_preference_ = config_.read_preference;
    SPDLOG_INFO("[MongoReplicaSet]Connected to MongoDB Replica Set, read_preference={}", config_.read_preference);
    return true;
}

void MongoReplicaSetConnection::disconnect()
{
    if (client_) {
        mongoc_client_destroy(client_);
        client_ = nullptr;
    }
    connected_ = false;
    state_ = ConnectionState::DISCONNECTED;
    SPDLOG_INFO("[MongoReplicaSet]Disconnected from MongoDB Replica Set");
}

bool MongoReplicaSetConnection::is_connected() const
{
    return state_ == ConnectionState::CONNECTED && client_ != nullptr;
}

mongoc_client_t* MongoReplicaSetConnection::client()
{
    return client_;
}

void MongoReplicaSetConnection::read_from_primary_after_write()
{
    if (!client_) {
        SPDLOG_WARN("[MongoReplicaSet]Cannot set read preference: not connected");
        return;
    }

    // 保存当前 read preference（仅首次调用时保存）
    if (saved_read_preference_.empty()) {
        saved_read_preference_ = config_.read_preference;
    }

    apply_read_preference("primary");
    SPDLOG_DEBUG("[MongoReplicaSet]Read preference set to primary (after write)");
}

void MongoReplicaSetConnection::restore_read_preference()
{
    if (!client_) {
        SPDLOG_WARN("[MongoReplicaSet]Cannot restore read preference: not connected");
        return;
    }

    apply_read_preference(saved_read_preference_);
    SPDLOG_DEBUG("[MongoReplicaSet]Read preference restored to {}", saved_read_preference_);
}

ConnectionState MongoReplicaSetConnection::state() const
{
    return state_;
}

void MongoReplicaSetConnection::apply_read_preference(const std::string& preference)
{
    if (!client_) {
        return;
    }

    mongoc_read_prefs_t* read_prefs = nullptr;

    if (preference == "primary") {
        read_prefs = mongoc_read_prefs_new(MONGOC_READ_PRIMARY);
    } else if (preference == "secondary") {
        read_prefs = mongoc_read_prefs_new(MONGOC_READ_SECONDARY);
    } else if (preference == "primaryPreferred") {
        read_prefs = mongoc_read_prefs_new(MONGOC_READ_PRIMARY_PREFERRED);
    } else if (preference == "secondaryPreferred") {
        read_prefs = mongoc_read_prefs_new(MONGOC_READ_SECONDARY_PREFERRED);
    } else if (preference == "nearest") {
        read_prefs = mongoc_read_prefs_new(MONGOC_READ_NEAREST);
    } else {
        SPDLOG_WARN("[MongoReplicaSet]Unknown read preference: {}, using primary", preference);
        read_prefs = mongoc_read_prefs_new(MONGOC_READ_PRIMARY);
    }

    mongoc_client_set_read_prefs(client_, read_prefs);
    mongoc_read_prefs_destroy(read_prefs);
}

}  // namespace farm
