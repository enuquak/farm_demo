#pragma once

#include "mongo_connection.h"
#include "redis_connection.h"

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace farm {

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED,           // 重试中
    FAILED_PERMANENT  // 超过最大重试次数
};

// 连接配置
struct MongoConfig {
    std::string uri;
    int retry_interval_ms = 3000;
    int max_retry_count = 0;  // 0 = 无限重试
};

struct RedisConfig {
    std::string uri;
    int retry_interval_ms = 3000;
    int max_retry_count = 0;  // 0 = 无限重试
};

class ConnectionManager {
public:
    ConnectionManager(const MongoConfig& mongo_config, const RedisConfig& redis_config);
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
    MongoConnection& mongo_connection();
    RedisConnection& redis_connection();

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

    MongoConfig mongo_config_;
    RedisConfig redis_config_;

    MongoConnection mongo_conn_;
    RedisConnection redis_conn_;

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
