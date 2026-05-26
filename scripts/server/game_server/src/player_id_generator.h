#pragma once

#include <cstdint>
#include <unordered_map>
#include <mutex>

namespace farm {

// Player ID 生成器
// 格式: (server_id << 20) | sequence
// 支持最多 4096 个服务器，每个服务器 1048576 个玩家
class PlayerIdGenerator {
public:
    PlayerIdGenerator() = default;
    ~PlayerIdGenerator() = default;

    // 生成唯一的 player_id
    uint64_t generate(uint32_t server_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sequences_.find(server_id);
        if (it == sequences_.end()) {
            sequences_[server_id] = 1;
        } else {
            it->second++;
        }
        return (static_cast<uint64_t>(server_id) << 20) | sequences_[server_id];
    }

private:
    std::unordered_map<uint32_t, uint64_t> sequences_;
    std::mutex mutex_;
};

}  // namespace farm
