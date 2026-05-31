#pragma once

#include <string>
#include <hiredis/hiredis.h>

namespace farm {

class RedisConnection {
public:
    RedisConnection();
    ~RedisConnection();

    // 禁止拷贝
    RedisConnection(const RedisConnection&) = delete;
    RedisConnection& operator=(const RedisConnection&) = delete;

    // 连接 Redis（uri 格式: redis://host:port 或 tcp://host:port）
    bool connect(const std::string& uri);

    // 断开连接
    void disconnect();

    // 是否已连接
    bool is_connected() const;

    // 获取底层 context（供 RedisServer 使用）
    redisContext* context();

private:
    redisContext* context_ = nullptr;
    bool connected_ = false;
};

}  // namespace farm
