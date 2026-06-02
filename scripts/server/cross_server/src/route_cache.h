#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <mutex>

namespace farm {

class RouteCache {
public:
    RouteCache() = default;
    ~RouteCache() = default;

    // 禁止拷贝
    RouteCache(const RouteCache&) = delete;
    RouteCache& operator=(const RouteCache&) = delete;

    // 查询玩家所在 Server
    std::optional<uint32_t> get_server_id(uint64_t player_id) const;

    // 更新玩家路由
    void update(uint64_t player_id, uint32_t server_id);

    // 删除玩家路由
    void remove(uint64_t player_id);

    // 获取缓存大小
    size_t size() const;

    // 清空缓存
    void clear();

    // 清空指定 Server 的所有路由
    void clear_server(uint32_t server_id);

private:
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, uint32_t> player_to_server_;
};

}  // namespace farm
