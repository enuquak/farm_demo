#pragma once

#include "scene_state.h"
#include "game_types.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <cstdint>

namespace farm {

class PlayerManager;
class DBMgrConnectionManager;

class GameSceneManager {
public:
    GameSceneManager(PlayerManager* player_mgr,
                     DBMgrConnectionManager* dbmgr_mgr,
                     SendGameMsgFunc send_game_msg);
    ~GameSceneManager() = default;

    // Scene management
    SceneState* get_or_create_scene(const std::string& scene_id);
    std::optional<SceneState*> get_scene(const std::string& scene_id) const;
    void handle_scene_change_req(uint64_t player_id,
                                  const uint8_t* payload, size_t payload_len);

    // Scene data persistence
    void save_all_scenes();
    void save_scene_data(const std::string& scene_id);
    void load_scene_data(const std::string& scene_id);
    static std::string make_scene_data_key(const std::string& scene_id);

private:
    PlayerManager* player_mgr_;
    DBMgrConnectionManager* dbmgr_mgr_;
    SendGameMsgFunc send_game_msg_;

    std::unordered_map<std::string, std::unique_ptr<SceneState>> scenes_;
};

}  // namespace farm
