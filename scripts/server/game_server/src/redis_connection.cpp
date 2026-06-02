#include "redis_connection.h"
#include "log_macros.h"

#include <cstring>
#include <vector>

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

// ===========================================
// Set operations
// ===========================================

bool RedisConnection::sadd(const std::string& key, const std::string& member) {
    if (!is_connected()) return false;

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "SADD %s %s", key.c_str(), member.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]SADD command failed: connection lost");
        connected_ = false;
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER);
    if (!success) {
        SPDLOG_ERROR("[Redis]SADD {} {} failed: {}", key, member,
                     reply->str ? reply->str : "unknown");
    }

    freeReplyObject(reply);
    return success;
}

bool RedisConnection::srem(const std::string& key, const std::string& member) {
    if (!is_connected()) return false;

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "SREM %s %s", key.c_str(), member.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]SREM command failed: connection lost");
        connected_ = false;
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER);
    if (!success) {
        SPDLOG_ERROR("[Redis]SREM {} {} failed: {}", key, member,
                     reply->str ? reply->str : "unknown");
    }

    freeReplyObject(reply);
    return success;
}

bool RedisConnection::sismember(const std::string& key, const std::string& member) {
    if (!is_connected()) return false;

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "SISMEMBER %s %s", key.c_str(), member.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]SISMEMBER command failed: connection lost");
        connected_ = false;
        return false;
    }

    bool result = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    freeReplyObject(reply);
    return result;
}

size_t RedisConnection::scard(const std::string& key) {
    if (!is_connected()) return 0;

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "SCARD %s", key.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]SCARD command failed: connection lost");
        connected_ = false;
        return 0;
    }

    size_t count = 0;
    if (reply->type == REDIS_REPLY_INTEGER) {
        count = static_cast<size_t>(reply->integer);
    }

    freeReplyObject(reply);
    return count;
}

std::unordered_set<std::string> RedisConnection::smembers(const std::string& key) {
    std::unordered_set<std::string> result;
    if (!is_connected()) return result;

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "SMEMBERS %s", key.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]SMEMBERS command failed: connection lost");
        connected_ = false;
        return result;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i] && reply->element[i]->type == REDIS_REPLY_STRING) {
                result.insert(std::string(reply->element[i]->str, reply->element[i]->len));
            }
        }
    }

    freeReplyObject(reply);
    return result;
}

// ===========================================
// Hash operations
// ===========================================

bool RedisConnection::hset(const std::string& key, const std::string& field,
                           const std::string& value) {
    if (!is_connected()) return false;

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]HSET command failed: connection lost");
        connected_ = false;
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER);
    if (!success) {
        SPDLOG_ERROR("[Redis]HSET {} {} failed: {}", key, field,
                     reply->str ? reply->str : "unknown");
    }

    freeReplyObject(reply);
    return success;
}

std::string RedisConnection::hget(const std::string& key, const std::string& field) {
    if (!is_connected()) return "";

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "HGET %s %s", key.c_str(), field.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]HGET command failed: connection lost");
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

bool RedisConnection::hdel(const std::string& key, const std::string& field) {
    if (!is_connected()) return false;

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "HDEL %s %s", key.c_str(), field.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]HDEL command failed: connection lost");
        connected_ = false;
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER);
    if (!success) {
        SPDLOG_ERROR("[Redis]HDEL {} {} failed: {}", key, field,
                     reply->str ? reply->str : "unknown");
    }

    freeReplyObject(reply);
    return success;
}

std::unordered_map<std::string, std::string> RedisConnection::hgetall(
    const std::string& key) {
    std::unordered_map<std::string, std::string> result;
    if (!is_connected()) return result;

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "HGETALL %s", key.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]HGETALL command failed: connection lost");
        connected_ = false;
        return result;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        // HGETALL returns alternating field/value pairs
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            auto* field_reply = reply->element[i];
            auto* value_reply = reply->element[i + 1];
            if (field_reply && field_reply->type == REDIS_REPLY_STRING &&
                value_reply && value_reply->type == REDIS_REPLY_STRING) {
                result.emplace(std::string(field_reply->str, field_reply->len),
                               std::string(value_reply->str, value_reply->len));
            }
        }
    }

    freeReplyObject(reply);
    return result;
}

// ===========================================
// Key scanning
// ===========================================

std::vector<std::string> RedisConnection::keys(const std::string& pattern) {
    std::vector<std::string> result;
    if (!is_connected()) return result;

    unsigned long cursor = 0;
    do {
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(context_, "SCAN %lu MATCH %s COUNT 100",
                         cursor, pattern.c_str()));

        if (!reply) {
            SPDLOG_ERROR("[Redis]SCAN command failed: connection lost");
            connected_ = false;
            return result;
        }

        if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 2) {
            // Parse cursor
            auto* cursor_reply = reply->element[0];
            if (cursor_reply && cursor_reply->type == REDIS_REPLY_STRING) {
                cursor = std::stoul(std::string(cursor_reply->str, cursor_reply->len));
            } else if (cursor_reply && cursor_reply->type == REDIS_REPLY_INTEGER) {
                cursor = static_cast<unsigned long>(cursor_reply->integer);
            }

            // Parse matched keys
            auto* keys_reply = reply->element[1];
            if (keys_reply && keys_reply->type == REDIS_REPLY_ARRAY) {
                for (size_t i = 0; i < keys_reply->elements; ++i) {
                    auto* key_reply = keys_reply->element[i];
                    if (key_reply && key_reply->type == REDIS_REPLY_STRING) {
                        result.push_back(std::string(key_reply->str, key_reply->len));
                    }
                }
            }
        } else {
            cursor = 0;
        }

        freeReplyObject(reply);
    } while (cursor != 0);

    return result;
}

}  // namespace farm
