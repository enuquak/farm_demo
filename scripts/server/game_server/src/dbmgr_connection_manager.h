#pragma once

#include "dbmgr_connection.h"

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>
#include <optional>
#include <tuple>

namespace farm {

// Callback for async PlayerDataResp: (code, value_data, value_len)
using PlayerDataCallback = std::function<void(int32_t code, const uint8_t* value_data, size_t value_len)>;

// Callback for async AccountDataResp: (code, roles)
using AccountDataCallback = std::function<void(int32_t code, const std::vector<std::tuple<uint32_t, uint64_t, std::string>>& roles)>;

// Callback for async AccountSetResp: (code, msg)
using AccountSetCallback = std::function<void(int32_t code, const std::string& msg)>;

// Callback for async AllocPlayerIdResp: (code, start_id, count)
using AllocPlayerIdCallback = std::function<void(int32_t code, uint64_t start_id, uint32_t count)>;

// Config entry for a single DBMgr instance
struct DBMgrConfig {
    std::string host;
    uint16_t port;
};

// Pending async request tracking
struct PendingRequest {
    uint64_t request_id;
    uint32_t dbmgr_index;       // Which DBMgr this request was sent to
    PlayerDataCallback callback;
    time_t send_time;           // For timeout detection
};

// Pending account request tracking
struct PendingAccountRequest {
    uint64_t request_id;
    uint32_t dbmgr_index;
    AccountDataCallback data_callback;
    AccountSetCallback set_callback;
    time_t send_time;
};

// Pending ID allocation request tracking
struct PendingAllocIdRequest {
    uint64_t request_id;
    uint32_t dbmgr_index;
    AllocPlayerIdCallback callback;
    time_t send_time;
};

class DBMgrConnectionManager {
public:
    DBMgrConnectionManager();
    ~DBMgrConnectionManager();

    /**
     * @brief Initialize and connect to all configured DBMgrs.
     * @param base  The event_base owned by GameServer (shared)
     * @param configs  List of DBMgr configs (host, port)
     * @return true on success
     */
    bool init(struct event_base* base, const std::vector<DBMgrConfig>& configs);

    // Shutdown: close all connections, cancel all pending requests
    void shutdown();

    /**
     * @brief Send a PlayerDataReq to the appropriate DBMgr.
     * @param player_id  Used to route: dbmgr_index = player_id % dbmgr_count
     * @param op         Operation type (GET/SET/DEL/GET_ALL/SET_ALL)
     * @param key        Data key
     * @param value      Data value (for SET/SET_ALL)
     * @param callback   Called when response arrives or on error
     * @return request_id (>0) on success, 0 on failure
     */
    uint64_t send_player_data_req(uint64_t player_id, int32_t op,
                                   const std::string& key, const std::string& value,
                                   PlayerDataCallback callback);

    /**
     * @brief Send an AccountDataReq to the appropriate DBMgr.
     * @param account_id  Used to route: dbmgr_index = hash(account_id) % dbmgr_count
     * @param callback    Called when response arrives or on error
     * @return request_id (>0) on success, 0 on failure
     */
    uint64_t send_account_data_req(const std::string& account_id,
                                    AccountDataCallback callback);

    /**
     * @brief Send an AccountSetReq to the appropriate DBMgr.
     * @param account_id  Used to route: dbmgr_index = hash(account_id) % dbmgr_count
     * @param server_id   Server ID for the new role
     * @param player_id   Player ID for the new role
     * @param role_name   Role name for the new role
     * @param callback    Called when response arrives or on error
     * @return request_id (>0) on success, 0 on failure
     */
    uint64_t send_account_set_req(const std::string& account_id,
                                   uint32_t server_id, uint64_t player_id,
                                   const std::string& role_name,
                                   AccountSetCallback callback);

    /**
     * @brief Send an AllocPlayerIdReq to DBMgr.
     * @param count     Number of IDs to allocate
     * @param callback  Called when response arrives
     * @return request_id (>0) on success, 0 on failure
     */
    uint64_t send_alloc_player_id_req(uint32_t count, AllocPlayerIdCallback callback);

    // Status queries
    bool is_connected(uint32_t dbmgr_index) const;
    size_t dbmgr_count() const { return connections_.size(); }
    std::vector<std::pair<uint32_t, DBMgrConnectionState>> get_all_status() const;

    // Broadcast message to all connected DBMgrs
    void broadcast_message(uint32_t msg_id, std::string_view payload);

private:
    // libevent callbacks (static -> this pointer)
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_heartbeat_timer(evutil_socket_t fd, short events, void* ctx);
    static void on_reconnect_timer(evutil_socket_t fd, short events, void* ctx);

    // Connection lifecycle
    void connect_to_dbmgr(uint32_t index, const std::string& host, uint16_t port);
    void handle_connected(DBMgrConnection* conn);
    void handle_read(DBMgrConnection* conn);
    void handle_disconnect(DBMgrConnection* conn);

    // Message routing (from DBMgr)
    void route_message(DBMgrConnection* conn, uint32_t msg_id,
                       const std::vector<uint8_t>& payload);
    void handle_dbmgr_identify(DBMgrConnection* conn,
                                const std::vector<uint8_t>& payload);
    void handle_dbmgr_heartbeat(DBMgrConnection* conn,
                                 const std::vector<uint8_t>& payload);
    void handle_player_data_resp(DBMgrConnection* conn,
                                  const std::vector<uint8_t>& payload);
    void handle_account_data_resp(DBMgrConnection* conn,
                                   const std::vector<uint8_t>& payload);
    void handle_account_set_resp(DBMgrConnection* conn,
                                  const std::vector<uint8_t>& payload);
    void handle_alloc_player_id_resp(DBMgrConnection* conn,
                                     const std::vector<uint8_t>& payload);

    // Heartbeat and reconnect
    void check_heartbeat();
    void try_reconnect();

    // Request management
    uint64_t generate_request_id();
    void fail_pending_requests_for(uint32_t dbmgr_index);
    void cleanup_stale_requests();

    // Account ID hash
    uint32_t hash_account_id(std::string_view account_id) const;

    // Send helper
    void send_to_dbmgr(DBMgrConnection* conn, uint32_t msg_id, std::string_view payload);

    // Lookup connection by bev pointer (for callbacks)
    std::optional<DBMgrConnection*> find_connection_by_bev(struct bufferevent* bev);

    struct event_base* base_;
    struct event* heartbeat_timer_;
    struct event* reconnect_timer_;
    bool running_;

    // Connections indexed by config_index (0, 1, 2, ...)
    std::vector<std::unique_ptr<DBMgrConnection>> connections_;

    // Async request tracking: request_id -> PendingRequest
    std::unordered_map<uint64_t, PendingRequest> pending_requests_;
    std::atomic<uint64_t> next_request_id_;

    // Async account request tracking: request_id -> PendingAccountRequest
    std::unordered_map<uint64_t, PendingAccountRequest> pending_account_requests_;

    // Async ID allocation request tracking: request_id -> PendingAllocIdRequest
    std::unordered_map<uint64_t, PendingAllocIdRequest> pending_alloc_id_requests_;

    // Reverse lookup: bev -> connection index (for fast callback dispatch)
    std::unordered_map<struct bufferevent*, uint32_t> bev_to_index_;
};

}  // namespace farm
