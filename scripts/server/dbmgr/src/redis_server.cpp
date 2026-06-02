#include "redis_server.h"
#include "log_macros.h"

#include <cstring>

namespace farm {

RedisServer::RedisServer(RedisPool& pool)
    : pool_(pool) {
}

CacheResult RedisServer::get(const std::string& key, std::vector<uint8_t>& value) {
    if (!pool_.is_ready()) {
        return CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_.acquire();
    if (!guard.is_valid()) {
        SPDLOG_ERROR("[RedisServer]Failed to acquire Redis connection for GET key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = (redisReply*)redisCommand(guard.context(), "GET %s", key.c_str());
    if (!reply) {
        SPDLOG_ERROR("[RedisServer]GET command failed for key: {}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_NIL) {
        result = CacheResult::NOT_FOUND;
        value.clear();
    } else if (reply->type == REDIS_REPLY_STRING) {
        result = CacheResult::SUCCESS;
        value.assign(reply->str, reply->str + reply->len);
    } else {
        SPDLOG_ERROR("[RedisServer]Unexpected reply type for GET: {}", reply->type);
        result = CacheResult::CONNECTION_ERROR;
        value.clear();
    }

    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::set(const std::string& key, const std::vector<uint8_t>& value, int ttl_seconds) {
    if (!pool_.is_ready()) {
        return CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_.acquire();
    if (!guard.is_valid()) {
        SPDLOG_ERROR("[RedisServer]Failed to acquire Redis connection for SET key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = (redisReply*)redisCommand(guard.context(),
        "SET %s %b EX %d",
        key.c_str(),
        value.data(), value.size(),
        ttl_seconds);

    if (!reply) {
        SPDLOG_ERROR("[RedisServer]SET command failed for key: {}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_STATUS && reply->str && std::strcmp(reply->str, "OK") == 0) {
        result = CacheResult::SUCCESS;
    } else {
        SPDLOG_ERROR("[RedisServer]SET failed: {}", reply->str ? reply->str : "unknown");
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::del(const std::string& key) {
    if (!pool_.is_ready()) {
        return CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_.acquire();
    if (!guard.is_valid()) {
        SPDLOG_ERROR("[RedisServer]Failed to acquire Redis connection for DEL key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = (redisReply*)redisCommand(guard.context(), "DEL %s", key.c_str());
    if (!reply) {
        SPDLOG_ERROR("[RedisServer]DEL command failed for key: {}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer > 0 ? CacheResult::SUCCESS : CacheResult::NOT_FOUND;
    } else {
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

bool RedisServer::is_available() const {
    return pool_.is_ready();
}

}  // namespace farm
