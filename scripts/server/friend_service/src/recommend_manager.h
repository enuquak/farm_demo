#pragma once

#include <cstdint>

namespace farm {

class RedisConnection;
class GameSession;
class FriendManager;

/**
 * RecommendManager provides friend-of-friend recommendations.
 *
 * For each friend, examines their friend set to find common
 * connections. Filters out existing friends, blocked, and self.
 * Returns top N sorted by common friend count.
 */
class RecommendManager {
public:
    RecommendManager(RedisConnection* redis, GameSession* session, FriendManager* friend_mgr);

    // Get friend recommendations
    void handle_recommend(uint64_t player_id, const uint8_t* data, size_t len);

private:
    RedisConnection* redis_;
    GameSession* session_;
    FriendManager* friend_mgr_;
};

}  // namespace farm
