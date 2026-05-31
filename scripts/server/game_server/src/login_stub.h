#pragma once

#include "game_types.h"
#include "player_id_generator.h"

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace farm {

class GateSession;
class PlayerManager;
class DBMgrConnectionManager;
class RedisConnection;
class GameClock;

class LoginStub {
public:
    LoginStub(PlayerManager* player_mgr,
              DBMgrConnectionManager* dbmgr_mgr,
              RedisConnection* redis_conn,
              uint32_t server_id,
              SendToGateFunc send_to_gate);
    ~LoginStub() = default;

    // Set clock reference (for enter_game response with clock data)
    void set_clock(GameClock* clock) { clock_ = clock; }

    // Handle account messages (top-level entry point, replaces AccountMessageHandler::handle)
    void handle_account_msg(std::shared_ptr<GateSession> session,
                            const std::vector<uint8_t>& payload);

    // Handle EnterGameReq from ClientMessage path
    void handle_enter_game_req(std::shared_ptr<GateSession> session,
                               uint64_t player_id, const std::string& payload);

    // Called when a player goes offline (cleans up Redis)
    void on_player_offline(uint64_t player_id);

private:
    // Account message sub-handlers
    void handle_query_roles(std::shared_ptr<GateSession> session,
                            const std::string& account_id);
    void handle_create_role(std::shared_ptr<GateSession> session,
                            const std::string& account_id,
                            const std::string& inner_payload);
    void handle_account_data(std::shared_ptr<GateSession> session,
                             const std::string& account_id);
    void handle_account_set(std::shared_ptr<GateSession> session,
                            const std::string& account_id,
                            const std::string& inner_payload);
    void handle_enter_game(std::shared_ptr<GateSession> session,
                           const std::string& account_id,
                           const std::string& inner_payload);

    // Redis helper: mark player online
    void mark_player_online(uint64_t player_id);

    PlayerManager* player_mgr_;
    DBMgrConnectionManager* dbmgr_mgr_;
    RedisConnection* redis_conn_;
    uint32_t server_id_;
    PlayerIdGenerator player_id_gen_;
    SendToGateFunc send_to_gate_;
    GameClock* clock_ = nullptr;
};

}  // namespace farm
