#pragma once

/**
 * @file redis_server.h
 * @brief Redis 服务层，支持普通连接池和集群两种模式
 *
 * 对外提供统一的 Redis 操作接口，内部根据构造方式自动选择
 * RedisPool（普通模式）或 RedisClusterClient（集群模式）。
 *
 * 普通模式：通过 RAII RedisConnectionGuard 管理连接
 * 集群模式：直接调用 RedisClusterClient 接口（内部已处理 slot 路由和重定向）
 */

#include "redis_pool.h"
#include "redis_cluster.h"
#include "db_types.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace farm {

class RedisServer {
public:
    /**
     * @brief 普通模式构造函数
     * @param pool Redis 连接池引用
     */
    explicit RedisServer(RedisPool& pool);

    /**
     * @brief 集群模式构造函数
     * @param cluster Redis 集群客户端引用
     */
    explicit RedisServer(RedisClusterClient& cluster);

    // 禁止拷贝
    RedisServer(const RedisServer&) = delete;
    RedisServer& operator=(const RedisServer&) = delete;

    // ========================================================================
    // KV 操作
    // ========================================================================

    CacheResult get(const std::string& key, std::string& value);
    CacheResult get(const std::string& key, std::vector<uint8_t>& value);
    CacheResult set(const std::string& key, const std::string& value, int ttl_seconds);
    CacheResult set(const std::string& key, const std::vector<uint8_t>& value, int ttl_seconds);
    CacheResult del(const std::string& key);

    // ========================================================================
    // Set 操作
    // ========================================================================

    CacheResult sadd(const std::string& key, const std::string& member);
    CacheResult srem(const std::string& key, const std::string& member);
    CacheResult sismember(const std::string& key, const std::string& member, bool& result);
    CacheResult smembers(const std::string& key, std::vector<std::string>& members);
    CacheResult scard(const std::string& key, int64_t& count);

    // ========================================================================
    // Hash 操作
    // ========================================================================

    CacheResult hset(const std::string& key, const std::string& field, const std::string& value);
    CacheResult hget(const std::string& key, const std::string& field, std::string& value);
    CacheResult hdel(const std::string& key, const std::string& field);
    CacheResult hgetall(const std::string& key, std::unordered_map<std::string, std::string>& kvs);

    // ========================================================================
    // 状态查询
    // ========================================================================

    bool is_available() const;

private:
    RedisPool* pool_ = nullptr;
    RedisClusterClient* cluster_ = nullptr;
    bool use_cluster_ = false;
};

}  // namespace farm
