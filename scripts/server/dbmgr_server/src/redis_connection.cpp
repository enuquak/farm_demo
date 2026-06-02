#include "redis_connection.h"
#include "log_macros.h"

#include <cstring>

namespace farm {

RedisConnection::RedisConnection() {
}

RedisConnection::~RedisConnection() {
    disconnect();
}

bool RedisConnection::connect(const std::string& uri) {
    if (connected_) {
        SPDLOG_WARN("[RedisConnection]Already connected, disconnecting first");
        disconnect();
    }

    // 解析 URI（支持 redis://host:port 和 tcp://host:port）
    std::string host = "127.0.0.1";
    int port = 6379;

    std::string uri_str = uri;
    // 移除协议前缀
    if (uri_str.find("redis://") == 0) {
        uri_str = uri_str.substr(8);
    } else if (uri_str.find("tcp://") == 0) {
        uri_str = uri_str.substr(6);
    }

    // 解析 host:port
    auto colon_pos = uri_str.find(':');
    if (colon_pos != std::string::npos) {
        host = uri_str.substr(0, colon_pos);
        port = std::stoi(uri_str.substr(colon_pos + 1));
    } else {
        host = uri_str;
    }

    // 连接
    context_ = redisConnect(host.c_str(), port);
    if (!context_) {
        SPDLOG_ERROR("[RedisConnection]Failed to allocate redis context");
        return false;
    }
    if (context_->err) {
        SPDLOG_ERROR("[RedisConnection]Connection failed: {}", context_->errstr);
        redisFree(context_);
        context_ = nullptr;
        return false;
    }

    // 测试连接
    redisReply* reply = (redisReply*)redisCommand(context_, "PING");
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        SPDLOG_ERROR("[RedisConnection]PING test failed");
        if (reply) freeReplyObject(reply);
        redisFree(context_);
        context_ = nullptr;
        return false;
    }
    freeReplyObject(reply);

    connected_ = true;
    SPDLOG_INFO("[RedisConnection]Connected to Redis: {}:{}", host, port);
    return true;
}

void RedisConnection::disconnect() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
    connected_ = false;
    SPDLOG_INFO("[RedisConnection]Disconnected from Redis");
}

bool RedisConnection::is_connected() const {
    return connected_;
}

redisContext* RedisConnection::context() {
    return context_;
}

}  // namespace farm
