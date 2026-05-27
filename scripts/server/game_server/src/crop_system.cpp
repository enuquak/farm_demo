#include "crop_system.h"
#include "world_state.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>

namespace farm {

CropSystem::CropSystem()
    : frozen_at_(0)
{
}

std::string CropSystem::make_key(int x, int y) {
    return std::to_string(x) + "," + std::to_string(y);
}

bool CropSystem::register_crop(int x, int y, WorldState* world,
                                time_t planted_at, int grow_time) {
    if (!world) {
        SPDLOG_ERROR("[CropSystem]register_crop: world is null");
        return false;
    }

    // Validate ground is TILLED
    if (world->get_ground(x, y) != GroundType::TILLED) {
        SPDLOG_INFO("[CropSystem]Cannot plant at ({},{}): ground is not TILLED", x, y);
        return false;
    }

    std::string key = make_key(x, y);
    CropData crop;
    crop.planted_at = (planted_at > 0) ? planted_at : std::time(nullptr);
    crop.grow_time = grow_time;

    growing_tiles_[key] = crop;
    SPDLOG_INFO("[CropSystem]Registered crop at ({},{}), planted_at={}, grow_time={}",
                x, y, crop.planted_at, crop.grow_time);
    return true;
}

void CropSystem::update(WorldState* world) {
    if (!world) return;

    time_t now = std::time(nullptr);

    for (auto it = growing_tiles_.begin(); it != growing_tiles_.end(); ) {
        const std::string& key = it->first;
        CropData& crop = it->second;

        // Parse tile coordinates from key
        size_t comma = key.find(',');
        if (comma == std::string::npos) {
            SPDLOG_WARN("[CropSystem]Invalid key format: {}", key);
            it = growing_tiles_.erase(it);
            continue;
        }

        int tx = std::stoi(key.substr(0, comma));
        int ty = std::stoi(key.substr(comma + 1));

        // Cleanup: if the object is no longer CROP_GROWING, remove stale data
        if (world->get_object(tx, ty) != ObjectType::CROP_GROWING) {
            SPDLOG_INFO("[CropSystem]Cleaning up stale crop data at ({},{})", tx, ty);
            it = growing_tiles_.erase(it);
            continue;
        }

        // Check maturity
        if (now - crop.planted_at >= crop.grow_time) {
            world->set_object(tx, ty, ObjectType::CROP_READY);
            SPDLOG_INFO("[CropSystem]Crop matured at ({},{})", tx, ty);
            it = growing_tiles_.erase(it);
        } else {
            ++it;
        }
    }
}

void CropSystem::simulate_elapsed(time_t elapsed, WorldState* world) {
    if (!world || elapsed <= 0) return;

    time_t effective_now = std::time(nullptr);

    for (auto it = growing_tiles_.begin(); it != growing_tiles_.end(); ) {
        const std::string& key = it->first;
        CropData& crop = it->second;

        size_t comma = key.find(',');
        if (comma == std::string::npos) {
            it = growing_tiles_.erase(it);
            continue;
        }

        int tx = std::stoi(key.substr(0, comma));
        int ty = std::stoi(key.substr(comma + 1));

        // Cleanup stale data
        if (world->get_object(tx, ty) != ObjectType::CROP_GROWING) {
            it = growing_tiles_.erase(it);
            continue;
        }

        // Check if the crop would have matured during the elapsed time
        // The total time since planting = (effective_now - planted_at)
        // But we simulate as if elapsed seconds passed from the freeze point
        // So: effective planted_at is shifted earlier by elapsed
        time_t effective_planted = crop.planted_at - elapsed;
        if (effective_now - effective_planted >= crop.grow_time) {
            world->set_object(tx, ty, ObjectType::CROP_READY);
            SPDLOG_INFO("[CropSystem]Crop matured during catch-up at ({},{})", tx, ty);
            it = growing_tiles_.erase(it);
        } else {
            // Update planted_at to reflect the elapsed time (shift planting earlier)
            crop.planted_at = effective_planted;
            ++it;
        }
    }
}

void CropSystem::remove_crop(int x, int y) {
    std::string key = make_key(x, y);
    auto it = growing_tiles_.find(key);
    if (it != growing_tiles_.end()) {
        growing_tiles_.erase(it);
        SPDLOG_INFO("[CropSystem]Removed crop data at ({},{})", x, y);
    }
}

bool CropSystem::has_crop(int x, int y) const {
    std::string key = make_key(x, y);
    return growing_tiles_.find(key) != growing_tiles_.end();
}

std::string CropSystem::serialize() const {
    nlohmann::json j;
    nlohmann::json tiles = nlohmann::json::object();

    for (const auto& kv : growing_tiles_) {
        nlohmann::json tile_data;
        tile_data["planted_at"] = kv.second.planted_at;
        tile_data["grow_time"] = kv.second.grow_time;
        tiles[kv.first] = tile_data;
    }

    j["tiles"] = tiles;
    return j.dump();
}

bool CropSystem::deserialize(const std::string& json_str) {
    growing_tiles_.clear();

    if (json_str.empty() || json_str == "{}") {
        return true;  // Empty is valid (old save compatibility)
    }

    try {
        nlohmann::json j = nlohmann::json::parse(json_str);

        // Support both "tiles" key (new) and "crops" key (legacy)
        nlohmann::json* tiles_ptr = nullptr;
        if (j.contains("tiles") && j["tiles"].is_object()) {
            tiles_ptr = &j["tiles"];
        } else if (j.contains("crops") && j["crops"].is_object()) {
            tiles_ptr = &j["crops"];
        }

        if (!tiles_ptr) {
            SPDLOG_INFO("[CropSystem]No crop data in save, starting empty");
            return true;
        }

        for (auto& kv : tiles_ptr->items()) {
            const std::string& key = kv.key();
            auto& tile_data = kv.value();

            if (!tile_data.contains("planted_at") || !tile_data.contains("grow_time")) {
                SPDLOG_WARN("[CropSystem]Skipping invalid crop entry: {}", key);
                continue;
            }

            CropData crop;
            crop.planted_at = tile_data["planted_at"].get<time_t>();
            crop.grow_time = tile_data["grow_time"].get<int>();

            growing_tiles_[key] = crop;
        }

        SPDLOG_INFO("[CropSystem]Deserialized {} crop entries", growing_tiles_.size());
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[CropSystem]Failed to deserialize: {}", e.what());
        return false;
    }
}

}  // namespace farm
