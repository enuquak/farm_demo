#include "rate_limiter.h"

namespace farm {

std::string RateLimiter::make_key(uint64_t player_id, uint32_t channel_type) {
    return std::to_string(player_id) + ":" + std::to_string(channel_type);
}

bool RateLimiter::check_limit(uint64_t player_id, uint32_t channel_type, float cooldown_sec) {
    auto it = last_send_.find(make_key(player_id, channel_type));
    if (it == last_send_.end()) {
        return true;  // No previous send
    }
    auto elapsed = std::chrono::duration<float>(Clock::now() - it->second).count();
    return elapsed >= cooldown_sec;
}

void RateLimiter::update(uint64_t player_id, uint32_t channel_type) {
    last_send_[make_key(player_id, channel_type)] = Clock::now();
}

void RateLimiter::remove_player(uint64_t player_id) {
    // Remove all channel entries for this player
    for (uint32_t ch = 0; ch <= 2; ++ch) {
        last_send_.erase(make_key(player_id, ch));
    }
}

}  // namespace farm
