#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace farm {

class RedisConnection;
class GameSession;
class FriendManager;

/**
 * ChatManager handles private chat between friends.
 *
 * Stores messages in Redis (offline queue + recent history).
 * Persists history via DBMgr when dirty flags are set.
 */
class ChatManager {
public:
    ChatManager(RedisConnection* redis, GameSession* session, FriendManager* friend_mgr);

    // Send private chat message
    void handle_send(uint64_t player_id, const uint8_t* data, size_t len);

    // Get chat history with a friend
    void handle_history(uint64_t player_id, const uint8_t* data, size_t len);

    // Deliver offline messages to a player on login
    void deliver_offline_messages(uint64_t player_id);

private:
    // Build a FriendChatNotify protobuf for a single message
    std::string build_chat_notify(uint64_t sender_id, int32_t msg_type,
                                  const std::string& content, uint64_t timestamp);

    // Store message to recent history list (capped)
    void store_to_history(uint64_t sender_id, uint64_t receiver_id,
                          int32_t msg_type, const std::string& content,
                          uint64_t timestamp);

    RedisConnection* redis_;
    GameSession* session_;
    FriendManager* friend_mgr_;
};

}  // namespace farm
