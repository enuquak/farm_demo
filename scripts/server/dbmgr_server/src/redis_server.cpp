#include "redis_server.h"
#include "log_macros.h"

// Windows: timeval 在 winsock2.h 中定义，hiredis 需要它
#ifdef _WIN32
#include <winsock2.h>
#endif

#include <hiredis/hiredis.h>

#include <cstring>

namespace farm {

// ============================================================================
// 构造函数
// ============================================================================

RedisServer::RedisServer(RedisPool& pool)
    : pool_(&pool)
    , cluster_(nullptr)
    , use_cluster_(false)
{
}

RedisServer::RedisServer(RedisClusterClient& cluster)
    : pool_(nullptr)
    , cluster_(&cluster)
    , use_cluster_(true)
{
}

// ============================================================================
// KV 操作
// ============================================================================

CacheResult RedisServer::get(const std::string& key, std::string& value) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        if (cluster_->get(key, value)) {
            return CacheResult::SUCCESS;
        }
        return CacheResult::NOT_FOUND;
    }

    // 普通模式：RAII 获取连接
    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        SPDLOG_ERROR("[RedisServer]Failed to acquire connection for GET key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "GET %s", key.c_str()));
    if (!reply) {
        SPDLOG_ERROR("[RedisServer]GET command failed for key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_NIL) {
        result = CacheResult::NOT_FOUND;
        value.clear();
    } else if (reply->type == REDIS_REPLY_STRING) {
        result = CacheResult::SUCCESS;
        value.assign(reply->str, reply->len);
    } else {
        SPDLOG_ERROR("[RedisServer]Unexpected reply type for GET: {}", reply->type);
        result = CacheResult::CONNECTION_ERROR;
        value.clear();
    }

    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::get(const std::string& key, std::vector<uint8_t>& value) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        std::string str_value;
        if (cluster_->get(key, str_value)) {
            value.assign(str_value.begin(), str_value.end());
            return CacheResult::SUCCESS;
        }
        value.clear();
        return CacheResult::NOT_FOUND;
    }

    // 普通模式：RAII 获取连接
    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        SPDLOG_ERROR("[RedisServer]Failed to acquire connection for GET key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "GET %s", key.c_str()));
    if (!reply) {
        SPDLOG_ERROR("[RedisServer]GET command failed for key={}", key);
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

CacheResult RedisServer::set(const std::string& key, const std::string& value, int ttl_seconds) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        if (cluster_->set(key, value, ttl_seconds)) {
            return CacheResult::SUCCESS;
        }
        return CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        SPDLOG_ERROR("[RedisServer]Failed to acquire connection for SET key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "SET %s %s EX %d",
                     key.c_str(), value.c_str(), ttl_seconds));
    if (!reply) {
        SPDLOG_ERROR("[RedisServer]SET command failed for key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_STATUS &&
        reply->str && std::strcmp(reply->str, "OK") == 0) {
        result = CacheResult::SUCCESS;
    } else {
        SPDLOG_ERROR("[RedisServer]SET failed: {}", reply->str ? reply->str : "unknown");
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::set(const std::string& key, const std::vector<uint8_t>& value, int ttl_seconds) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        std::string str_value(value.begin(), value.end());
        if (cluster_->set(key, str_value, ttl_seconds)) {
            return CacheResult::SUCCESS;
        }
        return CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        SPDLOG_ERROR("[RedisServer]Failed to acquire connection for SET key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "SET %s %b EX %d",
                     key.c_str(),
                     value.data(), value.size(),
                     ttl_seconds));
    if (!reply) {
        SPDLOG_ERROR("[RedisServer]SET command failed for key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_STATUS &&
        reply->str && std::strcmp(reply->str, "OK") == 0) {
        result = CacheResult::SUCCESS;
    } else {
        SPDLOG_ERROR("[RedisServer]SET failed: {}", reply->str ? reply->str : "unknown");
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::del(const std::string& key) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        if (cluster_->del(key)) {
            return CacheResult::SUCCESS;
        }
        return CacheResult::NOT_FOUND;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        SPDLOG_ERROR("[RedisServer]Failed to acquire connection for DEL key={}", key);
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "DEL %s", key.c_str()));
    if (!reply) {
        SPDLOG_ERROR("[RedisServer]DEL command failed for key={}", key);
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

// ============================================================================
// Set 操作
// ============================================================================

CacheResult RedisServer::sadd(const std::string& key, const std::string& member) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        return cluster_->sadd(key, member) ? CacheResult::SUCCESS : CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "SADD %s %s", key.c_str(), member.c_str()));
    if (!reply) {
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result = (reply->type == REDIS_REPLY_INTEGER)
                             ? CacheResult::SUCCESS
                             : CacheResult::CONNECTION_ERROR;
    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::srem(const std::string& key, const std::string& member) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        return cluster_->srem(key, member) ? CacheResult::SUCCESS : CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "SREM %s %s", key.c_str(), member.c_str()));
    if (!reply) {
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result = (reply->type == REDIS_REPLY_INTEGER)
                             ? CacheResult::SUCCESS
                             : CacheResult::CONNECTION_ERROR;
    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::sismember(const std::string& key, const std::string& member, bool& result) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        return cluster_->sismember(key, member, result)
                   ? CacheResult::SUCCESS
                   : CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "SISMEMBER %s %s", key.c_str(), member.c_str()));
    if (!reply) {
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult cache_result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = (reply->integer == 1);
        cache_result = CacheResult::SUCCESS;
    } else {
        cache_result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return cache_result;
}

CacheResult RedisServer::smembers(const std::string& key, std::vector<std::string>& members) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        return cluster_->smembers(key, members)
                   ? CacheResult::SUCCESS
                   : CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "SMEMBERS %s", key.c_str()));
    if (!reply) {
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        members.clear();
        members.reserve(reply->elements);
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i] && reply->element[i]->type == REDIS_REPLY_STRING) {
                members.push_back(
                    std::string(reply->element[i]->str, reply->element[i]->len));
            }
        }
        result = CacheResult::SUCCESS;
    } else {
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::scard(const std::string& key, int64_t& count) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        return cluster_->scard(key, count)
                   ? CacheResult::SUCCESS
                   : CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "SCARD %s", key.c_str()));
    if (!reply) {
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        count = reply->integer;
        result = CacheResult::SUCCESS;
    } else {
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

// ============================================================================
// Hash 操作
// ============================================================================

CacheResult RedisServer::hset(const std::string& key, const std::string& field, const std::string& value) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        return cluster_->hset(key, field, value)
                   ? CacheResult::SUCCESS
                   : CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "HSET %s %s %s",
                     key.c_str(), field.c_str(), value.c_str()));
    if (!reply) {
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result = (reply->type == REDIS_REPLY_INTEGER)
                             ? CacheResult::SUCCESS
                             : CacheResult::CONNECTION_ERROR;
    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::hget(const std::string& key, const std::string& field, std::string& value) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        if (cluster_->hget(key, field, value)) {
            return CacheResult::SUCCESS;
        }
        return CacheResult::NOT_FOUND;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "HGET %s %s", key.c_str(), field.c_str()));
    if (!reply) {
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_NIL) {
        result = CacheResult::NOT_FOUND;
    } else if (reply->type == REDIS_REPLY_STRING) {
        value.assign(reply->str, reply->len);
        result = CacheResult::SUCCESS;
    } else {
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

CacheResult RedisServer::hdel(const std::string& key, const std::string& field) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        return cluster_->hdel(key, field) ? CacheResult::SUCCESS : CacheResult::NOT_FOUND;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "HDEL %s %s", key.c_str(), field.c_str()));
    if (!reply) {
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

CacheResult RedisServer::hgetall(const std::string& key,
                                  std::unordered_map<std::string, std::string>& kvs) {
    if (use_cluster_) {
        if (!cluster_->is_healthy()) {
            return CacheResult::CONNECTION_ERROR;
        }
        return cluster_->hgetall(key, kvs)
                   ? CacheResult::SUCCESS
                   : CacheResult::CONNECTION_ERROR;
    }

    auto guard = pool_->acquire();
    if (!guard.is_valid()) {
        return CacheResult::CONNECTION_ERROR;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(guard.context(), "HGETALL %s", key.c_str()));
    if (!reply) {
        return CacheResult::CONNECTION_ERROR;
    }

    CacheResult result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        kvs.clear();
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            if (reply->element[i] && reply->element[i]->type == REDIS_REPLY_STRING &&
                reply->element[i + 1] && reply->element[i + 1]->type == REDIS_REPLY_STRING) {
                std::string field(reply->element[i]->str, reply->element[i]->len);
                std::string value(reply->element[i + 1]->str, reply->element[i + 1]->len);
                kvs[field] = value;
            }
        }
        result = CacheResult::SUCCESS;
    } else {
        result = CacheResult::CONNECTION_ERROR;
    }

    freeReplyObject(reply);
    return result;
}

// ============================================================================
// 状态查询
// ============================================================================

bool RedisServer::is_available() const {
    if (use_cluster_) {
        return cluster_ && cluster_->is_healthy();
    }
    return pool_ && pool_->is_ready();
}

}  // namespace farm
