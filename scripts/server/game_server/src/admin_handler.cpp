#include "admin_handler.h"
#include "game_scene_manager.h"
#include "game_clock.h"
#include "player_manager.h"
#include "dbmgr_connection_manager.h"
#include "log_macros.h"

namespace farm {

AdminHandler::AdminHandler(PlayerManager* player_mgr,
                           GameSceneManager* scene_mgr,
                           GameClock* game_clock,
                           DBMgrConnectionManager* dbmgr_mgr,
                           SendToGateFunc send_to_gate,
                           std::function<void()> disable_listener,
                           std::function<void()> stop_server)
    : player_mgr_(player_mgr)
    , scene_mgr_(scene_mgr)
    , game_clock_(game_clock)
    , dbmgr_mgr_(dbmgr_mgr)
    , send_to_gate_(std::move(send_to_gate))
    , disable_listener_(std::move(disable_listener))
    , stop_server_(std::move(stop_server))
{
}

void AdminHandler::handle(std::shared_ptr<GateSession> session,
                          uint32_t msg_id, const std::vector<uint8_t>& payload) {
    std::string payload_str(payload.begin(), payload.end());

    switch (msg_id) {
        case MSG_ID_SHUTDOWN: {
            AdminShutdownMsg msg;
            if (AdminShutdownMsg::deserialize(payload_str, msg)) {
                handle_shutdown(session, msg);
            } else {
                SPDLOG_ERROR("[Game]Failed to parse MSG_ID_SHUTDOWN");
            }
            break;
        }
        case MSG_ID_SHUTDOWN_RESP: {
            AdminShutdownResp resp;
            if (AdminShutdownResp::deserialize(payload_str, resp)) {
                handle_shutdown_resp(session, resp);
            } else {
                SPDLOG_ERROR("[Game]Failed to parse MSG_ID_SHUTDOWN_RESP");
            }
            break;
        }
        default:
            SPDLOG_INFO("[Game]Unknown admin msg_id={}", msg_id);
            break;
    }
}

void AdminHandler::handle_shutdown(std::shared_ptr<GateSession> session, const AdminShutdownMsg& msg) {
    SPDLOG_INFO("[Game]Received shutdown request: reason={}, timeout_ms={}", msg.reason, msg.timeout_ms);

    // Stop accepting new connections
    if (disable_listener_) {
        disable_listener_();
        SPDLOG_INFO("[Game]Stopped accepting new connections");
    }

    // Save all player data
    SPDLOG_INFO("[Game]Saving all player data...");
    player_mgr_->save_all_players();

    // Save all scene data
    SPDLOG_INFO("[Game]Saving all scene data...");
    scene_mgr_->save_all_scenes();

    // Save clock data
    SPDLOG_INFO("[Game]Saving clock data...");
    game_clock_->save_clock_data();

    // Forward MSG_ID_SHUTDOWN to all DBMgrs
    AdminShutdownMsg forward_msg;
    forward_msg.reason = msg.reason;
    forward_msg.timeout_ms = msg.timeout_ms;
    std::string forward_payload = forward_msg.serialize();

    dbmgr_mgr_->broadcast_message(MSG_ID_SHUTDOWN, forward_payload);
    SPDLOG_INFO("[Game]Forwarded shutdown to all DBMgrs");

    // Send response to Gate
    AdminShutdownResp resp;
    resp.code = 0;
    resp.msg = "GameServer shutting down";
    std::string resp_payload = resp.serialize();

    send_to_gate_(session, MSG_ID_SHUTDOWN_RESP, resp_payload);
    SPDLOG_INFO("[Game]Sent shutdown response to Gate");

    // Wait for DBMgr response (simplified: exit directly)
    SPDLOG_INFO("[Game]Shutdown initiated, stopping server...");
    if (stop_server_) {
        stop_server_();
    }
}

void AdminHandler::handle_shutdown_resp(std::shared_ptr<GateSession> session, const AdminShutdownResp& resp) {
    SPDLOG_INFO("[Game]Received shutdown response: code={}, msg={}", resp.code, resp.msg);

    // If DBMgr responded ready, safe to exit
    if (resp.code == 0) {
        SPDLOG_INFO("[Game]DBMgr ready to shutdown");
    }
}

}  // namespace farm
