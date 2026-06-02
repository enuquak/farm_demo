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
    FriendManager* friend_manager() { return friend_manager_.get(); }
    ChatManager* chat_manager() { return chat_manager_.get(); }
    GiftManager* gift_manager() { return gift_manager_.get(); }
    VisitManager* visit_manager() { return visit_manager_.get(); }
    RecommendManager* recommend_manager() { return recommend_manager_.get(); }
    FriendDataManager* friend_data_manager() { return friend_data_manager_.get(); }

private:
    void register_handlers();

    struct event_base* base_;
    std::unique_ptr<RedisConnection> redis_;
    std::unique_ptr<GameSession> game_session_;
    std::unique_ptr<FriendManager> friend_manager_;
    std::unique_ptr<ChatManager> chat_manager_;
    std::unique_ptr<GiftManager> gift_manager_;
    std::unique_ptr<VisitManager> visit_manager_;
    std::unique_ptr<RecommendManager> recommend_manager_;
    std::unique_ptr<FriendDataManager> friend_data_manager_;

    using MsgHandler = std::function<void(uint64_t, const uint8_t*, size_t)>;
    std::unordered_map<uint32_t, MsgHandler> handlers_;
};

}  // namespace farm
