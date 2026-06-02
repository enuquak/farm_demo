#include "redis_async.h"
#include "log_macros.h"

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>

#include <cstring>
#include <memory>

namespace farm {

// ============================================================================
// 回调包装结构体（用于在 hiredis 回调中传递用户回调）
// ============================================================================

/**
 * @brief 单值回调包装
 *
 * 作为 redisAsyncCommand 的 privdata 参数传递，
 * 在 on_command 中提取并调用用户回调。
 */
struct SingleCallbackWrapper {
    RedisCallback callback;
};

/**
 * @brief 数组回调包装
 *
 * 用于 SMEMBERS、HGETALL 等返回数组的命令。
 */
struct ArrayCallbackWrapper {
    RedisArrayCallback callback;
};

// ============================================================================
// 静态回调函数
// ============================================================================

void RedisAsyncClient::on_connect(const redisAsyncContext* ac, int status)
{
    // 通过 ac->data 获取 RedisAsyncClient 指针
    auto* self = static_cast<RedisAsyncClient*>(ac->data);
    if (!self) {
        return;
    }

    if (status != REDIS_OK) {
        SPDLOG_ERROR("[{}]Connect failed: {}", LogModule::RedisAsync,
                     ac->errstr ? ac->errstr : "unknown error");
        self->connected_.store(false, std::memory_order_release);
        return;
    }

    SPDLOG_INFO("[{}]Connected to {}:{}",
                LogModule::RedisAsync, self->config_.host, self->config_.port);
    self->connected_.store(true, std::memory_order_release);
}

void RedisAsyncClient::on_disconnect(const redisAsyncContext* ac, int status)
{
    auto* self = static_cast<RedisAsyncClient*>(ac->data);
    if (!self) {
        return;
    }

    self->connected_.store(false, std::memory_order_release);

    if (status != REDIS_OK) {
        SPDLOG_ERROR("[{}]Disconnected with error: {}",
                     LogModule::RedisAsync,
                     ac->errstr ? ac->errstr : "unknown error");
    } else {
        SPDLOG_INFO("[{}]Disconnected gracefully", LogModule::RedisAsync);
    }
}

void RedisAsyncClient::on_command(redisAsyncContext* ac, void* reply, void* privdata)
{
    auto* redis_reply = static_cast<redisReply*>(reply);

    // 处理 NULL 回调（连接断开时的情况）
    if (!privdata) {
        return;
    }

    // 尝试解析为 SingleCallbackWrapper
    auto* single_wrapper = static_cast<SingleCallbackWrapper*>(privdata);

    // 检查是否有错误或空回复
    if (!redis_reply) {
        // 连接已断开，回调失败
        if (single_wrapper->callback) {
            single_wrapper->callback(false, "connection lost");
        }
        delete single_wrapper;
        return;
    }

    // 根据回复类型分发
    switch (redis_reply->type) {
        case REDIS_REPLY_STRING: {
            std::string result(redis_reply->str, redis_reply->len);
            if (single_wrapper->callback) {
                single_wrapper->callback(true, result);
            }
            break;
        }
        case REDIS_REPLY_INTEGER: {
            std::string result = std::to_string(redis_reply->integer);
            if (single_wrapper->callback) {
                single_wrapper->callback(true, result);
            }
            break;
        }
        case REDIS_REPLY_STATUS: {
            std::string result(redis_reply->str, redis_reply->len);
            if (single_wrapper->callback) {
                single_wrapper->callback(true, result);
            }
            break;
        }
        case REDIS_REPLY_NIL: {
            if (single_wrapper->callback) {
                single_wrapper->callback(true, "");
            }
            break;
        }
        case REDIS_REPLY_ARRAY: {
            // 数组回复：尝试提取为 ArrayCallbackWrapper
            // 由于 SingleCallbackWrapper 和 ArrayCallbackWrapper 的内存布局不同，
            // 我们需要通过检查回调是否为空来判断类型
            // 这里我们先假设它是 SingleCallbackWrapper，将数组转为逗号分隔字符串
            std::string result;
            for (size_t i = 0; i < redis_reply->elements; ++i) {
                if (i > 0) {
                    result += ",";
                }
                if (redis_reply->element[i]->type == REDIS_REPLY_STRING) {
                    result += std::string(redis_reply->element[i]->str,
                                          redis_reply->element[i]->len);
                } else if (redis_reply->element[i]->type == REDIS_REPLY_INTEGER) {
                    result += std::to_string(redis_reply->element[i]->integer);
                }
            }
            if (single_wrapper->callback) {
                single_wrapper->callback(true, result);
            }
            break;
        }
        case REDIS_REPLY_ERROR: {
            std::string error_msg(redis_reply->str, redis_reply->len);
            SPDLOG_ERROR("[{}]Command error: {}", LogModule::RedisAsync, error_msg);
            if (single_wrapper->callback) {
                single_wrapper->callback(false, error_msg);
            }
            break;
        }
        default: {
            SPDLOG_WARN("[{}]Unknown reply type: {}", LogModule::RedisAsync, redis_reply->type);
            if (single_wrapper->callback) {
                single_wrapper->callback(false, "unknown reply type");
            }
            break;
        }
    }

    delete single_wrapper;
}

// ============================================================================
// RedisAsyncClient 实现
// ============================================================================

RedisAsyncClient::RedisAsyncClient(event_base* base, const RedisAsyncConfig& config)
    : base_(base)
    , async_ctx_(nullptr)
    , connected_(false)
    , config_(config)
{
}

RedisAsyncClient::~RedisAsyncClient()
{
    disconnect();
}

bool RedisAsyncClient::connect()
{
    if (async_ctx_) {
        SPDLOG_WARN("[{}]Already connected or connecting", LogModule::RedisAsync);
        return false;
    }

    // 创建异步连接
    async_ctx_ = redisAsyncConnect(config_.host.c_str(), config_.port);
    if (!async_ctx_ || async_ctx_->err) {
        std::string err_str = async_ctx_ ? async_ctx_->errstr : "failed to allocate context";
        SPDLOG_ERROR("[{}]redisAsyncConnect failed: {}", LogModule::RedisAsync, err_str);
        if (async_ctx_) {
            redisAsyncFree(async_ctx_);
            async_ctx_ = nullptr;
        }
        return false;
    }

    // 将 client 指针存入 context 的 data 字段，用于在静态回调中访问
    async_ctx_->data = this;

    // 绑定到 libevent 事件循环
    if (redisLibeventAttach(async_ctx_, base_) != REDIS_OK) {
        SPDLOG_ERROR("[{}]redisLibeventAttach failed", LogModule::RedisAsync);
        redisAsyncFree(async_ctx_);
        async_ctx_ = nullptr;
        return false;
    }

    // 设置连接和断开回调
    redisAsyncSetConnectCallback(async_ctx_, on_connect);
    redisAsyncSetDisconnectCallback(async_ctx_, on_disconnect);

    // 设置命令超时
    struct timeval timeout;
    timeout.tv_sec = config_.command_timeout_ms / 1000;
    timeout.tv_usec = (config_.command_timeout_ms % 1000) * 1000;
    redisAsyncSetTimeout(async_ctx_, timeout);

    SPDLOG_INFO("[{}]Connecting to {}:{}...", LogModule::RedisAsync, config_.host, config_.port);
    return true;
}

void RedisAsyncClient::disconnect()
{
    if (async_ctx_) {
        // 断开连接（会触发 on_disconnect 回调）
        redisAsyncDisconnect(async_ctx_);
        async_ctx_ = nullptr;
        connected_.store(false, std::memory_order_release);
    }
}

bool RedisAsyncClient::is_connected() const
{
    return connected_.load(std::memory_order_acquire);
}

// ============================================================================
// KV 操作
// ============================================================================

void RedisAsyncClient::set(const std::string& key, const std::string& value, int ttl, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret;
    if (ttl > 0) {
        // SET key value EX ttl
        ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "SET %b %b EX %d",
                                key.c_str(), key.size(),
                                value.c_str(), value.size(),
                                ttl);
    } else {
        // SET key value
        ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "SET %b %b",
                                key.c_str(), key.size(),
                                value.c_str(), value.size());
    }

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]SET command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

void RedisAsyncClient::get(const std::string& key, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "GET %b",
                                key.c_str(), key.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]GET command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

void RedisAsyncClient::del(const std::string& key, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "DEL %b",
                                key.c_str(), key.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]DEL command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

// ============================================================================
// Set 操作
// ============================================================================

void RedisAsyncClient::sadd(const std::string& key, const std::string& member, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "SADD %b %b",
                                key.c_str(), key.size(),
                                member.c_str(), member.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]SADD command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

void RedisAsyncClient::srem(const std::string& key, const std::string& member, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "SREM %b %b",
                                key.c_str(), key.size(),
                                member.c_str(), member.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]SREM command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

void RedisAsyncClient::sismember(const std::string& key, const std::string& member, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "SISMEMBER %b %b",
                                key.c_str(), key.size(),
                                member.c_str(), member.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]SISMEMBER command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

void RedisAsyncClient::smembers(const std::string& key, RedisArrayCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, {});
        }
        return;
    }

    // 使用 ArrayCallbackWrapper，通过 on_command_array 处理
    auto* wrapper = new ArrayCallbackWrapper{std::move(cb)};

    // 使用通用 on_command，但需要特殊处理数组
    // 由于 hiredis 的 privdata 是 void*，我们复用 on_command
    // 但 on_command 期望 SingleCallbackWrapper，这里需要调整
    // 解决方案：使用专门的数组回调
    int ret = redisAsyncCommand(async_ctx_,
                                [](redisAsyncContext* ac, void* reply, void* privdata) {
                                    auto* redis_reply = static_cast<redisReply*>(reply);
                                    auto* wrapper = static_cast<ArrayCallbackWrapper*>(privdata);

                                    if (!privdata) {
                                        return;
                                    }

                                    if (!redis_reply) {
                                        if (wrapper->callback) {
                                            wrapper->callback(false, {});
                                        }
                                        delete wrapper;
                                        return;
                                    }

                                    switch (redis_reply->type) {
                                        case REDIS_REPLY_ARRAY: {
                                            std::vector<std::string> results;
                                            results.reserve(redis_reply->elements);
                                            for (size_t i = 0; i < redis_reply->elements; ++i) {
                                                auto* elem = redis_reply->element[i];
                                                if (elem->type == REDIS_REPLY_STRING) {
                                                    results.emplace_back(elem->str, elem->len);
                                                } else if (elem->type == REDIS_REPLY_INTEGER) {
                                                    results.push_back(std::to_string(elem->integer));
                                                }
                                            }
                                            if (wrapper->callback) {
                                                wrapper->callback(true, results);
                                            }
                                            break;
                                        }
                                        case REDIS_REPLY_NIL: {
                                            if (wrapper->callback) {
                                                wrapper->callback(true, {});
                                            }
                                            break;
                                        }
                                        case REDIS_REPLY_ERROR: {
                                            std::string error_msg(redis_reply->str, redis_reply->len);
                                            SPDLOG_ERROR("[{}]SMEMBERS error: {}", LogModule::RedisAsync, error_msg);
                                            if (wrapper->callback) {
                                                wrapper->callback(false, {});
                                            }
                                            break;
                                        }
                                        default: {
                                            if (wrapper->callback) {
                                                wrapper->callback(false, {});
                                            }
                                            break;
                                        }
                                    }

                                    delete wrapper;
                                },
                                wrapper,
                                "SMEMBERS %b",
                                key.c_str(), key.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]SMEMBERS command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, {});
        }
        delete wrapper;
    }
}

void RedisAsyncClient::scard(const std::string& key, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "SCARD %b",
                                key.c_str(), key.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]SCARD command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

// ============================================================================
// Hash 操作
// ============================================================================

void RedisAsyncClient::hset(const std::string& key, const std::string& field,
                            const std::string& value, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "HSET %b %b %b",
                                key.c_str(), key.size(),
                                field.c_str(), field.size(),
                                value.c_str(), value.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]HSET command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

void RedisAsyncClient::hget(const std::string& key, const std::string& field, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "HGET %b %b",
                                key.c_str(), key.size(),
                                field.c_str(), field.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]HGET command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

void RedisAsyncClient::hdel(const std::string& key, const std::string& field, RedisCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, "not connected");
        }
        return;
    }

    auto* wrapper = new SingleCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_, on_command, wrapper,
                                "HDEL %b %b",
                                key.c_str(), key.size(),
                                field.c_str(), field.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]HDEL command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, "failed to enqueue command");
        }
        delete wrapper;
    }
}

void RedisAsyncClient::hgetall(const std::string& key, RedisArrayCallback cb)
{
    if (!is_connected()) {
        if (cb) {
            cb(false, {});
        }
        return;
    }

    auto* wrapper = new ArrayCallbackWrapper{std::move(cb)};

    int ret = redisAsyncCommand(async_ctx_,
                                [](redisAsyncContext* ac, void* reply, void* privdata) {
                                    auto* redis_reply = static_cast<redisReply*>(reply);
                                    auto* wrapper = static_cast<ArrayCallbackWrapper*>(privdata);

                                    if (!privdata) {
                                        return;
                                    }

                                    if (!redis_reply) {
                                        if (wrapper->callback) {
                                            wrapper->callback(false, {});
                                        }
                                        delete wrapper;
                                        return;
                                    }

                                    switch (redis_reply->type) {
                                        case REDIS_REPLY_ARRAY: {
                                            // HGETALL 返回 [field1, value1, field2, value2, ...]
                                            std::vector<std::string> results;
                                            results.reserve(redis_reply->elements);
                                            for (size_t i = 0; i < redis_reply->elements; ++i) {
                                                auto* elem = redis_reply->element[i];
                                                if (elem->type == REDIS_REPLY_STRING) {
                                                    results.emplace_back(elem->str, elem->len);
                                                } else if (elem->type == REDIS_REPLY_INTEGER) {
                                                    results.push_back(std::to_string(elem->integer));
                                                }
                                            }
                                            if (wrapper->callback) {
                                                wrapper->callback(true, results);
                                            }
                                            break;
                                        }
                                        case REDIS_REPLY_NIL: {
                                            if (wrapper->callback) {
                                                wrapper->callback(true, {});
                                            }
                                            break;
                                        }
                                        case REDIS_REPLY_ERROR: {
                                            std::string error_msg(redis_reply->str, redis_reply->len);
                                            SPDLOG_ERROR("[{}]HGETALL error: {}", LogModule::RedisAsync, error_msg);
                                            if (wrapper->callback) {
                                                wrapper->callback(false, {});
                                            }
                                            break;
                                        }
                                        default: {
                                            if (wrapper->callback) {
                                                wrapper->callback(false, {});
                                            }
                                            break;
                                        }
                                    }

                                    delete wrapper;
                                },
                                wrapper,
                                "HGETALL %b",
                                key.c_str(), key.size());

    if (ret != REDIS_OK) {
        SPDLOG_ERROR("[{}]HGETALL command failed to enqueue", LogModule::RedisAsync);
        if (wrapper->callback) {
            wrapper->callback(false, {});
        }
        delete wrapper;
    }
}

}  // namespace farm
