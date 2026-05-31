#pragma once

#include "game_types.h"
#include "admin_msg_ids.h"

#include <memory>
#include <vector>
#include <cstdint>
#include <functional>

namespace farm {

class GateSession;
class PlayerManager;
class GameSceneManager;
class GameClock;
class DBMgrConnectionManager;

class AdminHandler {
public:
    AdminHandler(PlayerManager* player_mgr,
                 GameSceneManager* scene_mgr,
                 GameClock* game_clock,
                 DBMgrConnectionManager* dbmgr_mgr,
                 SendToGateFunc send_to_gate,
                 std::function<void()> disable_listener,
                 std::function<void()> stop_server);
    ~AdminHandler() = default;

    // Handle admin messages (top-level entry point)
    void handle(std::shared_ptr<GateSession> session,
                uint32_t msg_id, const std::vector<uint8_t>& payload);

private:
    void handle_shutdown(std::shared_ptr<GateSession> session, const AdminShutdownMsg& msg);
    void handle_shutdown_resp(std::shared_ptr<GateSession> session, const AdminShutdownResp& resp);

    PlayerManager* player_mgr_;
    GameSceneManager* scene_mgr_;
    GameClock* game_clock_;
    DBMgrConnectionManager* dbmgr_mgr_;
    SendToGateFunc send_to_gate_;
    std::function<void()> disable_listener_;
    std::function<void()> stop_server_;
};

}  // namespace farm
