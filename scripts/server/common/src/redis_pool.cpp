#include "redis_pool.h"
#include "log_macros.h"

#include <hiredis/hiredis.h>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace farm {

// ============================================================================
// RedisConnectionGuard
// ============================================================================

RedisConnectionGuard::RedisConnectionGuard(RedisPool* pool, redisContext* context)
    : pool_(pool)
    , context_(context)
{
}

RedisConnectionGuard::~RedisConnectionGuard()
{
    if (pool_ && context_) {
        pool_->release(context_);
        context_ = nullptr;
        pool_ = nullptr;
    }
}

RedisConnectionGuard::RedisConnectionGuard(RedisConnectionGuard&& other) noexcept
    : pool_(other.pool_)
    , context_(other.context_)
{
    other.pool_ = nullptr;
    other.context_ = nullptr;
}

RedisConnectionGuard& RedisConnectionGuard::operator=(RedisConnectionGuard&& other) noexcept
{
    if (this != &other) {
        // 先归还当前持有的连接
        if (pool_ && context_) {
            pool_->release(context_);
        }
        pool_ = other.pool_;
        context_ = other.context_;
        other.pool_ = nullptr;
        other.context_ = nullptr;
    }
    return *this;
}

bool RedisConnectionGuard::is_valid() const
{
    return context_ != nullptr && !context_->err;
}

// ============================================================================
// RedisPool
// ============================================================================

RedisPool::RedisPool(const RedisPoolConfig& config)
    : config_(config)
{
    // 解析 URI
    if (!parse_uri(config_.uri, host_, port_)) {
        SPDLOG_ERROR("[RedisPool]Failed to parse URI: {}", config_.uri);
    }
}

RedisPool::~RedisPool()
{
    shutdown();
}

bool RedisPool::init()
{
    if (running_.load(std::memory_order_acquire)) {
        SPDLOG_WARN("[RedisPool]Already initialized");
        return true;
    }

    if (host_.empty()) {
        SPDLOG_ERROR("[RedisPool]Invalid host, cannot initialize");
        return false;
    }

    running_.store(true, std::memory_order_release);

    SPDLOG_INFO("[RedisPool]Initializing pool, uri={}, pool_size={}, min_idle={}",
                config_.uri, config_.pool_size, config_.min_idle);

    // 创建 min_idle 个初始连接
    int created = 0;
    for (int i = 0; i < config_.min_idle; ++i) {
        redisContext* ctx = create_connection();
        if (ctx) {
            std::lock_guard<std::mutex> lock(mutex_);
            idle_conns_.push_back(ctx);
            ++created;
        } else {
            SPDLOG_WARN("[RedisPool]Failed to create initial connection {}/{}", i + 1, config_.min_idle);
        }
    }

    if (created == 0) {
        SPDLOG_ERROR("[RedisPool]Failed to create any connections");
        running_.store(false, std::memory_order_release);
        return false;
    }

    ready_.store(true, std::memory_order_release);
    SPDLOG_INFO("[RedisPool]Pool initialized, created {}/{} connections", created, config_.min_idle);
    return true;
}

void RedisPool::shutdown()
{
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    running_.store(false, std::memory_order_release);
    ready_.store(false, std::memory_order_release);

    // 唤醒所有等待的线程
    cond_.notify_all();

    std::lock_guard<std::mutex> lock(mutex_);

    // 关闭所有空闲连接
    for (auto* ctx : idle_conns_) {
        if (ctx) {
            redisFree(ctx);
        }
    }
    idle_conns_.clear();

    // 关闭所有活跃连接
    for (auto* ctx : active_conns_) {
        if (ctx) {
            redisFree(ctx);
        }
    }
    active_conns_.clear();

    SPDLOG_INFO("[RedisPool]Pool shut down");
}

RedisConnectionGuard RedisPool::acquire()
{
    if (!running_.load(std::memory_order_acquire) || !ready_.load(std::memory_order_acquire)) {
        SPDLOG_ERROR("[RedisPool]Pool not ready");
        return RedisConnectionGuard(this, nullptr);
    }

    std::unique_lock<std::mutex> lock(mutex_);

    // 尝试获取空闲连接
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(config_.max_wait_ms);

    while (true) {
        // 从空闲列表取出连接
        while (!idle_conns_.empty()) {
            redisContext* ctx = idle_conns_.back();
            idle_conns_.pop_back();

            // 验证连接
            lock.unlock();
            bool valid = validate_connection(ctx);
            lock.lock();

            if (valid) {
                active_conns_.insert(ctx);
                SPDLOG_DEBUG("[RedisPool]Connection acquired, idle={}, active={}",
                             idle_conns_.size(), active_conns_.size());
                return RedisConnectionGuard(this, ctx);
            }

            // 连接无效，释放并尝试下一个
            redisFree(ctx);
        }

        // 空闲列表为空，尝试创建新连接
        if (static_cast<int>(idle_conns_.size() + active_conns_.size()) < config_.pool_size) {
            lock.unlock();
            redisContext* ctx = create_connection();
            lock.lock();

            if (ctx) {
                active_conns_.insert(ctx);
                SPDLOG_DEBUG("[RedisPool]New connection created on acquire, idle={}, active={}",
                             idle_conns_.size(), active_conns_.size());
                return RedisConnectionGuard(this, ctx);
            }
        }

        // 无法创建新连接，等待归还
        if (std::chrono::steady_clock::now() >= deadline) {
            SPDLOG_WARN("[RedisPool]Acquire timeout after {}ms, idle={}, active={}",
                        config_.max_wait_ms, idle_conns_.size(), active_conns_.size());
            return RedisConnectionGuard(this, nullptr);
        }

        // 等待直到有连接归还或超时
        cond_.wait_until(lock, deadline);
    }
}

void RedisPool::release(redisContext* context)
{
    if (!context) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 从活跃集合移除
    active_conns_.erase(context);

    if (!running_.load(std::memory_order_acquire)) {
        // 池已关闭，直接释放
        redisFree(context);
        return;
    }

    // 检查连接是否有效
    if (context->err) {
        SPDLOG_WARN("[RedisPool]Releasing invalid connection, freeing");
        redisFree(context);
    } else {
        idle_conns_.push_back(context);
        SPDLOG_DEBUG("[RedisPool]Connection returned, idle={}, active={}",
                     idle_conns_.size(), active_conns_.size());
    }

    // 通知等待的线程
    cond_.notify_one();
}

int RedisPool::available() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(idle_conns_.size());
}

int RedisPool::total() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(idle_conns_.size() + active_conns_.size());
}

redisContext* RedisPool::create_connection()
{
    SPDLOG_DEBUG("[RedisPool]Creating connection to {}:{}", host_, port_);

    // 设置连接超时
    struct timeval timeout;
    timeout.tv_sec = config_.connect_timeout_ms / 1000;
    timeout.tv_usec = (config_.connect_timeout_ms % 1000) * 1000;

    redisContext* ctx = redisConnectWithTimeout(host_.c_str(), port_, timeout);
    if (!ctx) {
        SPDLOG_ERROR("[RedisPool]Failed to allocate redis context");
        return nullptr;
    }

    if (ctx->err) {
        SPDLOG_ERROR("[RedisPool]Connection failed: {}", ctx->errstr);
        redisFree(ctx);
        return nullptr;
    }

    // 设置命令超时
    struct timeval cmd_timeout;
    cmd_timeout.tv_sec = config_.command_timeout_ms / 1000;
    cmd_timeout.tv_usec = (config_.command_timeout_ms % 1000) * 1000;
    redisSetTimeout(ctx, cmd_timeout);

    // PING 验证
    redisReply* reply = static_cast<redisReply*>(redisCommand(ctx, "PING"));
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        SPDLOG_ERROR("[RedisPool]PING failed on new connection");
        if (reply) {
            freeReplyObject(reply);
        }
        redisFree(ctx);
        return nullptr;
    }
    freeReplyObject(reply);

    SPDLOG_DEBUG("[RedisPool]Connection created successfully");
    return ctx;
}

bool RedisPool::validate_connection(redisContext* context)
{
    if (!context || context->err) {
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(redisCommand(context, "PING"));
    if (!reply) {
        return false;
    }

    bool valid = (reply->type == REDIS_REPLY_STATUS &&
                  std::string(reply->str, reply->len) == "PONG");
    freeReplyObject(reply);
    return valid;
}

bool RedisPool::parse_uri(const std::string& uri, std::string& host, int& port) const
{
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
        try {
            port = std::stoi(uri_str.substr(colon_pos + 1));
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[RedisPool]Failed to parse port from URI: {}, error: {}", uri, e.what());
            return false;
        }
    } else {
        host = uri_str;
        port = 6379;
    }

    if (host.empty()) {
        return false;
    }

    return true;
}

}  // namespace farm
