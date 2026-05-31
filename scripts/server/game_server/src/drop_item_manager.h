#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>
#include <ctime>

namespace farm {

// Drop item lifetime in seconds
static constexpr int DROP_ITEM_LIFETIME_SEC = 300;

// Auto-pickup distance in pixels
static constexpr float AUTO_PICKUP_DISTANCE = 48.0f;

// Drop item position offset range (tiles)
static constexpr float DROP_OFFSET_RANGE = 1.5f;

/**
 * @brief A dropped item entity in the world.
 */
struct DropItem {
    uint32_t drop_id;       // Unique ID
    int32_t item_id;        // Item type
    int32_t count;          // Stack count
    float x;                // World pixel X
    float y;                // World pixel Y
    time_t spawn_time;      // When it was created
};

/**
 * @brief Manages drop items in the world.
 *
 * Handles spawning, lifetime expiry, and auto-pickup queries.
 */
class DropItemManager {
public:
    DropItemManager();

    /**
     * @brief Spawn drop items at a position with random offset.
     *
     * @param item_id Item type to drop
     * @param count Number of items
     * @param center_x Center position X (pixels)
     * @param center_y Center position Y (pixels)
     * @return List of spawned drop item IDs
     */
    std::vector<uint32_t> spawn_drops(int32_t item_id, int32_t count,
                                       float center_x, float center_y);

    /**
     * @brief Update drop items (remove expired ones).
     *
     * @param now Current time
     * @return List of expired drop item IDs
     */
    std::vector<uint32_t> update(time_t now);

    /**
     * @brief Find drop items near a player position for auto-pickup.
     *
     * @param player_x Player position X (pixels)
     * @param player_y Player position Y (pixels)
     * @return List of drop item IDs within pickup range
     */
    std::vector<uint32_t> find_nearby(float player_x, float player_y) const;

    /**
     * @brief Remove a drop item (after pickup or expiry).
     *
     * @param drop_id The drop item ID to remove
     * @return true if removed, false if not found
     */
    bool remove(uint32_t drop_id);

    /**
     * @brief Get a drop item by ID.
     *
     * @param drop_id The drop item ID
     * @return Pointer to the drop item, or std::nullopt if not found
     */
    std::optional<const DropItem*> get(uint32_t drop_id) const;

    /**
     * @brief Get all active drop items.
     */
    const std::unordered_map<uint32_t, DropItem>& get_all() const { return drops_; }

    /**
     * @brief Get the number of active drop items.
     */
    size_t count() const { return drops_.size(); }

private:
    uint32_t next_id_;
    std::unordered_map<uint32_t, DropItem> drops_;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace farm
