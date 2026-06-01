#include "dbmgr_connection_manager.h"
#include "internal_msg_ids.h"
#include "message_parser.h"

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <cstring>

#include "dbmgr.pb.h"
#include "account.pb.h"
#include "log_macros.h"

namespace farm {

DBMgrConnectionManager::DBMgrConnectionManager()
    : base_(nullptr)
    , heartbeat_timer_(nullptr)
    , reconnect_timer_(nullptr)
    , running_(false)
    , next_request_id_(1)
{
}

DBMgrConnectionManager::~DBMgrConnectionManager() {
    shutdown();
}

bool DBMgrConnectionManager::init(struct event_base* base, const std::vector<DBMgrConfig>& configs) {
    if (!base || configs.empty()) {
        SPDLOG_ERROR("[DBMgrConnection]Invalid params: base={} configs_size={}", fmt::ptr(base), configs.size());
        return false;
    }

    base_ = base;
    running_ = true;

    // Connect to all configured DBMgrs
    for (uint32_t i = 0; i < configs.size(); i++) {
        connect_to_dbmgr(i, configs[i].host, configs[i].port);
    }

    // Create heartbeat timeout check timer (every 5 seconds)
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    heartbeat_timer_ = event_new(base_, -1, EV_PERSIST, on_heartbeat_timer, this);
    evtimer_add(heartbeat_timer_, &tv);

    // Create reconnect timer (every 5 seconds)
    reconnect_timer_ = event_new(base_, -1, EV_PERSIST, on_reconnect_timer, this);
    evtimer_add(reconnect_timer_, &tv);

    SPDLOG_INFO("[DBMgrConnection]Initialized with {} DBMgr configs", configs.size());
    return true;
}

void DBMgrConnectionManager::shutdown() {
    running_ = false;

    if (heartbeat_timer_) {
        event_free(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
    if (reconnect_timer_) {
        event_free(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }

    // Fail all pending requests
    for (auto& [request_id, request] : pending_requests_) {
        if (request.callback) {
            request.callback(-1, nullptr, 0);
        }
    }
    pending_requests_.clear();

    // Clear all connections (destructor frees bufferevent)
    bev_to_index_.clear();
    connections_.clear();

    SPDLOG_INFO("[DBMgrConnection]Shutdown complete");
}

// ===========================================
// Connection lifecycle
// ===========================================

void DBMgrConnectionManager::connect_to_dbmgr(uint32_t index, const std::string& host, uint16_t port) {
    // Create bufferevent for outbound connection
    struct bufferevent* bev = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        SPDLOG_ERROR("[DBMgrConnection]Failed to create bufferevent for DBMgr index={}", index);
        return;
    }

    // Create connection object
    auto conn = std::make_unique<DBMgrConnection>(index, host, port, bev);
    bev_to_index_[bev] = index;

    // Set callbacks (ctx = this)
    bufferevent_setcb(bev, on_read, nullptr, on_event, this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    // Initiate async TCP connect
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &sin.sin_addr);

    int ret = bufferevent_socket_connect(bev, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin));
    if (ret < 0) {
        SPDLOG_ERROR("[DBMgrConnection]connect_to_dbmgr failed for index={} {}:{}", index, host, port);
        bev_to_index_.erase(bev);
        return;
    }

    // Store connection (resize if needed)
    if (index >= connections_.size()) {
        connections_.resize(index + 1);
    }
    connections_[index] = std::move(conn);

    SPDLOG_INFO("[DBMgrConnection]Connecting to DBMgr index={} at {}:{}", index, host, port);
}

void DBMgrConnectionManager::handle_connected(DBMgrConnection* conn) {
    SPDLOG_INFO("[DBMgrConnection]TCP connected to DBMgr index={} at {}:{}", conn->config_index(), conn->host(), conn->port());
    // Now waiting for DBMgrIdentify message from DBMgr
}

void DBMgrConnectionManager::handle_disconnect(DBMgrConnection* conn) {
    if (conn->state() == DBMgrConnectionState::DISCONNECTED) {
        return;  // Already handled
    }

    SPDLOG_INFO("[DBMgrConnection]DBMgr disconnected index={} state={}", conn->config_index(), static_cast<int>(conn->state()));

    conn->set_state(DBMgrConnectionState::DISCONNECTED);

    // Remove bev -> index mapping
    struct bufferevent* old_bev = conn->bev();
    if (old_bev) {
        bev_to_index_.erase(old_bev);
        // Free the bev now (safe inside libevent callback).
        // Then null out the pointer in the connection so the destructor won't double-free.
        bufferevent_free(old_bev);
        conn->release_bev();  // Sets internal bev_ to nullptr
    }

    // Fail all pending requests for this DBMgr
    fail_pending_requests_for(conn->config_index());

    // Note: reconnect timer will attempt to reconnect periodically
}

void DBMgrConnectionManager::handle_read(DBMgrConnection* conn) {
    struct bufferevent* bev = conn->bev();
    struct evbuffer* input = bufferevent_get_input(bev);

    // Read all available data into connection's read buffer
    char buf[4096];
    while (true) {
        ev_ssize_t n = evbuffer_remove(input, buf, sizeof(buf));
        if (n <= 0) break;
        conn->append_read_data(reinterpret_cast<const uint8_t*>(buf), n);
    }

    // Try to parse complete messages
    auto& read_buf = conn->read_buffer();
    while (!read_buf.empty()) {
        ParsedMessage msg;
        size_t consumed = 0;
        if (MessageParser::try_parse(read_buf.data(), read_buf.size(), msg, consumed)) {
            conn->consume_read_data(consumed);
            route_message(conn, msg.msg_id, msg.payload);
        } else {
            break;  // Incomplete data, wait for more
        }
    }
}

// ===========================================
// libevent callbacks (static)
// ===========================================

void DBMgrConnectionManager::on_read(struct bufferevent* bev, void* ctx) {
    auto* mgr = static_cast<DBMgrConnectionManager*>(ctx);
    auto conn = mgr->find_connection_by_bev(bev);
    if (!conn) return;
    mgr->handle_read(*conn);
}

void DBMgrConnectionManager::on_event(struct bufferevent* bev, short events, void* ctx) {
    auto* mgr = static_cast<DBMgrConnectionManager*>(ctx);
    auto conn = mgr->find_connection_by_bev(bev);

    if (events & BEV_EVENT_CONNECTED) {
        if (conn) {
            mgr->handle_connected(*conn);
        }
        return;
    }

    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        if (conn) {
            mgr->handle_disconnect(*conn);
        }
    }
}

void DBMgrConnectionManager::on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* mgr = static_cast<DBMgrConnectionManager*>(ctx);
    mgr->check_heartbeat();
}

void DBMgrConnectionManager::on_reconnect_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* mgr = static_cast<DBMgrConnectionManager*>(ctx);
    mgr->try_reconnect();
}

// ===========================================
// Message routing
// ===========================================

void DBMgrConnectionManager::route_message(DBMgrConnection* conn, uint32_t msg_id,
                                            const std::vector<uint8_t>& payload) {
    // Before identification, only allow DBMgrIdentify
    if (conn->state() == DBMgrConnectionState::CONNECTED) {
        if (msg_id == MSG_ID_DBMGR_IDENTIFY) {
            handle_dbmgr_identify(conn, payload);
        } else {
            SPDLOG_INFO("[DBMgrConnection]Ignoring msg_id={} from unidentified DBMgr index={}", msg_id, conn->config_index());
        }
        return;
    }

    // Identified connection: route by msg_id
    switch (msg_id) {
        case MSG_ID_DBMGR_HEARTBEAT:
            handle_dbmgr_heartbeat(conn, payload);
            break;
        case MSG_ID_PLAYER_DATA_RESP:
            handle_player_data_resp(conn, payload);
            break;
        case MSG_ID_ACCOUNT_DATA_RESP:
            handle_account_data_resp(conn, payload);
            break;
        case MSG_ID_ACCOUNT_SET_RESP:
            handle_account_set_resp(conn, payload);
            break;
        case MSG_ID_ALLOC_PLAYER_ID_RESP:
            handle_alloc_player_id_resp(conn, payload);
            break;
        default:
            SPDLOG_INFO("[DBMgrConnection]Unknown msg_id={} from DBMgr index={}", msg_id, conn->config_index());
            break;
    }
}

void DBMgrConnectionManager::handle_dbmgr_identify(DBMgrConnection* conn,
                                                     const std::vector<uint8_t>& payload) {
    farm::DBMgrIdentify identify;
    if (!payload.empty() && !identify.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[DBMgrConnMgr]Failed to parse DBMgrIdentify");
        return;
    }

    conn->set_remote_index(identify.index());
    conn->set_remote_address(identify.address());
    conn->set_state(DBMgrConnectionState::IDENTIFIED);
    conn->update_heartbeat();

    SPDLOG_INFO("[DBMgrConnection]DBMgr identified: config_index={} remote_index={} address={}", conn->config_index(), identify.index(), identify.address());

    // Send DBMgrIdentifyResp
    farm::DBMgrIdentifyResp resp;
    resp.set_code(0);
    resp.set_msg("identified");
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_dbmgr(conn, MSG_ID_DBMGR_IDENTIFY_RESP, resp_data);
}

void DBMgrConnectionManager::handle_dbmgr_heartbeat(DBMgrConnection* conn,
                                                      const std::vector<uint8_t>& payload) {
    conn->update_heartbeat();

    // Parse heartbeat for timestamp
    farm::DBMgrHeartbeat hb;
    if (!payload.empty() && !hb.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[DBMgrConnMgr]Failed to parse DBMgrHeartbeat");
        return;
    }

    // Reply with DBMgrHeartbeatResp
    farm::DBMgrHeartbeatResp resp;
    resp.set_timestamp(static_cast<uint64_t>(std::time(nullptr)));
    std::string resp_data;
    resp.SerializeToString(&resp_data);

    send_to_dbmgr(conn, MSG_ID_DBMGR_HEARTBEAT_RESP, resp_data);
}

void DBMgrConnectionManager::handle_player_data_resp(DBMgrConnection* conn,
                                                       const std::vector<uint8_t>& payload) {
    farm::PlayerDataResp resp;
    if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[DBMgrConnMgr]Failed to parse PlayerDataResp");
        return;
    }

    uint64_t request_id = resp.request_id();

    auto it = pending_requests_.find(request_id);
    if (it == pending_requests_.end()) {
        SPDLOG_INFO("[DBMgrConnection]No pending request for request_id={}", request_id);
        return;
    }

    // Invoke callback
    if (it->second.callback) {
        const std::string& value = resp.value();
        it->second.callback(resp.code(),
                            reinterpret_cast<const uint8_t*>(value.data()),
                            value.size());
    }

    // Remove from pending
    pending_requests_.erase(it);
}

void DBMgrConnectionManager::handle_account_data_resp(DBMgrConnection* conn,
                                                        const std::vector<uint8_t>& payload) {
    farm::AccountDataResp resp;
    if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[DBMgrConnMgr]Failed to parse AccountDataResp");
        return;
    }

    // Find pending request by account_id (stored in request_id for simplicity)
    // We need to find the matching request - for now we'll use a simple approach
    // In a real implementation, we'd need a better matching mechanism
    uint64_t request_id = 0;
    for (auto& [req_id, request] : pending_account_requests_) {
        if (request.dbmgr_index == conn->config_index()) {
            request_id = req_id;
            break;
        }
    }

    if (request_id == 0) {
        SPDLOG_INFO("[DBMgrConnection]No pending account request for DBMgr index={}", conn->config_index());
        return;
    }

    auto it = pending_account_requests_.find(request_id);
    if (it == pending_account_requests_.end()) {
        return;
    }

    // Invoke callback
    if (it->second.data_callback) {
        std::vector<std::tuple<uint32_t, uint64_t, std::string>> roles;
        for (const auto& role : resp.roles()) {
            roles.emplace_back(role.server_id(), role.player_id(), role.role_name());
        }
        it->second.data_callback(resp.code(), roles);
    }

    // Remove from pending
    pending_account_requests_.erase(it);
}

void DBMgrConnectionManager::handle_account_set_resp(DBMgrConnection* conn,
                                                       const std::vector<uint8_t>& payload) {
    farm::AccountSetResp resp;
    if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[DBMgrConnMgr]Failed to parse AccountSetResp");
        return;
    }

    // Find pending request
    uint64_t request_id = 0;
    for (auto& [req_id, request] : pending_account_requests_) {
        if (request.dbmgr_index == conn->config_index()) {
            request_id = req_id;
            break;
        }
    }

    if (request_id == 0) {
        SPDLOG_INFO("[DBMgrConnection]No pending account set request for DBMgr index={}", conn->config_index());
        return;
    }

    auto it = pending_account_requests_.find(request_id);
    if (it == pending_account_requests_.end()) {
        return;
    }

    // Invoke callback
    if (it->second.set_callback) {
        it->second.set_callback(resp.code(), resp.msg());
    }

    // Remove from pending
    pending_account_requests_.erase(it);
}

void DBMgrConnectionManager::handle_alloc_player_id_resp(DBMgrConnection* conn,
                                                           const std::vector<uint8_t>& payload) {
    farm::AllocPlayerIdResp resp;
    if (!payload.empty() && !resp.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        SPDLOG_ERROR("[DBMgrConnMgr]Failed to parse AllocPlayerIdResp");
        return;
    }

    uint64_t request_id = resp.request_id();

    auto it = pending_alloc_id_requests_.find(request_id);
    if (it == pending_alloc_id_requests_.end()) {
        SPDLOG_INFO("[DBMgrConnection]No pending alloc_id request for request_id={}", request_id);
        return;
    }

    if (it->second.callback) {
        it->second.callback(resp.code(), resp.start_id(), resp.count());
    }

    pending_alloc_id_requests_.erase(it);
}

// ===========================================
// Heartbeat check
// ===========================================

void DBMgrConnectionManager::check_heartbeat() {
    time_t now = std::time(nullptr);

    for (auto& conn_ptr : connections_) {
        if (!conn_ptr) continue;
        if (conn_ptr->state() != DBMgrConnectionState::IDENTIFIED) continue;

        if (now - conn_ptr->last_heartbeat() > DBMGR_HEARTBEAT_TIMEOUT) {
            SPDLOG_INFO("[DBMgrConnection]Heartbeat timeout for DBMgr index={}", conn_ptr->config_index());
            handle_disconnect(conn_ptr.get());
        }
    }
}

// ===========================================
// Reconnect
// ===========================================

void DBMgrConnectionManager::try_reconnect() {
    for (auto& conn_ptr : connections_) {
        if (!conn_ptr) continue;
        if (conn_ptr->state() != DBMgrConnectionState::DISCONNECTED) continue;

        uint32_t idx = conn_ptr->config_index();
        std::string host = conn_ptr->host();
        uint16_t port = conn_ptr->port();

        SPDLOG_INFO("[DBMgrConnection]Attempting reconnect to DBMgr index={} at {}:{}", idx, host, port);

        // Remove old connection
        bev_to_index_.erase(conn_ptr->bev());
        connections_[idx].reset();

        // Create new connection
        connect_to_dbmgr(idx, host, port);
    }
}

// ===========================================
// Request management
// ===========================================

uint64_t DBMgrConnectionManager::generate_request_id() {
    return next_request_id_.fetch_add(1);
}

uint64_t DBMgrConnectionManager::send_player_data_req(uint64_t player_id, int32_t op,
                                                        const std::string& key, const std::string& value,
                                                        PlayerDataCallback callback) {
    if (connections_.empty()) {
        SPDLOG_ERROR("[DBMgrConnection]No DBMgr connections configured");
        if (callback) callback(-1, nullptr, 0);
        return 0;
    }

    // Route by player_id % dbmgr_count
    uint32_t dbmgr_index = static_cast<uint32_t>(player_id % connections_.size());

    // Find the target connection
    if (dbmgr_index >= connections_.size() || !connections_[dbmgr_index]) {
        SPDLOG_ERROR("[DBMgrConnection]Invalid dbmgr_index={}", dbmgr_index);
        if (callback) callback(-1, nullptr, 0);
        return 0;
    }

    auto& conn = connections_[dbmgr_index];
    if (conn->state() != DBMgrConnectionState::IDENTIFIED) {
        SPDLOG_ERROR("[DBMgrConnection]DBMgr index={} not connected", dbmgr_index);
        if (callback) callback(-1, nullptr, 0);
        return 0;
    }

    // Generate request_id
    uint64_t request_id = generate_request_id();

    // Build PlayerDataReq
    farm::PlayerDataReq req;
    req.set_request_id(request_id);
    req.set_player_id(player_id);
    req.set_op(static_cast<farm::PlayerDataOp>(op));
    req.set_key(key);
    if (!value.empty()) {
        req.set_value(value);
    }

    std::string req_data;
    req.SerializeToString(&req_data);

    // Register pending request
    PendingRequest pending;
    pending.request_id = request_id;
    pending.dbmgr_index = dbmgr_index;
    pending.callback = std::move(callback);
    pending.send_time = std::time(nullptr);
    pending_requests_[request_id] = std::move(pending);

    // Send
    send_to_dbmgr(conn.get(), MSG_ID_PLAYER_DATA_REQ, req_data);

    return request_id;
}

uint64_t DBMgrConnectionManager::send_account_data_req(const std::string& account_id,
                                                         AccountDataCallback callback) {
    if (connections_.empty()) {
        SPDLOG_ERROR("[DBMgrConnection]No DBMgr connections configured");
        if (callback) callback(-1, {});
        return 0;
    }

    // Route by hash(account_id) % dbmgr_count
    uint32_t dbmgr_index = hash_account_id(account_id);

    // Find the target connection
    if (dbmgr_index >= connections_.size() || !connections_[dbmgr_index]) {
        SPDLOG_ERROR("[DBMgrConnection]Invalid dbmgr_index={}", dbmgr_index);
        if (callback) callback(-1, {});
        return 0;
    }

    auto& conn = connections_[dbmgr_index];
    if (conn->state() != DBMgrConnectionState::IDENTIFIED) {
        SPDLOG_ERROR("[DBMgrConnection]DBMgr index={} not connected", dbmgr_index);
        if (callback) callback(-1, {});
        return 0;
    }

    // Generate request_id
    uint64_t request_id = generate_request_id();

    // Build AccountDataReq
    farm::AccountDataReq req;
    req.set_account_id(account_id);

    std::string req_data;
    req.SerializeToString(&req_data);

    // Register pending request
    PendingAccountRequest pending;
    pending.request_id = request_id;
    pending.dbmgr_index = dbmgr_index;
    pending.data_callback = std::move(callback);
    pending.send_time = std::time(nullptr);
    pending_account_requests_[request_id] = std::move(pending);

    // Send
    send_to_dbmgr(conn.get(), MSG_ID_ACCOUNT_DATA_REQ, req_data);

    return request_id;
}

uint64_t DBMgrConnectionManager::send_account_set_req(const std::string& account_id,
                                                        uint32_t server_id, uint64_t player_id,
                                                        const std::string& role_name,
                                                        AccountSetCallback callback) {
    if (connections_.empty()) {
        SPDLOG_ERROR("[DBMgrConnection]No DBMgr connections configured");
        if (callback) callback(-1, "No DBMgr connections");
        return 0;
    }

    // Route by hash(account_id) % dbmgr_count
    uint32_t dbmgr_index = hash_account_id(account_id);

    // Find the target connection
    if (dbmgr_index >= connections_.size() || !connections_[dbmgr_index]) {
        SPDLOG_ERROR("[DBMgrConnection]Invalid dbmgr_index={}", dbmgr_index);
        if (callback) callback(-1, "Invalid DBMgr index");
        return 0;
    }

    auto& conn = connections_[dbmgr_index];
    if (conn->state() != DBMgrConnectionState::IDENTIFIED) {
        SPDLOG_ERROR("[DBMgrConnection]DBMgr index={} not connected", dbmgr_index);
        if (callback) callback(-1, "DBMgr not connected");
        return 0;
    }

    // Generate request_id
    uint64_t request_id = generate_request_id();

    // Build AccountSetReq
    farm::AccountSetReq req;
    req.set_account_id(account_id);
    auto* new_role = req.mutable_new_role();
    new_role->set_server_id(server_id);
    new_role->set_player_id(player_id);
    new_role->set_role_name(role_name);

    std::string req_data;
    req.SerializeToString(&req_data);

    // Register pending request
    PendingAccountRequest pending;
    pending.request_id = request_id;
    pending.dbmgr_index = dbmgr_index;
    pending.set_callback = std::move(callback);
    pending.send_time = std::time(nullptr);
    pending_account_requests_[request_id] = std::move(pending);

    // Send
    send_to_dbmgr(conn.get(), MSG_ID_ACCOUNT_SET_REQ, req_data);

    return request_id;
}

uint64_t DBMgrConnectionManager::send_alloc_player_id_req(uint32_t count,
                                                            AllocPlayerIdCallback callback) {
    if (connections_.empty()) {
        SPDLOG_ERROR("[DBMgrConnection]No DBMgr connections configured");
        if (callback) callback(-1, 0, 0);
        return 0;
    }

    uint32_t dbmgr_index = 0;

    if (dbmgr_index >= connections_.size() || !connections_[dbmgr_index]) {
        SPDLOG_ERROR("[DBMgrConnection]Invalid dbmgr_index={}", dbmgr_index);
        if (callback) callback(-1, 0, 0);
        return 0;
    }

    auto& conn = connections_[dbmgr_index];
    if (conn->state() != DBMgrConnectionState::IDENTIFIED) {
        SPDLOG_ERROR("[DBMgrConnection]DBMgr index={} not connected", dbmgr_index);
        if (callback) callback(-1, 0, 0);
        return 0;
    }

    uint64_t request_id = generate_request_id();

    farm::AllocPlayerIdReq req;
    req.set_request_id(request_id);
    req.set_count(count);

    std::string req_data;
    req.SerializeToString(&req_data);

    PendingAllocIdRequest pending;
    pending.request_id = request_id;
    pending.dbmgr_index = dbmgr_index;
    pending.callback = std::move(callback);
    pending.send_time = std::time(nullptr);
    pending_alloc_id_requests_[request_id] = std::move(pending);

    send_to_dbmgr(conn.get(), MSG_ID_ALLOC_PLAYER_ID_REQ, req_data);

    SPDLOG_INFO("[DBMgrConnection]Sent AllocPlayerIdReq request_id={} count={}", request_id, count);
    return request_id;
}

uint32_t DBMgrConnectionManager::hash_account_id(std::string_view account_id) const {
    // Simple hash algorithm (djb2)
    uint32_t hash = 5381;
    for (char c : account_id) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % connections_.size();
}

void DBMgrConnectionManager::fail_pending_requests_for(uint32_t dbmgr_index) {
    std::vector<uint64_t> to_remove;

    for (auto& [request_id, request] : pending_requests_) {
        if (request.dbmgr_index == dbmgr_index) {
            if (request.callback) {
                request.callback(-1, nullptr, 0);
            }
            to_remove.push_back(request_id);
        }
    }

    for (uint64_t rid : to_remove) {
        pending_requests_.erase(rid);
    }

    if (!to_remove.empty()) {
        SPDLOG_ERROR("[DBMgrConnection]Failed {} pending requests for DBMgr index={}", to_remove.size(), dbmgr_index);
    }
}

void DBMgrConnectionManager::cleanup_stale_requests() {
    // Not implemented yet - could add timeout-based cleanup
}

// ===========================================
// Status queries
// ===========================================

bool DBMgrConnectionManager::is_connected(uint32_t dbmgr_index) const {
    if (dbmgr_index >= connections_.size() || !connections_[dbmgr_index]) {
        return false;
    }
    return connections_[dbmgr_index]->state() == DBMgrConnectionState::IDENTIFIED;
}

std::vector<std::pair<uint32_t, DBMgrConnectionState>> DBMgrConnectionManager::get_all_status() const {
    std::vector<std::pair<uint32_t, DBMgrConnectionState>> result;
    for (auto& conn_ptr : connections_) {
        if (conn_ptr) {
            result.emplace_back(conn_ptr->config_index(), conn_ptr->state());
        }
    }
    return result;
}

// ===========================================
// Helpers
// ===========================================

void DBMgrConnectionManager::send_to_dbmgr(DBMgrConnection* conn, uint32_t msg_id,
                                             std::string_view payload) {
    if (!conn || !conn->bev()) return;
    auto packed = MessageParser::pack(msg_id, payload);
    bufferevent_write(conn->bev(), packed.data(), packed.size());
}

std::optional<DBMgrConnection*> DBMgrConnectionManager::find_connection_by_bev(struct bufferevent* bev) {
    auto it = bev_to_index_.find(bev);
    if (it == bev_to_index_.end()) return std::nullopt;
    uint32_t idx = it->second;
    if (idx >= connections_.size() || !connections_[idx]) return std::nullopt;
    return connections_[idx].get();
}

void DBMgrConnectionManager::broadcast_message(uint32_t msg_id, std::string_view payload) {
    for (auto& conn_ptr : connections_) {
        if (!conn_ptr) continue;
        if (conn_ptr->state() != DBMgrConnectionState::IDENTIFIED) continue;

        send_to_dbmgr(conn_ptr.get(), msg_id, payload);
        SPDLOG_INFO("[DBMgrConnection]Broadcast msg_id={} to DBMgr index={}", msg_id, conn_ptr->config_index());
    }
}

}  // namespace farm
