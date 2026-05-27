/**
 * @file scene_state.h
 * @brief Server-side scene state management
 *
 * Bundles WorldState, CropSystem, DropItemManager for each scene.
 * Supports freeze/thaw mechanism for scene lifecycle.
 */
#pragma once

#include "world_state.h"
#include "crop_system.h"
#include "drop_item_manager.h"

#include <ctime>
#include <string>
#include <cstdint>

namespace farm {

/**
 * @brief Scene configuration definition
 */
struct SceneDef {
    std::string scene_id;
    int width;
    int height;
    int spawn_x;    // Default spawn tile X
    int spawn_y;    // Default spawn tile Y
};

/**
 * @brief Server-side scene state
 *
 * Contains all data for a single scene: tile map, crop system, drop items.
 * Supports freeze/thaw for performance optimization when no players are present.
 */
class SceneState {
public:
    SceneState(const std::string& scene_id, int width, int height);
    ~SceneState() = default;

    const std::string& scene_id() const { return scene_id_; }
    int width() const { return width_; }
    int height() const { return height_; }

    WorldState& world_state() { return world_state_; }
    const WorldState& world_state() const { return world_state_; }

    CropSystem& crop_system() { return crop_system_; }
    const CropSystem& crop_system() const { return crop_system_; }

    DropItemManager& drop_manager() { return drop_manager_; }
    const DropItemManager& drop_manager() const { return drop_manager_; }

    // Player count management
    int player_count() const { return player_count_; }
    void add_player();
    void remove_player();

    // Freeze/thaw
    bool is_frozen() const { return frozen_at_ != 0; }
    time_t frozen_at() const { return frozen_at_; }
    void freeze();
    void thaw();

    // Generate default map data for this scene
    void generate_default();

    // Serialize/deserialize for persistence
    std::string serialize() const;
    bool deserialize(const std::string& json_str);

private:
    std::string scene_id_;
    int width_;
    int height_;
    WorldState world_state_;
    CropSystem crop_system_;
    DropItemManager drop_manager_;
    int player_count_ = 0;
    time_t frozen_at_ = 0;
};

}  // namespace farm
