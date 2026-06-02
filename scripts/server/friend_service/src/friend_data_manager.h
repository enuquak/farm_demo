#pragma once

#include <cstdint>
#include <string>

namespace farm {

class RedisConnection;

/**
 * FriendDataManager handles persistence of friend data
 * between Redis (hot cache) and DBMgr (persistent storage).
 *
 * Players are marked dirty in Redis when their friend data changes.
 * persist_dirty() scans dirty keys and pushes data to DBMgr.
 * load_from_dbmgr() loads friend data from DBMgr into Redis on login.
 */
class FriendDataManager {
public:
    explicit FriendDataManager(RedisConnection* redis);

    // Scan dirty:friend:* keys and persist each to DBMgr.
    // Returns count of players persisted.
    int persist_dirty();

    // Load friend data from DBMgr into Redis for a player.
    // Called on login to warm the cache.
    void load_from_dbmgr(uint64_t player_id);

private:
    RedisConnection* redis_;
};

}  // namespace farm
