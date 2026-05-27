#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <unordered_map>

namespace farm {

class WorldState;

// Default crop grow time in seconds
static constexpr int DEFAULT_CROP_GROW_TIME = 60;

/**
 * @brief Crop growth data for a single planted tile.
 */
struct CropData {
    time_t planted_at;
    int grow_time;  // seconds
};

/**
 * @brief Server-side crop growth system.
 *
 * Manages planted crops from CROP_GROWING -> CROP_READY lifecycle.
 * Driven by server-side timer, supports scene freeze/resume catch-up.
 *
 * Serialization format:
 *   {"tiles": {"30,20": {"planted_at": 1743678050, "grow_time": 60}, ...}}
 */
class CropSystem {
public:
    CropSystem();

    /**
     * @brief Register a crop at the given tile.
     *
     * Validates that the target tile ground is TILLED before registering.
     *
     * @param x Tile X coordinate
     * @param y Tile Y coordinate
     * @param world WorldState for ground validation
     * @param planted_at Timestamp of planting (default: now)
     * @param grow_time Growth time in seconds (default: 60)
     * @return true if registered successfully
     */
    bool register_crop(int x, int y, WorldState* world,
                       time_t planted_at = 0, int grow_time = DEFAULT_CROP_GROW_TIME);

    /**
     * @brief Update all growing crops. Called periodically by the server.
     *
     * Checks each crop: if now - planted_at >= grow_time, transitions
     * the tile from CROP_GROWING to CROP_READY and removes from growing_tiles.
     * Also cleans up invalid tiles (where object is no longer CROP_GROWING).
     *
     * @param world WorldState for tile queries and updates
     */
    void update(WorldState* world);

    /**
     * @brief Simulate elapsed time for scene freeze/resume catch-up.
     *
     * When a scene is frozen and then resumed, this method processes
     * all pending crop maturations that occurred during the freeze.
     *
     * @param elapsed Seconds elapsed since freeze
     * @param world WorldState for tile queries and updates
     */
    void simulate_elapsed(time_t elapsed, WorldState* world);

    /**
     * @brief Remove crop data for a tile (e.g., after harvesting).
     *
     * @param x Tile X coordinate
     * @param y Tile Y coordinate
     */
    void remove_crop(int x, int y);

    /**
     * @brief Check if a tile has growing crop data.
     */
    bool has_crop(int x, int y) const;

    /**
     * @brief Serialize crop data to JSON string.
     *
     * Format: {"tiles": {"x,y": {"planted_at": ..., "grow_time": 60}, ...}}
     */
    std::string serialize() const;

    /**
     * @brief Deserialize crop data from JSON string.
     *
     * If data is empty or has no "crops"/"tiles" field, initializes empty.
     * Compatible with old saves that lack crop data.
     */
    bool deserialize(const std::string& json_str);

    /**
     * @brief Get the number of growing crops.
     */
    size_t count() const { return growing_tiles_.size(); }

private:
    // Key: "x,y", Value: crop growth data
    std::unordered_map<std::string, CropData> growing_tiles_;

    // Helper to make tile key
    static std::string make_key(int x, int y);

    // Freeze timestamp (0 = not frozen)
    time_t frozen_at_;
};

}  // namespace farm
