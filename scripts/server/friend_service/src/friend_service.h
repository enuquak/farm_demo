#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <cstdint>

#include <event2/event.h>

namespace farm {

class GameSession;
class FriendManager;
class ChatManager;
class GiftManager;
class VisitManager;
class RecommendManager;
class FriendDataManager;
class RedisConnection;

class FriendService {
public:
    FriendService(struct event_base* base);
    ~FriendService();

    bool start(const std::string& redis_uri, const std::string& game_host,
               int game_port, int listen_port);
    void stop();

    void handle_client_message(uint64_t player_id, uint32_t msg_id,
                               const uint8_t* payload, size_t len);

    GameSession* game_session() { return game_session_.get(); }

private:
    void register_handlers();

    struct event_base* base_;
    std::unique_ptr<RedisConnection> redis_;
    std::unique_ptr<GameSession> game_session_;

    using MsgHandler = std::function<void(uint64_t, const uint8_t*, size_t)>;
    std::unordered_map<uint32_t, MsgHandler> handlers_;
};

}  // namespace farm
