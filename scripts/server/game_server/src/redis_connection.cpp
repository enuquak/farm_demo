#include "redis_connection.h"
#include "log_macros.h"

#include <cstring>

namespace farm {

RedisConnection::RedisConnection() : context_(nullptr), connected_(false) {
}

RedisConnection::~RedisConnection() {
    disconnect();
}

bool RedisConnection::parse_uri(const std::string& uri, std::string& host, int& port) {
    // Support formats: redis://host:port or host:port
    std::string clean_uri = uri;
    if (clean_uri.substr(0, 8) == "redis://") {
        clean_uri = clean_uri.substr(8);
    }

    auto colon_pos = clean_uri.find(':');
    if (colon_pos == std::string::npos) {
        host = clean_uri;
        port = 6379;
    } else {
        host = clean_uri.substr(0, colon_pos);
        try {
            port = std::stoi(clean_uri.substr(colon_pos + 1));
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Redis]Failed to parse port from URI '{}': {}", uri, e.what());
            return false;
        }
    }
    return true;
}

bool RedisConnection::connect(const std::string& uri) {
    if (connected_) {
        disconnect();
    }

    std::string host;
    int port = 6379;
    if (!parse_uri(uri, host, port)) {
        SPDLOG_ERROR("[Redis]Failed to parse URI: {}", uri);
        return false;
    }

    SPDLOG_INFO("[Redis]Connecting to {}:{}", host, port);

    context_ = redisConnect(host.c_str(), port);
    if (!context_ || context_->err) {
        if (context_) {
            SPDLOG_ERROR("[Redis]Connection failed: {}", context_->errstr);
            redisFree(context_);
            context_ = nullptr;
        } else {
            SPDLOG_ERROR("[Redis]Connection failed: cannot allocate context");
        }
        connected_ = false;
        return false;
    }

    connected_ = true;
    SPDLOG_INFO("[Redis]Connected to {}:{}", host, port);
    return true;
}

void RedisConnection::disconnect() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
    connected_ = false;
}

bool RedisConnection::is_connected() const {
    return connected_ && context_ != nullptr;
}

bool RedisConnection::set(const std::string& key, const std::string& value) {
    if (!is_connected()) {
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "SET %s %s", key.c_str(), value.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]SET command failed: connection lost");
        connected_ = false;
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_STATUS &&
                    std::string(reply->str) == "OK");
    if (!success) {
        SPDLOG_ERROR("[Redis]SET {} failed: {}", key, reply->str ? reply->str : "unknown");
    }

    freeReplyObject(reply);
    return success;
}

std::string RedisConnection::get(const std::string& key) {
    if (!is_connected()) {
        return "";
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "GET %s", key.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]GET command failed: connection lost");
        connected_ = false;
        return "";
    }

    std::string result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    }

    freeReplyObject(reply);
    return result;
}

bool RedisConnection::del(const std::string& key) {
    if (!is_connected()) {
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "DEL %s", key.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]DEL command failed: connection lost");
        connected_ = false;
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER);
    if (!success) {
        SPDLOG_ERROR("[Redis]DEL {} failed: {}", key, reply->str ? reply->str : "unknown");
    }

    freeReplyObject(reply);
    return success;
}

}  // namespace farm
