#pragma once

#include <cstdint>

namespace farm {

class RedisConnection;
class GameSession;
class FriendManager;

/**
 * VisitManager handles friend farm visits.
 *
 * Tracks active visitors in a Redis set per farm owner.
 * Visit actions (water/weed/steal) require visitor to be
 * in the active visitors set.
 */
class VisitManager {
public:
    VisitManager(RedisConnection* redis, GameSession* session, FriendManager* friend_mgr);

    // Request to visit a friend's farm
    void handle_visit(uint64_t player_id, const uint8_t* data, size_t len);

    // Execute an action during a farm visit
    void handle_action(uint64_t player_id, const uint8_t* data, size_t len);

private:
    RedisConnection* redis_;
    GameSession* session_;
    FriendManager* friend_mgr_;
};

}  // namespace farm
