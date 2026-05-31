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
