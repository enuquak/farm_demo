#pragma once

/**
 * @file redis_async.h
 * @brief 异步 Redis 客户端，基于 hiredis async API + libevent 事件循环
 *
 * 提供非阻塞的 Redis 操作，所有命令通过回调返回结果。
 * 集成 libevent 事件循环，适合单线程事件驱动架构。
 *
 * 支持的操作：
 * - KV 操作：SET、GET、DEL（带 TTL 支持）
 * - Set 操作：SADD、SREM、SISMEMBER、SMEMBERS、SCARD
 * - Hash 操作：HSET、HGET、HDEL、HGETALL
 */

#include <event2/event.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// 前向声明 hiredis 类型
struct redisAsyncContext;

namespace farm {

/**
 * @brief 单值操作回调
 * @param success 操作是否成功
 * @param result  成功时返回结果字符串，失败时返回错误描述
 */
using RedisCallback = std::function<void(bool success, const std::string& result)>;

/**
 * @brief 数组操作回调
 * @param success 操作是否成功
 * @param results 成功时返回结果数组，失败时为空
 */
using RedisArrayCallback = std::function<void(bool success, const std::vector<std::string>& results)>;

/**
 * @brief 异步 Redis 客户端配置
 */
struct RedisAsyncConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
    int connect_timeout_ms = 5000;
    int command_timeout_ms = 1000;
};

/**
 * @brief 异步 Redis 客户端
 *
 * 基于 hiredis async API 实现，集成 libevent 事件循环。
 * 所有命令均为非阻塞，通过回调返回结果。
 *
 * 使用示例:
 * @code
 *   RedisAsyncConfig config;
 *   config.host = "127.0.0.1";
 *   config.port = 6379;
 *
 *   RedisAsyncClient client(event_base, config);
 *   client.connect();
 *
 *   client.set("key", "value", 60, [](bool ok, const std::string& res) {
 *       if (ok) SPDLOG_INFO("SET success");
 *   });
 *
 *   client.get("key", [](bool ok, const std::string& res) {
 *       if (ok) SPDLOG_INFO("GET result: {}", res);
 *   });
 * @endcode
 */
class RedisAsyncClient {
public:
    /**
     * @brief 构造函数
     * @param base   libevent 事件循环指针
     * @param config Redis 连接配置
     */
    RedisAsyncClient(event_base* base, const RedisAsyncConfig& config);

    /**
     * @brief 析构函数，自动断开连接
     */
    ~RedisAsyncClient();

    // 禁止拷贝和移动
    RedisAsyncClient(const RedisAsyncClient&) = delete;
    RedisAsyncClient& operator=(const RedisAsyncClient&) = delete;
    RedisAsyncClient(RedisAsyncClient&&) = delete;
    RedisAsyncClient& operator=(RedisAsyncClient&&) = delete;

    /**
     * @brief 发起异步连接
     * @return true 连接请求已发起，false 连接请求失败
     */
    bool connect();

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 检查是否已连接
     */
    bool is_connected() const;

    // ========================================================================
    // KV 操作
    // ========================================================================

    /**
     * @brief 异步 SET 命令
     * @param key   键名
     * @param value 值
     * @param ttl   过期时间（秒），0 表示不设置过期
     * @param cb    完成回调
     */
    void set(const std::string& key, const std::string& value, int ttl, RedisCallback cb);

    /**
     * @brief 异步 GET 命令
     * @param key 键名
     * @param cb  完成回调
     */
    void get(const std::string& key, RedisCallback cb);

    /**
     * @brief 异步 DEL 命令
     * @param key 键名
     * @param cb  完成回调
     */
    void del(const std::string& key, RedisCallback cb);

    // ========================================================================
    // Set 操作
    // ========================================================================

    /**
     * @brief 异步 SADD 命令（向集合添加成员）
     * @param key    集合键名
     * @param member 要添加的成员
     * @param cb     完成回调
     */
    void sadd(const std::string& key, const std::string& member, RedisCallback cb);

    /**
     * @brief 异步 SREM 命令（从集合移除成员）
     * @param key    集合键名
     * @param member 要移除的成员
     * @param cb     完成回调
     */
    void srem(const std::string& key, const std::string& member, RedisCallback cb);

    /**
     * @brief 异步 SISMEMBER 命令（检查成员是否在集合中）
     * @param key    集合键名
     * @param member 要检查的成员
     * @param cb     完成回调
     */
    void sismember(const std::string& key, const std::string& member, RedisCallback cb);

    /**
     * @brief 异步 SMEMBERS 命令（获取集合所有成员）
     * @param key 集合键名
     * @param cb  完成回调
     */
    void smembers(const std::string& key, RedisArrayCallback cb);

    /**
     * @brief 异步 SCARD 命令（获取集合成员数量）
     * @param key 集合键名
     * @param cb  完成回调
     */
    void scard(const std::string& key, RedisCallback cb);

    // ========================================================================
    // Hash 操作
    // ========================================================================

    /**
     * @brief 异步 HSET 命令（设置哈希字段值）
     * @param key   哈希键名
     * @param field 字段名
     * @param value 字段值
     * @param cb    完成回调
     */
    void hset(const std::string& key, const std::string& field, const std::string& value, RedisCallback cb);

    /**
     * @brief 异步 HGET 命令（获取哈希字段值）
     * @param key   哈希键名
     * @param field 字段名
     * @param cb    完成回调
     */
    void hget(const std::string& key, const std::string& field, RedisCallback cb);

    /**
     * @brief 异步 HDEL 命令（删除哈希字段）
     * @param key   哈希键名
     * @param field 字段名
     * @param cb    完成回调
     */
    void hdel(const std::string& key, const std::string& field, RedisCallback cb);

    /**
     * @brief 异步 HGETALL 命令（获取哈希所有字段和值）
     * @param key 哈希键名
     * @param cb  完成回调
     */
    void hgetall(const std::string& key, RedisArrayCallback cb);

private:
    /**
     * @brief 连接成功回调（hiredis 静态回调）
     */
    static void on_connect(const redisAsyncContext* ac, int status);

    /**
     * @brief 断开连接回调（hiredis 静态回调）
     */
    static void on_disconnect(const redisAsyncContext* ac, int status);

    /**
     * @brief 命令完成回调（hiredis 静态回调）
     */
    static void on_command(redisAsyncContext* ac, void* reply, void* privdata);

    // libevent 事件循环
    event_base* base_;

    // hiredis 异步上下文
    redisAsyncContext* async_ctx_;

    // 连接状态
    std::atomic<bool> connected_;

    // 配置
    RedisAsyncConfig config_;
};

}  // namespace farm
