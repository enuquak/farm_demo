/**
 * @file scene_state.cpp
 * @brief Server-side scene state implementation
 */
#include "scene_state.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>
#include <ctime>
#include <random>

namespace farm {

SceneState::SceneState(const std::string& scene_id, int width, int height)
    : scene_id_(scene_id)
    , width_(width)
    , height_(height)
    , world_state_(width, height)
{
}

void SceneState::add_player() {
    player_count_++;
    if (frozen_at_ != 0) {
        thaw();
    }
}

void SceneState::remove_player() {
    if (player_count_ > 0) {
        player_count_--;
    }
    if (player_count_ == 0 && frozen_at_ == 0) {
        freeze();
    }
}

void SceneState::freeze() {
    frozen_at_ = std::time(nullptr);
    SPDLOG_INFO("[SceneState]Scene {} frozen at {}", scene_id_, frozen_at_);
}

void SceneState::thaw() {
    if (frozen_at_ == 0) {
        return;
    }

    time_t now = std::time(nullptr);
    time_t elapsed = now - frozen_at_;

    if (elapsed > 0) {
        crop_system_.simulate_elapsed(elapsed, &world_state_);
        SPDLOG_INFO("[SceneState]Scene {} thawed after {} seconds", scene_id_, elapsed);
    }

    frozen_at_ = 0;
}

void SceneState::generate_default() {
    // Use the same seed as the client for consistency
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            float r = dist(rng);
            uint8_t ground_id;
            if (r < 0.70f) {
                ground_id = static_cast<uint8_t>(GroundType::GRASS);
            } else if (r < 0.80f) {
                ground_id = static_cast<uint8_t>(GroundType::DIRT);
            } else if (r < 0.88f) {
                ground_id = static_cast<uint8_t>(GroundType::SAND);
            } else if (r < 0.93f) {
                ground_id = static_cast<uint8_t>(GroundType::TILLED);
            } else {
                ground_id = static_cast<uint8_t>(GroundType::WATER);
            }
            world_state_.set_ground(x, y, static_cast<GroundType>(ground_id));
        }
    }

    // Stones
    int stone_positions[][2] = {
        {1,0}, {5,5}, {10,8}, {15,12}, {20,3}, {25,15},
        {30,10}, {35,20}, {40,5}, {45,18}, {50,8}
    };
    for (auto& pos : stone_positions) {
        int sx = pos[0], sy = pos[1];
        if (world_state_.is_valid(sx, sy)) {
            world_state_.set_object(sx, sy, ObjectType::STONE);
        }
    }

    // Trees
    int tree_positions[][2] = {
        {2,2}, {3,2}, {4,2}, {2,3}, {3,3}, {4,3},
        {width_-3, 2}, {width_-2, 2},
        {width_-3, 3}, {width_-2, 3}
    };
    for (auto& pos : tree_positions) {
        int tx = pos[0], ty = pos[1];
        if (world_state_.is_valid(tx, ty)) {
            world_state_.set_object(tx, ty, ObjectType::TREE);
        }
    }

    // Doors
    int door_positions[][2] = {{8, 8}, {52, 42}};
    for (auto& pos : door_positions) {
        int dx = pos[0], dy = pos[1];
        if (world_state_.is_valid(dx, dy)) {
            world_state_.set_object(dx, dy, ObjectType::DOOR_IN);
        }
    }

    SPDLOG_INFO("[SceneState]Generated default map for scene {} ({}x{})", scene_id_, width_, height_);
}

void SceneState::set_cave(bool is_cave, int level) {
    is_cave_ = is_cave;
    cave_level_ = level;
}

void SceneState::set_boss_defeated(bool defeated) {
    boss_defeated_ = defeated;
}

std::string SceneState::serialize() const {
    nlohmann::json j;
    j["scene_id"] = scene_id_;
    j["width"] = width_;
    j["height"] = height_;
    j["frozen_at"] = frozen_at_;
    j["player_count"] = player_count_;

    // Serialize world state (ground + objects as flat arrays)
    const auto& ws = world_state_;
    std::vector<uint8_t> ground_flat;
    ground_flat.reserve(width_ * height_);
    std::vector<uint8_t> objects_flat;
    objects_flat.reserve(width_ * height_);

    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            ground_flat.push_back(static_cast<uint8_t>(ws.get_ground(x, y)));
            objects_flat.push_back(static_cast<uint8_t>(ws.get_object(x, y)));
        }
    }
    j["ground"] = ground_flat;
    j["objects"] = objects_flat;

    // Serialize crop system
    j["crops"] = nlohmann::json::parse(crop_system_.serialize());

    return j.dump();
}

bool SceneState::deserialize(const std::string& json_str) {
    if (json_str.empty() || json_str == "{}") {
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(json_str);

        if (!j.contains("width") || !j.contains("height") ||
            !j.contains("ground") || !j.contains("objects")) {
            return false;
        }

        int w = j["width"].get<int>();
        int h = j["height"].get<int>();

        if (w <= 0 || h <= 0 || w > 256 || h > 256) {
            return false;
        }

        auto ground_flat = j["ground"].get<std::vector<uint8_t>>();
        auto objects_flat = j["objects"].get<std::vector<uint8_t>>();

        if (static_cast<int>(ground_flat.size()) != w * h ||
            static_cast<int>(objects_flat.size()) != w * h) {
            return false;
        }

        // Update dimensions if needed
        if (w != width_ || h != height_) {
            width_ = w;
            height_ = h;
            world_state_ = WorldState(w, h);
        }

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                world_state_.set_ground(x, y, static_cast<GroundType>(ground_flat[y * w + x]));
                world_state_.set_object(x, y, static_cast<ObjectType>(objects_flat[y * w + x]));
            }
        }

        // Deserialize crop system
        if (j.contains("crops")) {
            crop_system_.deserialize(j["crops"].dump());
        }

        // Restore freeze state
        if (j.contains("frozen_at")) {
            frozen_at_ = j["frozen_at"].get<time_t>();
        }

        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[SceneState]Failed to deserialize scene {}: {}", scene_id_, e.what());
        return false;
    }
}

}  // namespace farm
