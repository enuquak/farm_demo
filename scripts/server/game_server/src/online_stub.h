#pragma once

#include <functional>
#include <string>
#include <cstdint>

namespace farm {

class RedisConnection;

class OnlineStub {
public:
    explicit OnlineStub(RedisConnection* redis_conn);
    ~OnlineStub() = default;

    // Query which server a player is on (0 = offline)
    using OnlineQueryCallback = std::function<void(uint64_t player_id, uint32_t server_id)>;
    void query_player_server(uint64_t player_id, OnlineQueryCallback callback);

private:
    RedisConnection* redis_conn_;
};

}  // namespace farm
