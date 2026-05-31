# LoginStub + OnlineStub Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement LoginStub and OnlineStub as Game Server internal components, replacing existing login flow and adding Redis-based online status tracking.

**Architecture:** LoginStub replaces AccountMessageHandler and handles all login-related messages (EnterGameReq, account queries). RedisConnection provides a thin hiredis wrapper for KV operations. OnlineStub exposes online status queries for future cross-server features.

**Tech Stack:** C++17, hiredis, libevent, protobuf

---

## File Structure

### New Files
| File | Responsibility |
|------|----------------|
| `scripts/server/game_server/src/redis_connection.h` | RedisConnection class declaration |
| `scripts/server/game_server/src/redis_connection.cpp` | RedisConnection implementation (hiredis wrapper) |
| `scripts/server/game_server/src/login_stub.h` | LoginStub class declaration |
| `scripts/server/game_server/src/login_stub.cpp` | LoginStub implementation (login flow + Redis write) |
| `scripts/server/game_server/src/online_stub.h` | OnlineStub class declaration |
| `scripts/server/game_server/src/online_stub.cpp` | OnlineStub implementation (online status query) |

### Modified Files
| File | Changes |
|------|---------|
| `scripts/server/game_server/src/game_server.h` | Add RedisConnection, LoginStub, OnlineStub members |
| `scripts/server/game_server/src/game_server.cpp` | Integrate stubs, delegate login handling |
| `scripts/server/game_server/src/player_manager.h` | Add offline callback support |
| `scripts/server/game_server/src/player_manager.cpp` | Notify on player offline |
| `scripts/server/game_server/src/main.cpp` | Load Redis config, pass to GameServer |
| `scripts/server/game_server/CMakeLists.txt` | Add new source files, link hiredis |
| `config/game_server.json` | Add Redis config block |

### Deleted Files
| File | Reason |
|------|--------|
| `scripts/server/game_server/src/account_message_handler.h` | Replaced by LoginStub |
| `scripts/server/game_server/src/account_message_handler.cpp` | Replaced by LoginStub |

---

### Task 1: Create RedisConnection

**Files:**
- Create: `scripts/server/game_server/src/redis_connection.h`
- Create: `scripts/server/game_server/src/redis_connection.cpp`

- [ ] **Step 1: Create redis_connection.h**

```cpp
#pragma once

#include <string>
#include <hiredis/hiredis.h>

namespace farm {

class RedisConnection {
public:
    RedisConnection();
    ~RedisConnection();

    // Non-copyable
    RedisConnection(const RedisConnection&) = delete;
    RedisConnection& operator=(const RedisConnection&) = delete;

    // Connection management
    bool connect(const std::string& uri);
    void disconnect();
    bool is_connected() const;

    // KV operations (synchronous, safe for single-threaded event loop)
    bool set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);  // empty string = not found or error
    bool del(const std::string& key);

private:
    // Parse redis://host:port from URI
    bool parse_uri(const std::string& uri, std::string& host, int& port);

    redisContext* context_ = nullptr;
    bool connected_ = false;
};

}  // namespace farm
```

- [ ] **Step 2: Create redis_connection.cpp**

```cpp
#include "redis_connection.h"
#include "log_macros.h"

#include <cstring>

namespace farm {

RedisConnection::RedisConnection() : context_(nullptr), connected_(false) {
}

RedisConnection::~RedisConnection() {
    disconnect();
}

bool RedisConnection::parse_uri(const std::string& uri, std::string& host, int& port) {
    // Support formats: redis://host:port or host:port
    std::string clean_uri = uri;
    if (clean_uri.substr(0, 8) == "redis://") {
        clean_uri = clean_uri.substr(8);
    }

    auto colon_pos = clean_uri.find(':');
    if (colon_pos == std::string::npos) {
        host = clean_uri;
        port = 6379;
    } else {
        host = clean_uri.substr(0, colon_pos);
        port = std::stoi(clean_uri.substr(colon_pos + 1));
    }
    return true;
}

bool RedisConnection::connect(const std::string& uri) {
    if (connected_) {
        disconnect();
    }

    std::string host;
    int port = 6379;
    if (!parse_uri(uri, host, port)) {
        SPDLOG_ERROR("[Redis]Failed to parse URI: {}", uri);
        return false;
    }

    SPDLOG_INFO("[Redis]Connecting to {}:{}", host, port);

    context_ = redisConnect(host.c_str(), port);
    if (!context_ || context_->err) {
        if (context_) {
            SPDLOG_ERROR("[Redis]Connection failed: {}", context_->errstr);
            redisFree(context_);
            context_ = nullptr;
        } else {
            SPDLOG_ERROR("[Redis]Connection failed: cannot allocate context");
        }
        connected_ = false;
        return false;
    }

    connected_ = true;
    SPDLOG_INFO("[Redis]Connected to {}:{}", host, port);
    return true;
}

void RedisConnection::disconnect() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
    connected_ = false;
}

bool RedisConnection::is_connected() const {
    return connected_ && context_ != nullptr;
}

bool RedisConnection::set(const std::string& key, const std::string& value) {
    if (!is_connected()) {
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "SET %s %s", key.c_str(), value.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]SET command failed: connection lost");
        connected_ = false;
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_STATUS &&
                    std::string(reply->str) == "OK");
    if (!success) {
        SPDLOG_ERROR("[Redis]SET {} failed: {}", key, reply->str ? reply->str : "unknown");
    }

    freeReplyObject(reply);
    return success;
}

std::string RedisConnection::get(const std::string& key) {
    if (!is_connected()) {
        return "";
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "GET %s", key.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]GET command failed: connection lost");
        connected_ = false;
        return "";
    }

    std::string result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    }

    freeReplyObject(reply);
    return result;
}

bool RedisConnection::del(const std::string& key) {
    if (!is_connected()) {
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(context_, "DEL %s", key.c_str()));

    if (!reply) {
        SPDLOG_ERROR("[Redis]DEL command failed: connection lost");
        connected_ = false;
        return false;
    }

    bool success = (reply->type == REDIS_REPLY_INTEGER);
    if (!success) {
        SPDLOG_ERROR("[Redis]DEL {} failed: {}", key, reply->str ? reply->str : "unknown");
    }

    freeReplyObject(reply);
    return success;
}

}  // namespace farm
```

- [ ] **Step 3: Commit**

```bash
git add scripts/server/game_server/src/redis_connection.h scripts/server/game_server/src/redis_connection.cpp
git commit -m "feat(game_server): add RedisConnection hiredis wrapper"
```

---

### Task 2: Create LoginStub

**Files:**
- Create: `scripts/server/game_server/src/login_stub.h`
- Create: `scripts/server/game_server/src/login_stub.cpp`

- [ ] **Step 1: Create login_stub.h**

```cpp
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
```

- [ ] **Step 2: Create login_stub.cpp**

```cpp
#include "login_stub.h"
#include "game_clock.h"
#include "player_manager.h"
#include "dbmgr_connection_manager.h"
#include "redis_connection.h"
#include "player.h"
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
                     SendToGateFunc send_to_gate)
    : player_mgr_(player_mgr)
    , dbmgr_mgr_(dbmgr_mgr)
    , redis_conn_(redis_conn)
    , server_id_(server_id)
    , send_to_gate_(std::move(send_to_gate))
{
}

// ===========================================
// Account message handling (replaces AccountMessageHandler)
// ===========================================

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
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_QUERY_ROLES_RESP);

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
    uint64_t player_id = player_id_gen_.generate(server_id);

    dbmgr_mgr_->send_account_set_req(account_id,
        server_id, player_id, role_name,
        [this, session, account_id, player_id](int32_t code, const std::string& msg) {
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_CREATE_ROLE_RESP);

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
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_ACCOUNT_DATA_RESP);

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
            farm::AccountMessageResp resp;
            resp.set_account_id(account_id);
            resp.set_msg_id(MSG_ID_ACCOUNT_SET_RESP);

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

    if (!dbmgr_mgr_->is_connected(0)) {
        SPDLOG_ERROR("[LoginStub]DBMgr not available for EnterGameReq (account), player_id={}", req_player_id);

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

    auto callback = [this, session, account_id, req_player_id](uint64_t pid, bool success, const std::string& msg) {
        farm::AccountMessageResp resp;
        resp.set_account_id(account_id);
        resp.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp enter_resp;
        if (success) {
            Player* player = player_mgr_->get_player(pid);
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

                farm::EnergySync* energy_sync = enter_resp.mutable_energy();
                energy_sync->set_current(data.energy);
                energy_sync->set_max(farm::MAX_ENERGY);

                enter_resp.set_code(0);
                enter_resp.set_msg("success");

                if (clock_) {
                    farm::ClockSync* clock_sync = enter_resp.mutable_clock();
                    clock_sync->set_day(clock_->day());
                    clock_sync->set_time_slot(clock_->time_slot());
                    clock_sync->set_paused(clock_->paused());
                }

                // Mark player online in Redis
                mark_player_online(pid);
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

// ===========================================
// EnterGameReq from ClientMessage path
// ===========================================

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

    if (!dbmgr_mgr_->is_connected(0)) {
        SPDLOG_ERROR("[LoginStub]DBMgr not available for EnterGameReq, player_id={}", req_player_id);

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

    auto callback = [this, session, req_player_id](uint64_t pid, bool success, const std::string& msg) {
        farm::GameMessage game_msg;
        game_msg.set_player_id(req_player_id);
        game_msg.set_msg_id(MSG_ID_ENTER_GAME_RESP);

        farm::EnterGameResp resp;
        if (success) {
            Player* player = player_mgr_->get_player(pid);
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

                farm::EnergySync* energy_sync = resp.mutable_energy();
                energy_sync->set_current(data.energy);
                energy_sync->set_max(farm::MAX_ENERGY);

                resp.set_code(0);
                resp.set_msg("success");

                // Mark player online in Redis
                mark_player_online(pid);
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

// ===========================================
// Offline handling
// ===========================================

void LoginStub::on_player_offline(uint64_t player_id) {
    SPDLOG_INFO("[LoginStub]Player offline: player_id={}", player_id);
    if (redis_conn_ && redis_conn_->is_connected()) {
        std::string key = "player:online:" + std::to_string(player_id);
        redis_conn_->del(key);
        SPDLOG_INFO("[LoginStub]Removed Redis key: {}", key);
    }
}

// ===========================================
// Redis helper
// ===========================================

void LoginStub::mark_player_online(uint64_t player_id) {
    if (!redis_conn_ || !redis_conn_->is_connected()) {
        SPDLOG_WARN("[LoginStub]Redis not connected, skipping mark_player_online for player_id={}", player_id);
        return;
    }

    std::string key = "player:online:" + std::to_string(player_id);
    std::string value = std::to_string(server_id_);
    if (redis_conn_->set(key, value)) {
        SPDLOG_INFO("[LoginStub]Marked player online: {} -> {}", key, value);
    } else {
        SPDLOG_ERROR("[LoginStub]Failed to mark player online: player_id={}", player_id);
    }
}

}  // namespace farm
```

- [ ] **Step 3: Commit**

```bash
git add scripts/server/game_server/src/login_stub.h scripts/server/game_server/src/login_stub.cpp
git commit -m "feat(game_server): add LoginStub with Redis online tracking"
```

---

### Task 3: Create OnlineStub

**Files:**
- Create: `scripts/server/game_server/src/online_stub.h`
- Create: `scripts/server/game_server/src/online_stub.cpp`

- [ ] **Step 1: Create online_stub.h**

```cpp
#pragma once

#include <functional>
#include <string>
#include <cstdint>

namespace farm {

class RedisConnection;

class OnlineStub {
public:
    explicit OnlineStub(RedisConnection* redis_conn);
    ~OnlineStub() = default;

    // Query which server a player is on (0 = offline)
    using OnlineQueryCallback = std::function<void(uint64_t player_id, uint32_t server_id)>;
    void query_player_server(uint64_t player_id, OnlineQueryCallback callback);

private:
    RedisConnection* redis_conn_;
};

}  // namespace farm
```

- [ ] **Step 2: Create online_stub.cpp**

```cpp
#include "online_stub.h"
#include "redis_connection.h"
#include "log_macros.h"

#include <string>

namespace farm {

OnlineStub::OnlineStub(RedisConnection* redis_conn)
    : redis_conn_(redis_conn)
{
}

void OnlineStub::query_player_server(uint64_t player_id, OnlineQueryCallback callback) {
    if (!redis_conn_ || !redis_conn_->is_connected()) {
        SPDLOG_WARN("[OnlineStub]Redis not connected, returning offline for player_id={}", player_id);
        if (callback) {
            callback(player_id, 0);
        }
        return;
    }

    std::string key = "player:online:" + std::to_string(player_id);
    std::string value = redis_conn_->get(key);

    uint32_t server_id = 0;
    if (!value.empty()) {
        try {
            server_id = static_cast<uint32_t>(std::stoul(value));
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[OnlineStub]Failed to parse server_id from Redis: key={} value={} error={}",
                         key, value, e.what());
        }
    }

    if (callback) {
        callback(player_id, server_id);
    }
}

}  // namespace farm
```

- [ ] **Step 3: Commit**

```bash
git add scripts/server/game_server/src/online_stub.h scripts/server/game_server/src/online_stub.cpp
git commit -m "feat(game_server): add OnlineStub for online status queries"
```

---

### Task 4: Integrate Stubs into GameServer

**Files:**
- Modify: `scripts/server/game_server/src/game_server.h:31-127`
- Modify: `scripts/server/game_server/src/game_server.cpp:33-46,99-134,284-294,435-437,576-702`

- [ ] **Step 1: Update game_server.h**

Add includes and new members to `game_server.h`:

```cpp
// Add after existing includes
#include "login_stub.h"
#include "online_stub.h"
#include "redis_connection.h"
```

Replace `AccountMessageHandler` forward declaration with:

```cpp
// Remove: class AccountMessageHandler;
// LoginStub, OnlineStub, RedisConnection are now included via headers
```

Add new members to GameServer class (after `item_handler_`):

```cpp
    // Redis connection
    RedisConnection redis_conn_;

    // Stubs
    std::unique_ptr<LoginStub> login_stub_;
    std::unique_ptr<OnlineStub> online_stub_;
```

Update constructor signature to accept Redis URI:

```cpp
    GameServer(const std::string& ip, uint16_t port,
               const std::vector<DBMgrConfig>& dbmgr_configs = {},
               const std::string& redis_uri = "");
```

Add `redis_uri_` member:

```cpp
    std::string redis_uri_;
```

Remove `account_handler_` member:

```cpp
    // Remove: std::unique_ptr<AccountMessageHandler> account_handler_;
```

- [ ] **Step 2: Update game_server.cpp constructor**

Update constructor to accept and store redis_uri:

```cpp
GameServer::GameServer(const std::string& ip, uint16_t port,
                       const std::vector<DBMgrConfig>& dbmgr_configs,
                       const std::string& redis_uri)
    : ip_(ip)
    , port_(port)
    , base_(nullptr)
    , listener_(nullptr)
    , heartbeat_timer_(nullptr)
    , update_timer_(nullptr)
    , running_(false)
    , dbmgr_configs_(dbmgr_configs)
    , redis_uri_(redis_uri)
    , item_handler_()
{
}
```

- [ ] **Step 3: Update game_server.cpp start() to initialize stubs**

In `start()`, replace the `account_handler_` initialization block with stub initialization:

Remove:
```cpp
    account_handler_ = std::make_unique<AccountMessageHandler>(&dbmgr_mgr_, &player_mgr_, send_to_gate_func);
    account_handler_->set_clock(game_clock_.get());
```

Add (after `game_clock_` initialization):
```cpp
    // Initialize Redis connection
    if (!redis_uri_.empty()) {
        if (redis_conn_.connect(redis_uri_)) {
            SPDLOG_INFO("[Game]Redis connected: {}", redis_uri_);
        } else {
            SPDLOG_WARN("[Game]Redis connection failed: {} (online tracking disabled)", redis_uri_);
        }
    } else {
        SPDLOG_INFO("[Game]No Redis URI configured, online tracking disabled");
    }

    // Initialize stubs
    login_stub_ = std::make_unique<LoginStub>(&player_mgr_, &dbmgr_mgr_, &redis_conn_, server_id, send_to_gate_func);
    login_stub_->set_clock(game_clock_.get());
    online_stub_ = std::make_unique<OnlineStub>(&redis_conn_);
    SPDLOG_INFO("[Game]LoginStub and OnlineStub initialized");
```

Note: `server_id` needs to be extracted from config. Add to the config parsing in `main.cpp` and pass to GameServer.

- [ ] **Step 4: Update route_internal_message to use LoginStub**

Replace `account_handler_->handle` with `login_stub_->handle_account_msg`:

```cpp
        case MSG_ID_ACCOUNT_MSG:
            login_stub_->handle_account_msg(session, payload);
            break;
```

- [ ] **Step 5: Update handle_enter_game_req to delegate to LoginStub**

Replace the entire `handle_enter_game_req` method body with delegation:

```cpp
void GameServer::handle_enter_game_req(std::shared_ptr<GateSession> session,
                                        uint64_t player_id, const std::string& payload) {
    login_stub_->handle_enter_game_req(session, player_id, payload);
}
```

- [ ] **Step 6: Update handle_disconnect to notify LoginStub**

Add LoginStub notification in `handle_disconnect`:

```cpp
void GameServer::handle_disconnect(std::shared_ptr<GateSession> session) {
    evutil_socket_t fd = session->fd();
    std::string gate_id = session->gate_id();
    SPDLOG_INFO("[Game]Gate disconnected fd={} gate_id={}", fd, gate_id);

    // Collect player IDs before removing
    std::vector<uint64_t> player_ids;
    for (auto& kv : player_mgr_.get_all_players()) {
        // This won't work - need a different approach
    }

    // 清理该 Gate 关联的所有玩家（并保存数据）
    player_mgr_.remove_players_by_gate_with_save(session.get());

    // Note: LoginStub offline notification is handled via PlayerManager callback (Task 5)

    // 移除会话
    gate_sessions_.erase(fd);
}
```

- [ ] **Step 7: Update stop() to disconnect Redis**

Add Redis disconnect in `stop()`:

```cpp
void GameServer::stop() {
    running_ = false;
    // Disconnect Redis
    redis_conn_.disconnect();
    // Shutdown DBMgr connections first
    dbmgr_mgr_.shutdown();
    // ... rest of existing code
}
```

- [ ] **Step 8: Remove AccountMessageHandler include**

Remove from game_server.cpp:

```cpp
// Remove: #include "account_message_handler.h"
```

- [ ] **Step 9: Commit**

```bash
git add scripts/server/game_server/src/game_server.h scripts/server/game_server/src/game_server.cpp
git commit -m "feat(game_server): integrate LoginStub and OnlineStub into GameServer"
```

---

### Task 5: Add Offline Callback to PlayerManager

**Files:**
- Modify: `scripts/server/game_server/src/player_manager.h:15-72`
- Modify: `scripts/server/game_server/src/player_manager.cpp:88-147`

- [ ] **Step 1: Update player_manager.h**

Add offline callback type and setter:

```cpp
// Add after PlayerJoinCallback
using PlayerOfflineCallback = std::function<void(uint64_t player_id)>;
```

Add to PlayerManager class:

```cpp
    // Set callback for player offline events
    void set_offline_callback(PlayerOfflineCallback callback) {
        offline_callback_ = std::move(callback);
    }
```

Add private member:

```cpp
    PlayerOfflineCallback offline_callback_;
```

- [ ] **Step 2: Update player_manager.cpp remove methods**

Update `remove_player` to notify:

```cpp
void PlayerManager::remove_player(uint64_t player_id) {
    if (offline_callback_) {
        offline_callback_(player_id);
    }
    players_.erase(player_id);
}
```

Update `remove_player_with_save` to notify:

```cpp
void PlayerManager::remove_player_with_save(uint64_t player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return;
    }

    save_player_data(player_id);

    if (offline_callback_) {
        offline_callback_(player_id);
    }

    players_.erase(it);
}
```

Update `remove_players_by_gate` to notify:

```cpp
void PlayerManager::remove_players_by_gate(GateSession* gate_session) {
    std::vector<uint64_t> to_remove;
    for (auto& kv : players_) {
        if (kv.second->gate_session() == gate_session) {
            to_remove.push_back(kv.first);
        }
    }
    for (uint64_t pid : to_remove) {
        SPDLOG_INFO("[Player]Removing player {} due to Gate disconnect", pid);
        if (offline_callback_) {
            offline_callback_(pid);
        }
        players_.erase(pid);
    }
}
```

Update `remove_players_by_gate_with_save` to notify:

```cpp
void PlayerManager::remove_players_by_gate_with_save(GateSession* gate_session) {
    std::vector<uint64_t> to_remove;
    for (auto& kv : players_) {
        if (kv.second->gate_session() == gate_session) {
            to_remove.push_back(kv.first);
        }
    }
    for (uint64_t pid : to_remove) {
        SPDLOG_INFO("[Player]Removing player {} due to Gate disconnect (with save)", pid);
        save_player_data(pid);
        if (offline_callback_) {
            offline_callback_(pid);
        }
        players_.erase(pid);
    }
}
```

- [ ] **Step 3: Wire up in GameServer::start()**

Add after LoginStub initialization:

```cpp
    // Wire up offline callback
    player_mgr_.set_offline_callback([this](uint64_t player_id) {
        login_stub_->on_player_offline(player_id);
    });
```

- [ ] **Step 4: Commit**

```bash
git add scripts/server/game_server/src/player_manager.h scripts/server/game_server/src/player_manager.cpp scripts/server/game_server/src/game_server.cpp
git commit -m "feat(game_server): add offline callback to PlayerManager for Redis cleanup"
```

---

### Task 6: Update main.cpp and Config

**Files:**
- Modify: `scripts/server/game_server/src/main.cpp:23-90`
- Modify: `config/game_server.json`

- [ ] **Step 1: Update main.cpp to parse Redis config and server_id**

Add Redis config parsing after DBMgr config parsing:

```cpp
    // Parse Redis config
    std::string redis_uri = config.value("/redis/uri"_json_pointer, "");

    // Parse server ID
    uint32_t server_id = config.value("/server/id"_json_pointer, 1u);
```

Update GameServer construction:

```cpp
    farm::GameServer server(ip, port, dbmgr_configs, redis_uri);
```

Add logging:

```cpp
    SPDLOG_INFO("[Main]Server ID: {}", server_id);
    if (!redis_uri.empty()) {
        SPDLOG_INFO("[Main]Redis: {}", redis_uri);
    }
```

- [ ] **Step 2: Update config/game_server.json**

Add Redis config block and server ID:

```json
{
    "server": {
        "id": 1,
        "ip": "0.0.0.0",
        "port": 6000
    },
    "redis": {
        "uri": "redis://localhost:6379"
    },
    "dbmgrs": [
        {
            "host": "127.0.0.1",
            "port": 5000
        }
    ],
    "pid_file": "./runtimeData/game_server.pid",
    "logging": {
        "dir": "./runtimeData/logs/server",
        "level": "debug",
        "max_file_size_mb": 20,
        "max_files": 7
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add scripts/server/game_server/src/main.cpp config/game_server.json
git commit -m "feat(game_server): add Redis config and server_id to main.cpp"
```

---

### Task 7: Update CMakeLists.txt

**Files:**
- Modify: `scripts/server/game_server/CMakeLists.txt:1-203`

- [ ] **Step 1: Add hiredis include path**

Add after LIBEVENT_ROOT:

```cmake
set(HIREDIS_ROOT "C:/hiredis_install")
```

- [ ] **Step 2: Add new source files to SOURCES**

Add to SOURCES list:

```cmake
    src/redis_connection.cpp
    src/login_stub.cpp
    src/online_stub.cpp
```

- [ ] **Step 3: Add hiredis include directory**

Add to target_include_directories:

```cmake
    ${HIREDIS_ROOT}/include
```

- [ ] **Step 4: Add hiredis library**

Add to target_link_libraries (MSVC section):

```cmake
        hiredis.lib
```

Add to target_link_libraries (Linux section):

```cmake
        hiredis
```

- [ ] **Step 5: Remove account_message_handler from SOURCES**

Remove from SOURCES:

```cmake
    # Remove: src/account_message_handler.cpp
```

- [ ] **Step 6: Update TEST_SOURCES similarly**

Add new source files and remove account_message_handler.cpp from TEST_SOURCES.

- [ ] **Step 7: Commit**

```bash
git add scripts/server/game_server/CMakeLists.txt
git commit -m "build(game_server): add hiredis and new stub source files"
```

---

### Task 8: Delete AccountMessageHandler

**Files:**
- Delete: `scripts/server/game_server/src/account_message_handler.h`
- Delete: `scripts/server/game_server/src/account_message_handler.cpp`

- [ ] **Step 1: Delete the files**

```bash
rm scripts/server/game_server/src/account_message_handler.h
rm scripts/server/game_server/src/account_message_handler.cpp
```

- [ ] **Step 2: Verify no remaining references**

Search for any remaining includes or references to account_message_handler.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "refactor(game_server): remove AccountMessageHandler (replaced by LoginStub)"
```

---

### Task 9: Build and Verify

**Files:**
- None (verification only)

- [ ] **Step 1: Build the project**

```bash
cd scripts/server/game_server
mkdir -p build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

- [ ] **Step 2: Fix any compilation errors**

Address missing includes, type mismatches, or linker errors.

- [ ] **Step 3: Verify hiredis links correctly**

Check that the binary can load hiredis.dll at runtime.

- [ ] **Step 4: Commit any fixes**

```bash
git add -A
git commit -m "fix(game_server): resolve build issues for stub integration"
```

---

## Summary

| Task | Description | New Files | Modified Files | Deleted Files |
|------|-------------|-----------|----------------|---------------|
| 1 | RedisConnection | 2 | 0 | 0 |
| 2 | LoginStub | 2 | 0 | 0 |
| 3 | OnlineStub | 2 | 0 | 0 |
| 4 | Integrate into GameServer | 0 | 2 | 0 |
| 5 | PlayerManager offline callback | 0 | 2 | 0 |
| 6 | main.cpp + config | 0 | 2 | 0 |
| 7 | CMakeLists.txt | 0 | 1 | 0 |
| 8 | Delete AccountMessageHandler | 0 | 0 | 2 |
| 9 | Build verification | 0 | 0 | 0 |
| **Total** | | **6** | **7** | **2** |
