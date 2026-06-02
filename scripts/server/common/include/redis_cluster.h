#pragma once

/**
 * @file redis_cluster.h
 * @brief Redis Cluster 客户端，支持 slot 路由和 MOVED/ASK 重定向
 *
 * 基于 hiredis 同步 API 实现，支持：
 * - CRC16 slot 计算（含 hash tag 支持）
 * - 自动 MOVED/ASK 重定向处理
 * - 周期性 CLUSTER SLOTS 刷新
 * - KV / Set / Hash 操作
 * - 线程安全
 */

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// 前向声明 hiredis 和 libevent 类型
struct redisContext;
struct event_base;

namespace farm {

/**
 * @brief Redis Cluster 节点信息
 */
struct ClusterNode {
    std::string host;
    int port = 6379;
    std::string node_id;
    bool is_master = false;
    std::vector<std::pair<int, int>> slots;  // [start, end] slot ranges
};

/**
 * @brief Redis Cluster 配置
 */
struct RedisClusterConfig {
    std::string seed_uri;                      // redis://host1:port1,redis://host2:port2,...
    int connect_timeout_ms = 5000;
    int command_timeout_ms = 1000;
    int retry_interval_ms = 3000;
    int max_retry_count = 0;                   // 0 = 无限重试
};

/**
 * @brief Redis Cluster 客户端
 *
 * 实现 Redis Cluster 协议，包括 slot 路由和 MOVED/ASK 重定向处理。
 * 使用 hiredis 同步 API，所有操作线程安全。
 *
 * 使用示例:
 * @code
 *   RedisClusterConfig config;
 *   config.seed_uri = "redis://127.0.0.1:7000,redis://127.0.0.1:7001";
 *
 *   RedisClusterClient client(base, config);
 *   client.init();
 *
 *   client.set("mykey", "myvalue");
 *   std::string value;
 *   client.get("mykey", value);
 *
 *   client.shutdown();
 * @endcode
 */
class RedisClusterClient {
public:
    /**
     * @brief 构造函数
     * @param base   libevent 事件循环（用于定时刷新，可为 nullptr）
     * @param config 集群配置
     */
    RedisClusterClient(event_base* base, const RedisClusterConfig& config);

    ~RedisClusterClient();

    // 禁止拷贝和移动
    RedisClusterClient(const RedisClusterClient&) = delete;
    RedisClusterClient& operator=(const RedisClusterClient&) = delete;
    RedisClusterClient(RedisClusterClient&&) = delete;
    RedisClusterClient& operator=(RedisClusterClient&&) = delete;

    /**
     * @brief 初始化集群客户端
     * 解析 seed URI，连接节点，获取 CLUSTER SLOTS，启动刷新线程
     * @return true 成功，false 失败
     */
    bool init();

    /**
     * @brief 关闭集群客户端
     * 停止刷新线程，关闭所有连接
     */
    void shutdown();

    // ========================================================================
    // KV 操作
    // ========================================================================

    bool set(const std::string& key, const std::string& value);
    bool set(const std::string& key, const std::string& value, int ttl_seconds);
    bool get(const std::string& key, std::string& value);
    bool del(const std::string& key);

    // ========================================================================
    // Set 操作
    // ========================================================================

    bool sadd(const std::string& key, const std::string& member);
    bool srem(const std::string& key, const std::string& member);
    bool sismember(const std::string& key, const std::string& member, bool& result);
    bool smembers(const std::string& key, std::vector<std::string>& members);
    bool scard(const std::string& key, int64_t& count);

    // ========================================================================
    // Hash 操作
    // ========================================================================

    bool hset(const std::string& key, const std::string& field, const std::string& value);
    bool hget(const std::string& key, const std::string& field, std::string& value);
    bool hdel(const std::string& key, const std::string& field);
    bool hgetall(const std::string& key, std::unordered_map<std::string, std::string>& kvs);

    // ========================================================================
    // 集群状态
    // ========================================================================

    /**
     * @brief 获取集群节点列表
     */
    std::vector<ClusterNode> get_nodes() const;

    /**
     * @brief 集群是否健康（至少有一个可用连接）
     */
    bool is_healthy() const;

private:
    // ========================================================================
    // 内部工具
    // ========================================================================

    /**
     * @brief CRC16 计算（Redis Cluster 标准算法）
     * @param data 数据指针
     * @param len  数据长度
     * @return CRC16 值
     */
    static uint16_t crc16(const char* data, size_t len);

    /**
     * @brief 计算 key 对应的 slot（支持 hash tag）
     * @param key Redis key
     * @return slot 值 (0-16383)
     */
    static uint16_t key_slot(const std::string& key);

    /**
     * @brief 获取指定 slot 的连接
     * @param slot slot 值
     * @return redisContext 指针，nullptr 表示无可用连接
     */
    redisContext* get_connection(uint16_t slot);

    /**
     * @brief 刷新集群 slot 映射
     * 发送 CLUSTER SLOTS 到任意已连接节点
     * @return true 成功，false 失败
     */
    bool refresh_slots();

    /**
     * @brief 执行 Redis 命令，处理 MOVED/ASK 重定向
     * @param slot    目标 slot
     * @param format  命令格式字符串
     * @param ...     命令参数
     * @return redisReply 指针，调用者负责 freeReplyObject；nullptr 表示失败
     */
    redisReply* execute_command(uint16_t slot, const char* format, ...);

    /**
     * @brief 连接到指定节点
     * @param host 主机地址
     * @param port 端口号
     * @return redisContext 指针，nullptr 表示失败
     */
    redisContext* connect_node(const std::string& host, int port);

    /**
     * @brief 解析 seed URI
     * @param uri seed URI 字符串
     * @return 节点列表
     */
    std::vector<ClusterNode> parse_seed_uri(const std::string& uri) const;

    /**
     * @brief 解析 CLUSTER SLOTS 响应
     * @param reply CLUSTER SLOTS 的 redisReply
     * @return true 成功，false 失败
     */
    bool parse_cluster_slots(redisReply* reply);

    /**
     * @brief 获取任意可用连接（用于 CLUSTER SLOTS 等集群命令）
     * @return redisContext 指针，nullptr 表示无可用连接
     */
    redisContext* get_any_connection();

    /**
     * @brief 关闭并释放指定节点的连接
     */
    void close_connection(const std::string& node_key);

    /**
     * @brief 关闭所有连接
     */
    void close_all_connections();

    /**
     * @brief 后台刷新线程函数
     */
    void refresh_loop();

    /**
     * @brief 构造节点唯一键
     */
    static std::string make_node_key(const std::string& host, int port);

    // libevent 事件循环（可为 nullptr）
    event_base* base_;

    // 配置
    RedisClusterConfig config_;

    // slot 映射：slot -> 节点键 (host:port)
    std::string slot_map_[16384];

    // 连接池：节点键 -> redisContext
    std::unordered_map<std::string, redisContext*> connections_;

    // 集群节点信息
    std::vector<ClusterNode> nodes_;

    // 同步原语
    mutable std::mutex mutex_;

    // 状态
    std::atomic<bool> healthy_{false};
    std::atomic<bool> running_{false};

    // 后台刷新线程
    std::thread refresh_thread_;
};

}  // namespace farm
