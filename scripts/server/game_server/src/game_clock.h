#pragma once

#include "game_types.h"

#include <cstdint>
#include <string>

namespace farm {

class PlayerManager;
class GameSceneManager;
class DBMgrConnectionManager;
class Player;

class GameClock {
public:
    GameClock(PlayerManager* player_mgr,
              GameSceneManager* scene_mgr,
              DBMgrConnectionManager* dbmgr_mgr,
              SendGameMsgFunc send_game_msg);
    ~GameClock() = default;

    // Update clock state (called every second)
    void update();

    // Broadcast clock sync to all players
    void broadcast_clock_sync();

    // Save/load clock data via DBMgr
    void save_clock_data();
    void load_clock_data();

    // Handle force sleep ready message from client
    void handle_force_sleep_ready(uint64_t player_id,
                                   const uint8_t* payload, size_t payload_len);

    // Complete force sleep for a player
    void complete_force_sleep(Player* player);

    // Getters
    int32_t day() const { return clock_day_; }
    int32_t time_slot() const { return clock_time_slot_; }
    bool paused() const { return clock_paused_; }

private:
    void on_day_end();

    PlayerManager* player_mgr_;
    GameSceneManager* scene_mgr_;
    DBMgrConnectionManager* dbmgr_mgr_;
    SendGameMsgFunc send_game_msg_;

    int32_t clock_day_ = 1;
    int32_t clock_time_slot_ = 0;
    double clock_elapsed_ = 0.0;
    bool clock_paused_ = false;
    bool force_sleep_pending_ = false;
    int force_sleep_timeout_counter_ = 0;  // Timeout counter in seconds
};

}  // namespace farm
