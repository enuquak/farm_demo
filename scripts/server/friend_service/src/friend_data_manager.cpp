#include "friend_data_manager.h"
#include "friend_constants.h"
#include "redis_connection.h"
#include "log_macros.h"

namespace farm {

FriendDataManager::FriendDataManager(RedisConnection* redis)
    : redis_(redis)
{
}

// ===========================================
// persist_dirty
// ===========================================

int FriendDataManager::persist_dirty() {
    // Scan for dirty:friend:* keys
    auto dirty_keys = redis_->keys("dirty:friend:*");
    int count = 0;

    for (const auto& dirty_key : dirty_keys) {
        // Extract player_id from "dirty:friend:{pid}"
        size_t last_colon = dirty_key.rfind(':');
        if (last_colon == std::string::npos) continue;

        std::string pid_str = dirty_key.substr(last_colon + 1);
        uint64_t player_id = 0;
        try {
            player_id = std::stoull(pid_str);
        } catch (...) {
            continue;
        }

        // Collect friend data from Redis
        auto friend_ids = redis_->smembers(redis_key::friend_set(player_id));
        auto blocked_ids = redis_->smembers(redis_key::friend_blocked(player_id));
        auto requests = redis_->hgetall(redis_key::friend_requests(player_id));

        // Build a serialized representation for DBMgr storage.
        // Format: "f:id1,id2,...|b:id1,id2,...|r:sender1:ts1,sender2:ts2,..."
        std::string serialized = "f:";
        bool first = true;
        for (const auto& fid : friend_ids) {
            if (!first) serialized += ",";
            serialized += fid;
            first = false;
        }
        serialized += "|b:";
        first = true;
        for (const auto& bid : blocked_ids) {
            if (!first) serialized += ",";
            serialized += bid;
            first = false;
        }
        serialized += "|r:";
        first = true;
        for (const auto& [sender_id, timestamp] : requests) {
            if (!first) serialized += ",";
            serialized += sender_id + ":" + timestamp;
            first = false;
        }

        // Store to DBMgr key
        // NOTE: Actual DBMgr RPC is not implemented in this MVP.
        // This stores the serialized data in Redis under the dbmgr key
        // as a placeholder. In production, this would be an RPC call
        // to the DBMgr service.
        redis_->set(dbmgr_key::friend_data(player_id), serialized);

        // Clear dirty flag
        redis_->del(dirty_key);

        ++count;
    }

    if (count > 0) {
        SPDLOG_INFO("[FriendDataManager]Persisted {} dirty players", count);
    }

    return count;
}

// ===========================================
// load_from_dbmgr
// ===========================================

void FriendDataManager::load_from_dbmgr(uint64_t player_id) {
    // Load serialized friend data from DBMgr key
    // NOTE: In production, this would be an RPC call to DBMgr.
    // For MVP, we read from the Redis-stored placeholder.
    std::string serialized = redis_->get(dbmgr_key::friend_data(player_id));
    if (serialized.empty()) {
        SPDLOG_INFO("[FriendDataManager]No DBMgr data for player={}", player_id);
        return;
    }

    // Parse format: "f:id1,id2,...|b:id1,id2,...|r:sender1:ts1,sender2:ts2,..."
    // Split by '|'
    std::string friends_part, blocked_part, requests_part;

    size_t pipe1 = serialized.find('|');
    size_t pipe2 = (pipe1 != std::string::npos) ? serialized.find('|', pipe1 + 1) : std::string::npos;

    if (pipe1 != std::string::npos) {
        friends_part = serialized.substr(0, pipe1);
    }
    if (pipe1 != std::string::npos && pipe2 != std::string::npos) {
        blocked_part = serialized.substr(pipe1 + 1, pipe2 - pipe1 - 1);
    }
    if (pipe2 != std::string::npos) {
        requests_part = serialized.substr(pipe2 + 1);
    }

    // Clear existing data in Redis for this player
    redis_->del(redis_key::friend_set(player_id));
    redis_->del(redis_key::friend_blocked(player_id));
    redis_->del(redis_key::friend_requests(player_id));

    // Load friends (skip "f:" prefix)
    if (friends_part.size() > 2) {
        std::string ids = friends_part.substr(2);
        size_t pos = 0;
        while (pos < ids.size()) {
            size_t comma = ids.find(',', pos);
            std::string fid = ids.substr(pos, comma - pos);
            if (!fid.empty()) {
                redis_->sadd(redis_key::friend_set(player_id), fid);
            }
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    }

    // Load blocked (skip "b:" prefix)
    if (blocked_part.size() > 2) {
        std::string ids = blocked_part.substr(2);
        size_t pos = 0;
        while (pos < ids.size()) {
            size_t comma = ids.find(',', pos);
            std::string bid = ids.substr(pos, comma - pos);
            if (!bid.empty()) {
                redis_->sadd(redis_key::friend_blocked(player_id), bid);
            }
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    }

    // Load requests (skip "r:" prefix)
    if (requests_part.size() > 2) {
        std::string entries = requests_part.substr(2);
        size_t pos = 0;
        while (pos < entries.size()) {
            size_t comma = entries.find(',', pos);
            std::string entry = entries.substr(pos, comma - pos);
            size_t colon = entry.find(':');
            if (colon != std::string::npos) {
                std::string sender_id = entry.substr(0, colon);
                std::string timestamp = entry.substr(colon + 1);
                redis_->hset(redis_key::friend_requests(player_id), sender_id, timestamp);
            }
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    }

    SPDLOG_INFO("[FriendDataManager]Loaded friend data from DBMgr for player={}", player_id);
}

}  // namespace farm
