#pragma once

#include <cstdint>
#include <string>

namespace farm {

class RedisConnection;
class GameSession;

/**
 * FriendManager handles all core friend CRUD operations.
 *
 * Manages friend relationships, friend requests, blocking, and online status
 * notifications via Redis storage.
 *
 * Redis key patterns used:
 *   friend:{pid}:set      - Set of friend player_ids
 *   friend:{pid}:requests - Hash of sender_id -> timestamp
 *   friend:{pid}:blocked  - Set of blocked player_ids
 *   player:{pid}:online   - "1" or "0"
 *   player:{pid}:info     - "role_name|level|scene_id"
 *   dirty:friend:{pid}    - "1" if needs persistence
 */
class FriendManager {
public:
    FriendManager(RedisConnection* redis, GameSession* session);

    // === Message Handlers ===
    // Each handler: parses protobuf request from (data, len),
    // validates, executes Redis operations, builds response,
    // and sends via session->send_to_client().

    void handle_search(uint64_t player_id, const uint8_t* data, size_t len);
    void handle_add(uint64_t player_id, const uint8_t* data, size_t len);
    void handle_accept(uint64_t player_id, const uint8_t* data, size_t len);
    void handle_reject(uint64_t player_id, const uint8_t* data, size_t len);
    void handle_delete(uint64_t player_id, const uint8_t* data, size_t len);
    void handle_list(uint64_t player_id, const uint8_t* data, size_t len);
    void handle_block(uint64_t player_id, const uint8_t* data, size_t len);
    void handle_unblock(uint64_t player_id, const uint8_t* data, size_t len);

    // === Online Status Management ===

    // Set player online, optionally update player_info
    void set_player_online(uint64_t player_id, const std::string& role_name,
                           int32_t level, const std::string& scene_id);

    // Set player offline, notify friends
    void set_player_offline(uint64_t player_id);

    // === Relationship Checks ===

    bool is_friend(uint64_t player_id, uint64_t target_id);
    bool is_blocked(uint64_t player_id, uint64_t target_id);
    size_t get_friend_count(uint64_t player_id);

    // === Notifications ===

    // Notify all friends that player_id has come online
    void notify_friends_online(uint64_t player_id);

    // Notify all friends that player_id has gone offline
    void notify_friends_offline(uint64_t player_id);

private:
    // Helper: build FriendInfo protobuf from Redis-stored player_info
    // Returns empty FriendInfo if player_info not found.
    // Fields populated: player_id, role_name, level, online, scene_id
    void fill_friend_info(uint64_t player_id, void* friend_info_out);

    RedisConnection* redis_;
    GameSession* session_;
};

}  // namespace farm
