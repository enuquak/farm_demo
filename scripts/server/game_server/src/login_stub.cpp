#include "login_stub.h"
#include "game_clock.h"
#include "player_manager.h"
#include "dbmgr_connection_manager.h"
#include "player.h"
#include "redis_connection.h"
#include "internal_msg_ids.h"
#include "msg_ids.h"
#include "game_constants.h"
#include "log_macros.h"

#include "internal.pb.h"
#include "account.pb.h"
#include "player.pb.h"
#include "dbmgr.pb.h"

namespace farm {

LoginStub::LoginStub(PlayerManager* player_mgr,
                     DBMgrConnectionManager* dbmgr_mgr,
                     RedisConnection* redis_conn,
                     uint32_t server_id,
                     PlayerIdPool* id_pool,
                     SendToGateFunc send_to_gate)
    : player_mgr_(player_mgr)
    , dbmgr_mgr_(dbmgr_mgr)
    , redis_conn_(redis_conn)
    , server_id_(server_id)
    , id_pool_(id_pool)
    , send_to_gate_(std::move(send_to_gate))
{
}

void LoginStub::handle_account_msg(std::shared_ptr<GateSession> session,
                                   const std::vector<uint8_t>& payload) {
    farm::AccountMessage req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[LoginStub]Failed to parse AccountMessage");
        return;
    }

    const std::string& account_id = req.account_id();
    uint32_t msg_id = req.msg_id();
    const std::string& inner_payload = req.payload();

    SPDLOG_INFO("[LoginStub]Account message: account_id={} msg_id={}", account_id, msg_id);

    // Route based on msg_id
    if (msg_id == MSG_ID_QUERY_ROLES_REQ) {
        handle_query_roles(session, account_id);
    } else if (msg_id == MSG_ID_CREATE_ROLE_REQ) {
        handle_create_role(session, account_id, inner_payload);
    } else if (msg_id == MSG_ID_ACCOUNT_DATA_REQ) {
        handle_account_data(session, account_id);
    } else if (msg_id == MSG_ID_ACCOUNT_SET_REQ) {
        handle_account_set(session, account_id, inner_payload);
    } else if (msg_id == MSG_ID_ENTER_GAME_REQ) {
        handle_enter_game(session, account_id, inner_payload);
    } else {
        SPDLOG_INFO("[LoginStub]Unknown account msg_id={}", msg_id);
    }
}

void LoginStub::handle_query_roles(std::shared_ptr<GateSession> session,
                                   const std::string& account_id) {
    dbmgr_mgr_->send_account_data_req(account_id,
        [this, session, account_id](int32_t code, const std::vector<std::tuple<uint32_t, uint64_t, std::string>>& roles) {
            // Build response
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_QUERY_ROLES_RESP);

            // Build QueryRolesResp
            farm::QueryRolesResp roles_resp;
            roles_resp.set_code(code);
            if (code == 0) {
                for (const auto& role : roles) {
                    auto* r = roles_resp.add_roles();
                    r->set_server_id(std::get<0>(role));
                    r->set_player_id(std::get<1>(role));
                    r->set_role_name(std::get<2>(role));
                }
            } else {
                roles_resp.set_msg("Failed to query roles");
            }

            std::string resp_data;
            roles_resp.SerializeToString(&resp_data);
            resp.set_payload(resp_data);

            std::string final_resp;
            resp.SerializeToString(&final_resp);

            send_to_gate_(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
        });
}

void LoginStub::handle_create_role(std::shared_ptr<GateSession> session,
                                   const std::string& account_id,
                                   const std::string& inner_payload) {
    farm::CreateRoleReq create_req;
    if (!inner_payload.empty() && !create_req.ParseFromArray(inner_payload.data(), static_cast<int>(inner_payload.size()))) {
        SPDLOG_ERROR("[LoginStub]Failed to parse CreateRoleReq");
        return;
    }

    uint32_t server_id = create_req.server_id();
    const std::string& role_name = create_req.role_name();

    // Acquire player_id from global pool
    uint64_t player_id = id_pool_->acquire();

    dbmgr_mgr_->send_account_set_req(account_id,
        server_id, player_id, role_name,
        [this, session, account_id, player_id](int32_t code, const std::string& msg) {
            // Build response
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_CREATE_ROLE_RESP);

            // Build CreateRoleResp
            farm::CreateRoleResp create_resp;
            create_resp.set_code(code);
            create_resp.set_msg(msg);
            if (code == 0) {
                create_resp.set_player_id(player_id);
            }

            std::string resp_data;
            create_resp.SerializeToString(&resp_data);
            resp.set_payload(resp_data);

            std::string final_resp;
            resp.SerializeToString(&final_resp);

            send_to_gate_(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
        });
}

void LoginStub::handle_account_data(std::shared_ptr<GateSession> session,
                                    const std::string& account_id) {
    dbmgr_mgr_->send_account_data_req(account_id,
        [this, session, account_id](int32_t code, const std::vector<std::tuple<uint32_t, uint64_t, std::string>>& roles) {
            // Build response
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_ACCOUNT_DATA_RESP);

            // Build AccountDataResp
            farm::AccountDataResp data_resp;
            data_resp.set_code(code);
            if (code == 0) {
                for (const auto& role : roles) {
                    auto* r = data_resp.add_roles();
                    r->set_server_id(std::get<0>(role));
                    r->set_player_id(std::get<1>(role));
                    r->set_role_name(std::get<2>(role));
                }
            } else {
                data_resp.set_msg("Failed to get account data");
            }

            std::string resp_data;
            data_resp.SerializeToString(&resp_data);
            resp.set_payload(resp_data);

            std::string final_resp;
            resp.SerializeToString(&final_resp);

            send_to_gate_(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
        });
}

void LoginStub::handle_account_set(std::shared_ptr<GateSession> session,
                                   const std::string& account_id,
                                   const std::string& inner_payload) {
    farm::AccountSetReq set_req;
    if (!inner_payload.empty() && !set_req.ParseFromArray(inner_payload.data(), static_cast<int>(inner_payload.size()))) {
        SPDLOG_ERROR("[LoginStub]Failed to parse AccountSetReq");
        return;
    }

    const auto& new_role = set_req.new_role();
    dbmgr_mgr_->send_account_set_req(account_id,
        new_role.server_id(), new_role.player_id(), new_role.role_name(),
        [this, session, account_id](int32_t code, const std::string& msg) {
            // Build response
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_ACCOUNT_SET_RESP);

            // Build AccountSetResp
            farm::AccountSetResp set_resp;
            set_resp.set_code(code);
            set_resp.set_msg(msg);

            std::string resp_data;
            set_resp.SerializeToString(&resp_data);
            resp.set_payload(resp_data);

            std::string final_resp;
            resp.SerializeToString(&final_resp);

            send_to_gate_(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
        });
}

void LoginStub::handle_enter_game(std::shared_ptr<GateSession> session,
                                  const std::string& account_id,
                                  const std::string& inner_payload) {
    farm::EnterGameReq enter_req;
    if (!inner_payload.empty() && !enter_req.ParseFromArray(inner_payload.data(), static_cast<int>(inner_payload.size()))) {
        SPDLOG_ERROR("[LoginStub]Failed to parse EnterGameReq from account_msg");
        return;
    }

    uint64_t req_player_id = enter_req.player_id();
    uint32_t server_id = enter_req.server_id();

    SPDLOG_INFO("[LoginStub]EnterGameReq (account): player_id={} server_id={}", req_player_id, server_id);

    // Check if DBMgr is available
    if (!dbmgr_mgr_->is_connected(0)) {
        SPDLOG_ERROR("[LoginStub]DBMgr not available for EnterGameReq (account), player_id={}", req_player_id);

        // Send failure response
        farm::AccountMessageResp resp;
        resp.set_account_id(account_id);
        resp.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp enter_resp;
        enter_resp.set_code(-1);
        enter_resp.set_msg("DBMgr not available");

        std::string resp_data;
        enter_resp.SerializeToString(&resp_data);
        resp.set_payload(resp_data);

        std::string final_resp;
        resp.SerializeToString(&final_resp);

        send_to_gate_(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
        return;
    }

    // Use PlayerManager to create player and load data
    auto callback = [this, session, account_id, req_player_id](uint64_t pid, bool success, const std::string& msg) {
        // Build response
        farm::AccountMessageResp resp;
        resp.set_account_id(account_id);
        resp.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp enter_resp;
        if (success) {
            // Mark player online in Redis
            mark_player_online(pid);

            // Get player data
            Player* player = player_mgr_->get_player(pid).value_or(nullptr);
            if (player) {
                const PlayerBizData& data = player->player_data();
                farm::PlayerData player_data;
                player_data.set_player_id(pid);
                player_data.set_role_name(data.role_name);
                player_data.set_level(data.level);
                player_data.set_exp(data.experience);
                player_data.set_pos_x(data.pos_x);
                player_data.set_pos_y(data.pos_y);
                player_data.set_pos_z(data.pos_z);
                player_data.set_scene_id(data.scene_id);
                *enter_resp.mutable_player_data() = player_data;

                // Include energy data
                farm::EnergySync* energy_sync = enter_resp.mutable_energy();
                energy_sync->set_current(data.energy);
                energy_sync->set_max(farm::MAX_ENERGY);

                enter_resp.set_code(0);
                enter_resp.set_msg("success");

                // Include clock data
                if (clock_) {
                    farm::ClockSync* clock_sync = enter_resp.mutable_clock();
                    clock_sync->set_day(clock_->day());
                    clock_sync->set_time_slot(clock_->time_slot());
                    clock_sync->set_paused(clock_->paused());
                }
            } else {
                enter_resp.set_code(-1);
                enter_resp.set_msg("Player not found after data load");
            }
        } else {
            enter_resp.set_code(-1);
            enter_resp.set_msg(msg);
        }

        std::string resp_data;
        enter_resp.SerializeToString(&resp_data);
        resp.set_payload(resp_data);

        std::string final_resp;
        resp.SerializeToString(&final_resp);

        send_to_gate_(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
    };

    bool added = player_mgr_->add_player_with_data_load(req_player_id, session.get(), std::move(callback));
    if (!added) {
        // Player already exists, send failure response
        farm::AccountMessageResp resp;
        resp.set_account_id(account_id);
        resp.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp enter_resp;
        enter_resp.set_code(1);
        enter_resp.set_msg("Player already in game");

        std::string resp_data;
        enter_resp.SerializeToString(&resp_data);
        resp.set_payload(resp_data);

        std::string final_resp;
        resp.SerializeToString(&final_resp);

        send_to_gate_(session, MSG_ID_ACCOUNT_MSG_RESP, final_resp);
    }
}

void LoginStub::handle_enter_game_req(std::shared_ptr<GateSession> session,
                                      uint64_t player_id, const std::string& payload) {
    farm::EnterGameReq req;
    if (!payload.empty() && !req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[LoginStub]Failed to parse EnterGameReq");
        return;
    }

    uint64_t req_player_id = req.player_id();
    uint32_t server_id = req.server_id();

    SPDLOG_INFO("[LoginStub]EnterGameReq: player_id={} server_id={}", req_player_id, server_id);

    // Check if DBMgr is available
    if (!dbmgr_mgr_->is_connected(0)) {
        SPDLOG_ERROR("[LoginStub]DBMgr not available for EnterGameReq, player_id={}", req_player_id);

        // Send failure response
        farm::GameMessage game_msg;
        game_msg.set_player_id(req_player_id);
        game_msg.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp resp;
        resp.set_code(-1);
        resp.set_msg("DBMgr not available");

        std::string resp_data;
        resp.SerializeToString(&resp_data);
        game_msg.set_payload(resp_data);

        std::string final_resp;
        game_msg.SerializeToString(&final_resp);

        send_to_gate_(session, MSG_ID_GAME_MSG, final_resp);
        return;
    }

    // Use PlayerManager to create player and load data
    auto callback = [this, session, req_player_id](uint64_t pid, bool success, const std::string& msg) {
        // Build response
        farm::GameMessage game_msg;
        game_msg.set_player_id(req_player_id);
        game_msg.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp resp;
        if (success) {
            // Mark player online in Redis
            mark_player_online(pid);

            // Get player data
            Player* player = player_mgr_->get_player(pid).value_or(nullptr);
            if (player) {
                const PlayerBizData& data = player->player_data();
                farm::PlayerData player_data;
                player_data.set_player_id(pid);
                player_data.set_role_name(data.role_name);
                player_data.set_level(data.level);
                player_data.set_exp(data.experience);
                player_data.set_pos_x(data.pos_x);
                player_data.set_pos_y(data.pos_y);
                player_data.set_pos_z(data.pos_z);
                player_data.set_scene_id(data.scene_id);
                *resp.mutable_player_data() = player_data;

                // Include energy data
                farm::EnergySync* energy_sync = resp.mutable_energy();
                energy_sync->set_current(data.energy);
                energy_sync->set_max(farm::MAX_ENERGY);

                resp.set_code(0);
                resp.set_msg("success");

                // Include clock data
                if (clock_) {
                    farm::ClockSync* clock_sync = resp.mutable_clock();
                    clock_sync->set_day(clock_->day());
                    clock_sync->set_time_slot(clock_->time_slot());
                    clock_sync->set_paused(clock_->paused());
                }
            } else {
                resp.set_code(-1);
                resp.set_msg("Player not found after data load");
            }
        } else {
            resp.set_code(-1);
            resp.set_msg(msg);
        }

        std::string resp_data;
        resp.SerializeToString(&resp_data);
        game_msg.set_payload(resp_data);

        std::string final_resp;
        game_msg.SerializeToString(&final_resp);

        send_to_gate_(session, MSG_ID_GAME_MSG, final_resp);
    };

    bool added = player_mgr_->add_player_with_data_load(req_player_id, session.get(), std::move(callback));
    if (!added) {
        // Player already exists, send failure response
        farm::GameMessage game_msg;
        game_msg.set_player_id(req_player_id);
        game_msg.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp resp;
        resp.set_code(1);
        resp.set_msg("Player already in game");

        std::string resp_data;
        resp.SerializeToString(&resp_data);
        game_msg.set_payload(resp_data);

        std::string final_resp;
        game_msg.SerializeToString(&final_resp);

        send_to_gate_(session, MSG_ID_GAME_MSG, final_resp);
    }
}

void LoginStub::on_player_offline(uint64_t player_id) {
    if (redis_conn_ && redis_conn_->is_connected()) {
        std::string key = "player:online:" + std::to_string(player_id);
        redis_conn_->del(key);
        SPDLOG_INFO("[LoginStub]Player offline, removed Redis key: {}", key);
    }
}

void LoginStub::mark_player_online(uint64_t player_id) {
    if (redis_conn_ && redis_conn_->is_connected()) {
        std::string key = "player:online:" + std::to_string(player_id);
        std::string value = std::to_string(server_id_);
        if (redis_conn_->set(key, value)) {
            SPDLOG_INFO("[LoginStub]Marked player online: player_id={} server_id={}", player_id, server_id_);
        } else {
            SPDLOG_ERROR("[LoginStub]Failed to mark player online: player_id={}", player_id);
        }
    } else {
        SPDLOG_WARN("[LoginStub]Redis not connected, skipping mark_player_online for player_id={}", player_id);
    }
}

}  // namespace farm
