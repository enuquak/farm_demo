#include "drop_item_manager.h"
#include "log_macros.h"

#include <cmath>
#include <cstdlib>

namespace farm {

DropItemManager::DropItemManager()
    : next_id_(1)
{
}

std::vector<uint32_t> DropItemManager::spawn_drops(int32_t item_id, int32_t count,
                                                     float center_x, float center_y) {
    std::vector<uint32_t> spawned_ids;
    time_t now = std::time(nullptr);

    for (int32_t i = 0; i < count; i++) {
        // Random offset within DROP_OFFSET_RANGE tiles (32 pixels per tile)
        float offset_x = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * DROP_OFFSET_RANGE * 2.0f * 32.0f;
        float offset_y = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * DROP_OFFSET_RANGE * 2.0f * 32.0f;

        DropItem drop;
        drop.drop_id = next_id_++;
        drop.item_id = item_id;
        drop.count = 1;
        drop.x = center_x + offset_x;
        drop.y = center_y + offset_y;
        drop.spawn_time = now;

        drops_[drop.drop_id] = drop;
        spawned_ids.push_back(drop.drop_id);

        SPDLOG_INFO("[DropItemMgr]Spawned drop_id={} item_id={} at ({:.1f}, {:.1f})",
                    drop.drop_id, item_id, drop.x, drop.y);
    }

    return spawned_ids;
}

std::vector<uint32_t> DropItemManager::update(time_t now) {
    std::vector<uint32_t> expired;

    for (auto it = drops_.begin(); it != drops_.end(); ) {
        if (now - it->second.spawn_time >= DROP_ITEM_LIFETIME_SEC) {
            SPDLOG_INFO("[DropItemMgr]Drop expired: drop_id={} item_id={}",
                        it->second.drop_id, it->second.item_id);
            expired.push_back(it->second.drop_id);
            it = drops_.erase(it);
        } else {
            ++it;
        }
    }

    return expired;
}

std::vector<uint32_t> DropItemManager::find_nearby(float player_x, float player_y) const {
    std::vector<uint32_t> nearby;
    float pickup_dist_sq = AUTO_PICKUP_DISTANCE * AUTO_PICKUP_DISTANCE;

    for (const auto& kv : drops_) {
        float dx = kv.second.x - player_x;
        float dy = kv.second.y - player_y;
        float dist_sq = dx * dx + dy * dy;
        if (dist_sq < pickup_dist_sq) {
            nearby.push_back(kv.first);
        }
    }

    return nearby;
}

bool DropItemManager::remove(uint32_t drop_id) {
    return drops_.erase(drop_id) > 0;
}

const DropItem* DropItemManager::get(uint32_t drop_id) const {
    auto it = drops_.find(drop_id);
    if (it != drops_.end()) {
        return &it->second;
    }
    return nullptr;
}

}  // namespace farm
