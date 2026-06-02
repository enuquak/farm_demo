#include "route_cache.h"

namespace farm {

std::optional<uint32_t> RouteCache::get_server_id(uint64_t player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = player_to_server_.find(player_id);
    if (it != player_to_server_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void RouteCache::update(uint64_t player_id, uint32_t server_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    player_to_server_[player_id] = server_id;
}

void RouteCache::remove(uint64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    player_to_server_.erase(player_id);
}

size_t RouteCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return player_to_server_.size();
}

void RouteCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    player_to_server_.clear();
}

void RouteCache::clear_server(uint32_t server_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = player_to_server_.begin(); it != player_to_server_.end();) {
        if (it->second == server_id) {
            it = player_to_server_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace farm
