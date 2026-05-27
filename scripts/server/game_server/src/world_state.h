#pragma once

#include "item_effects.h"

#include <cstdint>
#include <string>
#include <vector>
#include <random>

namespace farm {

// Default map dimensions
static constexpr int DEFAULT_MAP_WIDTH = 60;
static constexpr int DEFAULT_MAP_HEIGHT = 50;

/**
 * @brief Server-side world state (tile map).
 *
 * Manages ground and object layers for the farm scene.
 * The authoritative source for tile data used in item interaction validation.
 */
class WorldState {
public:
    WorldState(int width = DEFAULT_MAP_WIDTH, int height = DEFAULT_MAP_HEIGHT);

    int width() const { return width_; }
    int height() const { return height_; }

    // Ground layer access
    GroundType get_ground(int x, int y) const;
    void set_ground(int x, int y, GroundType type);

    // Object layer access
    ObjectType get_object(int x, int y) const;
    void set_object(int x, int y, ObjectType type);

    // Check if coordinates are valid
    bool is_valid(int x, int y) const;

    // Serialize to JSON string (for farm_state persistence)
    std::string serialize() const;

    // Deserialize from JSON string
    bool deserialize(const std::string& json_str);

    // Generate default map (same seed as client for consistency)
    void generate_default();

private:
    int width_;
    int height_;
    std::vector<std::vector<uint8_t>> ground_;   // Ground layer
    std::vector<std::vector<uint8_t>> objects_;   // Object layer
};

}  // namespace farm
