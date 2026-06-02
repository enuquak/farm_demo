#pragma once

#include <cstdint>

namespace farm {

class RedisConnection;
class GameSession;
class FriendManager;

/**
 * GiftManager handles sending gifts between friends.
 *
 * For MVP, only notification is sent. Actual item transfer
 * requires Game Server cooperation.
 */
class GiftManager {
public:
    GiftManager(RedisConnection* redis, GameSession* session, FriendManager* friend_mgr);

    // Send a gift to a friend
    void handle_send(uint64_t player_id, const uint8_t* data, size_t len);

private:
    RedisConnection* redis_;
    GameSession* session_;
    FriendManager* friend_mgr_;
};

}  // namespace farm
