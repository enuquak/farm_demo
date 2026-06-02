#pragma once

#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <string>

namespace farm {

class RateLimiter {
public:
    RateLimiter() = default;
    ~RateLimiter() = default;

    // Check if player can send on this channel. Returns true if allowed.
    bool check_limit(uint64_t player_id, uint32_t channel_type, float cooldown_sec);

    // Record that player just sent on this channel.
    void update(uint64_t player_id, uint32_t channel_type);

    // Remove all entries for a player (on disconnect).
    void remove_player(uint64_t player_id);

private:
    static std::string make_key(uint64_t player_id, uint32_t channel_type);

    using Clock = std::chrono::steady_clock;
    std::unordered_map<std::string, Clock::time_point> last_send_;
};

}  // namespace farm
