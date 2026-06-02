#pragma once

/**
 * @file redis_pool.h
 * @brief Redis 连接池，RAII 管理 hiredis 连接
 *
 * 提供线程安全的连接池，支持：
 * - 阻塞获取连接（带超时）
 * - RAII 自动归还连接（RedisConnectionGuard）
 * - 空闲连接验证（PING）
 * - 可配置池大小、超时、重试参数
 */

#include <hiredis/hiredis.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace farm {

// 前向声明
class RedisPool;

/**
 * @brief Redis 连接池配置
 */
struct RedisPoolConfig {
    std::string uri;                       // redis://host:port
    int pool_size = 8;                     // 最大连接数
    int min_idle = 2;                      // 最小空闲连接数
    int max_wait_ms = 3000;                // 获取连接最大等待时间（毫秒）
    int connect_timeout_ms = 5000;         // 连接超时（毫秒）
    int command_timeout_ms = 1000;         // 命令超时（毫秒）
    int retry_interval_ms = 3000;          // 重试间隔（毫秒）
    int max_retry_count = 0;               // 最大重试次数（0 = 无限重试）
    bool cluster_mode = false;             // 是否集群模式（预留）
};

/**
 * @brief RAII Redis 连守卫
 *
 * 构造时从池中获取连接，析构时自动归还。
 * 不可拷贝，可移动。
 */
class RedisConnectionGuard {
public:
    /**
     * @brief 构造函数
     * @param pool    连接池指针
     * @param context hiredis 上下文
     */
    RedisConnectionGuard(RedisPool* pool, redisContext* context);

    /**
     * @brief 析构函数，自动归还连接到池
     */
    ~RedisConnectionGuard();

    // 禁止拷贝
    RedisConnectionGuard(const RedisConnectionGuard&) = delete;
    RedisConnectionGuard& operator=(const RedisConnectionGuard&) = delete;

    // 允许移动
    RedisConnectionGuard(RedisConnectionGuard&& other) noexcept;
    RedisConnectionGuard& operator=(RedisConnectionGuard&& other) noexcept;

    /**
     * @brief 获取底层 redisContext
     */
    redisContext* context() const { return context_; }

    /**
     * @brief 检查连接是否有效
     */
    bool is_valid() const;

private:
    RedisPool* pool_ = nullptr;
    redisContext* context_ = nullptr;
};

/**
 * @brief Redis 连接池
 *
 * 线程安全的连接池，支持阻塞获取和 RAII 归还。
 * 初始化时创建 min_idle 个连接，运行时按需创建直到 pool_size。
 */
class RedisPool {
public:
    explicit RedisPool(const RedisPoolConfig& config);
    ~RedisPool();

    // 禁止拷贝和移动
    RedisPool(const RedisPool&) = delete;
    RedisPool& operator=(const RedisPool&) = delete;
    RedisPool(RedisPool&&) = delete;
    RedisPool& operator=(RedisPool&&) = delete;

    /**
     * @brief 初始化连接池，创建 min_idle 个连接
     * @return true 成功，false 失败（无法创建任何连接）
     */
    bool init();

    /**
     * @brief 关闭连接池，释放所有连接
     */
    void shutdown();

    /**
     * @brief 获取一个连接（阻塞，带超时）
     * @return RedisConnectionGuard（RAII），获取失败返回 guard 且 context() == nullptr
     */
    RedisConnectionGuard acquire();

    /**
     * @brief 归还连接到池
     * @param context 要归还的 redisContext
     */
    void release(redisContext* context);

    /**
     * @brief 当前空闲连接数
     */
    int available() const;

    /**
     * @brief 当前总连接数（空闲 + 活跃）
     */
    int total() const;

    /**
     * @brief 连接池是否就绪
     */
    bool is_ready() const { return ready_.load(std::memory_order_acquire); }

private:
    /**
     * @brief 创建一个新连接
     * @return redisContext 指针，失败返回 nullptr
     */
    redisContext* create_connection();

    /**
     * @brief 验证连接是否有效（PING 检查）
     * @param context 要验证的 redisContext
     * @return true 有效，false 无效
     */
    bool validate_connection(redisContext* context);

    /**
     * @brief 解析 redis://host:port URI
     * @param uri     输入 URI
     * @param host    输出 host
     * @param port    输出 port
     * @return true 解析成功，false 解析失败
     */
    bool parse_uri(const std::string& uri, std::string& host, int& port) const;

    // 配置
    RedisPoolConfig config_;

    // 解析后的连接参数
    std::string host_;
    int port_ = 6379;

    // 连接存储
    std::vector<redisContext*> idle_conns_;
    std::unordered_set<redisContext*> active_conns_;

    // 同步原语
    mutable std::mutex mutex_;
    std::condition_variable cond_;

    // 状态
    std::atomic<bool> running_{false};
    std::atomic<bool> ready_{false};
};

}  // namespace farm
