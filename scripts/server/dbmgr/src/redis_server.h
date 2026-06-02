#pragma once

#include "redis_pool.h"
#include "db_types.h"

#include <string>
#include <vector>
#include <cstdint>

namespace farm {

class RedisServer {
public:
    explicit RedisServer(RedisPool& pool);

    // 获取缓存
    CacheResult get(const std::string& key, std::vector<uint8_t>& value);

    // 设置缓存（ttl_seconds 由调用方传入）
    CacheResult set(const std::string& key, const std::vector<uint8_t>& value, int ttl_seconds);

    // 删除缓存
    CacheResult del(const std::string& key);

    // 检查连接池是否可用
    bool is_available() const;

private:
    RedisPool& pool_;
};

}  // namespace farm
