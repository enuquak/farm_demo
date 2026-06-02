#pragma once

#include "scene_state.h"
#include "game_types.h"
#include "cave_spawner.h"

#include <string>
#include <string_view>
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
    static std::string make_scene_data_key(std::string_view scene_id);

    // 矿洞场景
    CaveSpawner* get_or_create_cave_spawner(int cave_level);
    void update_cave_spawners(float dt, uint64_t player_id, float player_x, float player_y);

private:
    PlayerManager* player_mgr_;
    DBMgrConnectionManager* dbmgr_mgr_;
    SendGameMsgFunc send_game_msg_;

    std::unordered_map<std::string, std::unique_ptr<SceneState>> scenes_;
    std::unordered_map<int, std::unique_ptr<CaveSpawner>> cave_spawners_;
};

}  // namespace farm
