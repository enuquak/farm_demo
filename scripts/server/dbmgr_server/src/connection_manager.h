#pragma once

#include "mongo_replica_set.h"
#include "redis_pool.h"
#include "db_types.h"

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace farm {

// 连接配置（使用新的配置类型）
using MongoConfig = MongoReplicaSetConfig;
using RedisConfig = RedisPoolConfig;

class ConnectionManager {
public:
    ConnectionManager(const MongoReplicaSetConfig& mongo_config, const RedisPoolConfig& redis_config);
    ~ConnectionManager();

    // 禁止拷贝
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // 初始化：尝试连接，失败则启动后台重试
    bool init();

    // 关闭：停止重试线程，断开连接
    void shutdown();

    // 是否所有连接就绪
    bool is_ready() const;

    // 获取连接状态
    ConnectionState state() const;

    // 获取连接对象
    MongoReplicaSetConnection& mongo_connection();
    RedisPool& redis_pool();

    // 写后读一致性：切到 primary 读
    void read_from_primary_after_write();

    // 恢复默认 read preference
    void restore_read_preference();

    // 状态变化回调（DbMgrServer 注册）
    void set_on_ready_callback(std::function<void()> callback);

private:
    // 尝试连接 MongoDB
    bool try_connect_mongo();

    // 尝试连接 Redis
    bool try_connect_redis();

    // 后台重试线程函数
    void retry_thread_func();

    // 更新状态
    void update_state();

    MongoReplicaSetConfig mongo_config_;
    RedisPoolConfig redis_config_;

    MongoReplicaSetConnection mongo_conn_;
    RedisPool redis_pool_;

    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    std::atomic<bool> running_{false};
    std::thread retry_thread_;
    std::mutex mutex_;

    // 回调
    std::function<void()> on_ready_callback_;

    // 重试计数
    int mongo_retry_count_ = 0;
    int redis_retry_count_ = 0;
};

}  // namespace farm
