#include "world_state.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

namespace farm {

WorldState::WorldState(int width, int height)
    : width_(width)
    , height_(height)
    , ground_(height, std::vector<uint8_t>(width, 0))
    , objects_(height, std::vector<uint8_t>(width, 0))
{
}

GroundType WorldState::get_ground(int x, int y) const {
    if (!is_valid(x, y)) return GroundType::GRASS;
    return static_cast<GroundType>(ground_[y][x]);
}

void WorldState::set_ground(int x, int y, GroundType type) {
    if (!is_valid(x, y)) return;
    ground_[y][x] = static_cast<uint8_t>(type);
}

ObjectType WorldState::get_object(int x, int y) const {
    if (!is_valid(x, y)) return ObjectType::NONE;
    return static_cast<ObjectType>(objects_[y][x]);
}

void WorldState::set_object(int x, int y, ObjectType type) {
    if (!is_valid(x, y)) return;
    objects_[y][x] = static_cast<uint8_t>(type);
}

bool WorldState::is_valid(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

std::string WorldState::serialize() const {
    nlohmann::json j;
    j["width"] = width_;
    j["height"] = height_;

    // Serialize ground as flat array
    std::vector<uint8_t> ground_flat;
    ground_flat.reserve(width_ * height_);
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            ground_flat.push_back(ground_[y][x]);
        }
    }
    j["ground"] = ground_flat;

    // Serialize objects as flat array
    std::vector<uint8_t> objects_flat;
    objects_flat.reserve(width_ * height_);
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            objects_flat.push_back(objects_[y][x]);
        }
    }
    j["objects"] = objects_flat;

    return j.dump();
}

bool WorldState::deserialize(const std::string& json_str) {
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

        width_ = w;
        height_ = h;
        ground_.resize(h, std::vector<uint8_t>(w, 0));
        objects_.resize(h, std::vector<uint8_t>(w, 0));

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                ground_[y][x] = ground_flat[y * w + x];
                objects_[y][x] = objects_flat[y * w + x];
            }
        }

        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[WorldState]Failed to deserialize: {}", e.what());
        return false;
    }
}

void WorldState::generate_default() {
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
            ground_[y][x] = ground_id;
        }
    }

    // Stones (first one near player spawn for testing)
    int stone_positions[][2] = {
        {1,0}, {5,5}, {10,8}, {15,12}, {20,3}, {25,15},
        {30,10}, {35,20}, {40,5}, {45,18}, {50,8}
    };
    for (auto& pos : stone_positions) {
        int sx = pos[0], sy = pos[1];
        if (is_valid(sx, sy)) {
            objects_[sy][sx] = static_cast<uint8_t>(ObjectType::STONE);
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
        if (is_valid(tx, ty)) {
            objects_[ty][tx] = static_cast<uint8_t>(ObjectType::TREE);
        }
    }

    // Doors
    int door_positions[][2] = {{8, 8}, {52, 42}};
    for (auto& pos : door_positions) {
        int dx = pos[0], dy = pos[1];
        if (is_valid(dx, dy)) {
            objects_[dy][dx] = static_cast<uint8_t>(ObjectType::DOOR_IN);
        }
    }

    SPDLOG_INFO("[WorldState]Generated default map {}x{}", width_, height_);
}

}  // namespace farm
