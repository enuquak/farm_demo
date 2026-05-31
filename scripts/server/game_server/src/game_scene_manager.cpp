#include "game_scene_manager.h"
#include "player_manager.h"
#include "dbmgr_connection_manager.h"
#include "player.h"
#include "msg_ids.h"
#include "log_macros.h"

#include "player.pb.h"
#include "dbmgr.pb.h"

#include <nlohmann/json.hpp>

namespace farm {

// Reserved player_id for system/scene data persistence via DBMgr
static constexpr uint64_t SCENE_DATA_PLAYER_ID = 0;

GameSceneManager::GameSceneManager(PlayerManager* player_mgr,
                                   DBMgrConnectionManager* dbmgr_mgr,
                                   SendGameMsgFunc send_game_msg)
    : player_mgr_(player_mgr)
    , dbmgr_mgr_(dbmgr_mgr)
    , send_game_msg_(std::move(send_game_msg))
{
}

std::optional<SceneState*> GameSceneManager::get_scene(const std::string& scene_id) const {
    auto it = scenes_.find(scene_id);
    if (it != scenes_.end()) {
        return it->second.get();
    }
    return std::nullopt;
}

SceneState* GameSceneManager::get_or_create_scene(const std::string& scene_id) {
    auto it = scenes_.find(scene_id);
    if (it != scenes_.end()) {
        return it->second.get();
    }

    // Scene dimensions (matching client scene_defs.py)
    int width = 60;
    int height = 50;
    if (scene_id == "house") {
        width = 10;
        height = 8;
    }

    auto scene = std::make_unique<SceneState>(scene_id, width, height);
    scene->generate_default();

    SceneState* ptr = scene.get();
    scenes_[scene_id] = std::move(scene);

    SPDLOG_INFO("[Game]Created scene: {} ({}x{})", scene_id, width, height);

    // Attempt to load saved scene data from DBMgr (async, will overwrite defaults if found)
    load_scene_data(scene_id);

    return ptr;
}

void GameSceneManager::handle_scene_change_req(uint64_t player_id,
                                                const uint8_t* payload, size_t payload_len) {
    // Parse SceneChangeReq
    farm::SceneChangeReq req;
    if (payload_len > 0 && !req.ParseFromArray(payload, static_cast<int>(payload_len))) {
        SPDLOG_ERROR("[Game]Failed to parse SceneChangeReq");
        return;
    }

    const std::string& target_scene = req.target_scene();
    const std::string& target_portal_id = req.target_portal_id();

    SPDLOG_INFO("[Game]SceneChangeReq: player_id={} target_scene={} target_portal={}",
                player_id, target_scene, target_portal_id);

    // Get player
    Player* player = player_mgr_->get_player(player_id).value_or(nullptr);
    if (!player) {
        SPDLOG_ERROR("[Game]SceneChangeReq: player_id={} not found", player_id);
        return;
    }

    const std::string& current_scene_id = player->get_scene_id();

    // Validate target scene exists in our scene definitions
    if (target_scene != "farm" && target_scene != "house") {
        SPDLOG_ERROR("[Game]SceneChangeReq: invalid target_scene={}", target_scene);

        // Send failure response
        farm::SceneChangeResp resp;
        resp.set_code(1);
        resp.set_msg("invalid target scene");
        resp.set_target_scene(target_scene);

        std::string resp_data;
        resp.SerializeToString(&resp_data);
        send_game_msg_(player_id, MSG_ID_SCENE_CHANGE_RESP,
                       reinterpret_cast<const uint8_t*>(resp_data.data()),
                       resp_data.size());
        return;
    }

    // Freeze current scene (if player was in one)
    if (!current_scene_id.empty()) {
        auto it = scenes_.find(current_scene_id);
        if (it != scenes_.end()) {
            it->second->remove_player();
        }
    }

    // Get or create target scene
    SceneState* target = get_or_create_scene(target_scene);

    // Thaw if frozen
    if (target->is_frozen()) {
        target->thaw();
    }
    target->add_player();

    // Determine spawn position
    int spawn_x = 5;
    int spawn_y = 5;
    if (target_scene == "farm") {
        // Farm spawn near the door
        spawn_x = 30;
        spawn_y = 25;
    } else if (target_scene == "house") {
        // House spawn inside
        spawn_x = 5;
        spawn_y = 6;
    }

    // Update player data
    player->set_scene_id(target_scene);
    player->set_pos_x(static_cast<float>(spawn_x * 16));  // TILE_SIZE = 16
    player->set_pos_y(static_cast<float>(spawn_y * 16));
    player->set_dirty(true);

    // Send response
    farm::SceneChangeResp resp;
    resp.set_code(0);
    resp.set_msg("success");
    resp.set_target_scene(target_scene);
    resp.set_spawn_x(spawn_x);
    resp.set_spawn_y(spawn_y);
    resp.set_active_scene(target_scene);

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_game_msg_(player_id, MSG_ID_SCENE_CHANGE_RESP,
                   reinterpret_cast<const uint8_t*>(resp_data.data()),
                   resp_data.size());

    SPDLOG_INFO("[Game]SceneChangeReq: player_id={} switched to scene={} spawn=({},{})",
                player_id, target_scene, spawn_x, spawn_y);
}

// ===========================================
// Scene data persistence (via DBMgr)
// ===========================================

std::string GameSceneManager::make_scene_data_key(std::string_view scene_id) {
    return std::string("scene:").append(scene_id);
}

void GameSceneManager::save_all_scenes() {
    SPDLOG_INFO("[Game]Saving all scene data...");
    for (auto& [scene_id, scene] : scenes_) {
        save_scene_data(scene_id);
    }
    SPDLOG_INFO("[Game]All scene data save requests sent");
}

void GameSceneManager::save_scene_data(const std::string& scene_id) {
    if (!dbmgr_mgr_->is_connected(0)) {
        SPDLOG_INFO("[Game]No DBMgr connected, skipping scene save for {}", scene_id);
        return;
    }

    auto it = scenes_.find(scene_id);
    if (it == scenes_.end()) {
        return;
    }

    // Build multi-scene save format: {"active_scene": "farm", "scenes": {"farm": {...}, "house": {...}}}
    // For individual scene save, wrap in the expected format
    nlohmann::json save_data;
    save_data["active_scene"] = scene_id;
    save_data["scenes"][scene_id] = nlohmann::json::parse(it->second->serialize());

    std::string value = save_data.dump();
    std::string key = make_scene_data_key(scene_id);

    auto callback = [scene_id](int32_t code, const uint8_t* /*data*/, size_t /*len*/) {
        if (code == 0) {
            SPDLOG_INFO("[Game]Scene data saved: scene={}", scene_id);
        } else {
            SPDLOG_ERROR("[Game]Scene data save failed: scene={} code={}", scene_id, code);
        }
    };

    dbmgr_mgr_->send_player_data_req(
        SCENE_DATA_PLAYER_ID,
        static_cast<int32_t>(farm::PlayerDataOp::SET),
        key, value,
        std::move(callback));
}

void GameSceneManager::load_scene_data(const std::string& scene_id) {
    if (!dbmgr_mgr_->is_connected(0)) {
        SPDLOG_INFO("[Game]No DBMgr connected, cannot load scene data for {}", scene_id);
        return;
    }

    std::string key = make_scene_data_key(scene_id);

    auto callback = [this, scene_id](int32_t code, const uint8_t* value_data, size_t value_len) {
        if (code != 0 || !value_data || value_len == 0) {
            SPDLOG_INFO("[Game]No saved data for scene {}, using default", scene_id);
            return;
        }

        auto it = scenes_.find(scene_id);
        if (it == scenes_.end()) {
            return;
        }

        try {
            std::string json_str(reinterpret_cast<const char*>(value_data), value_len);
            nlohmann::json save_data = nlohmann::json::parse(json_str);

            // BUG-003 fix: handle old single-scene format migration
            // Old format: {"width": ..., "height": ..., "ground": [...], "objects": [...]}
            // New format: {"active_scene": "farm", "scenes": {"farm": {...}}}
            std::string scene_json;
            if (save_data.contains("scenes") && save_data["scenes"].is_object()) {
                // New multi-scene format
                if (save_data["scenes"].contains(scene_id)) {
                    scene_json = save_data["scenes"][scene_id].dump();
                }
            } else if (save_data.contains("ground") && save_data.contains("objects")) {
                // Old single-scene format - migrate to current scene
                SPDLOG_INFO("[Game]Migrating old save format for scene {}", scene_id);
                scene_json = save_data.dump();
            }

            if (!scene_json.empty()) {
                if (it->second->deserialize(scene_json)) {
                    SPDLOG_INFO("[Game]Loaded saved data for scene {}", scene_id);
                } else {
                    SPDLOG_ERROR("[Game]Failed to deserialize scene data for {}, using default", scene_id);
                    it->second->generate_default();
                }
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Game]Error loading scene data for {}: {}", scene_id, e.what());
        }
    };

    dbmgr_mgr_->send_player_data_req(
        SCENE_DATA_PLAYER_ID,
        static_cast<int32_t>(farm::PlayerDataOp::GET),
        key, "",
        std::move(callback));
}

}  // namespace farm
