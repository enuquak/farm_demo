#pragma once

#include "redis_connection.h"

#include <string>
#include <vector>
#include <cstdint>

namespace farm {

enum class CacheResult : int32_t {
    SUCCESS = 0,
    NOT_FOUND = 1,
    CONNECTION_ERROR = 2,
    TIMEOUT = 3,
};

class RedisServer {
public:
    RedisServer(RedisConnection& conn);

    // 获取缓存
    CacheResult get(const std::string& key, std::vector<uint8_t>& value);

    // 设置缓存（ttl_seconds 由调用方传入）
    CacheResult set(const std::string& key, const std::vector<uint8_t>& value, int ttl_seconds);

    // 删除缓存
    CacheResult del(const std::string& key);

    // 检查连接是否可用
    bool is_available() const;

private:
    RedisConnection& conn_;
};

}  // namespace farm
