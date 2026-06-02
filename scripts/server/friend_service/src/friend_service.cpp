#include "friend_service.h"
#include "friend_manager.h"
#include "game_session.h"
#include "redis_connection.h"
#include "message_ids.h"
#include "log_macros.h"

#include "friend.pb.h"

namespace farm {

FriendService::FriendService(struct event_base* base)
    : base_(base)
{
}

FriendService::~FriendService() {
    stop();
}

bool FriendService::start(const std::string& redis_uri, const std::string& game_host,
                           int game_port, int listen_port) {
    // Connect to Redis
    redis_ = std::make_unique<RedisConnection>();
    if (!redis_uri.empty()) {
        if (!redis_->connect(redis_uri)) {
            SPDLOG_ERROR("[FriendService]Failed to connect to Redis: {}", redis_uri);
            return false;
        }
        SPDLOG_INFO("[FriendService]Connected to Redis");
    }

    // Create GameSession and connect to Game Server
    game_session_ = std::make_unique<GameSession>(this, base_);
    if (!game_session_->connect(game_host, game_port)) {
        SPDLOG_ERROR("[FriendService]Failed to connect to Game Server at {}:{}",
                     game_host, game_port);
        return false;
    }

    // Create FriendManager
    friend_manager_ = std::make_unique<FriendManager>(redis_.get(), game_session_.get());

    // Register message handlers
    register_handlers();

    SPDLOG_INFO("[FriendService]Started (game={}:{}, redis={})",
                game_host, game_port, redis_uri);
    return true;
}

void FriendService::stop() {
    friend_manager_.reset();
    if (game_session_) {
        game_session_->disconnect();
        game_session_.reset();
    }
    if (redis_) {
        redis_->disconnect();
        redis_.reset();
    }
    handlers_.clear();
    SPDLOG_INFO("[FriendService]Stopped");
}

void FriendService::handle_client_message(uint64_t player_id, uint32_t msg_id,
                                            const uint8_t* payload, size_t len) {
    auto it = handlers_.find(msg_id);
    if (it != handlers_.end()) {
        it->second(player_id, payload, len);
    } else {
        SPDLOG_INFO("[FriendService]Unhandled client msg_id={} from player={}",
                     msg_id, player_id);
    }
}

void FriendService::register_handlers() {
    // Friend relationship messages (8001-8015)
    handlers_[MSG_ID_FRIEND_SEARCH_REQ] = [this](uint64_t player_id, const uint8_t* data, size_t len) {
        friend_manager_->handle_search(player_id, data, len);
    };
    handlers_[MSG_ID_FRIEND_ADD_REQ] = [this](uint64_t player_id, const uint8_t* data, size_t len) {
        friend_manager_->handle_add(player_id, data, len);
    };
    handlers_[MSG_ID_FRIEND_ACCEPT_REQ] = [this](uint64_t player_id, const uint8_t* data, size_t len) {
        friend_manager_->handle_accept(player_id, data, len);
    };
    handlers_[MSG_ID_FRIEND_REJECT_REQ] = [this](uint64_t player_id, const uint8_t* data, size_t len) {
        friend_manager_->handle_reject(player_id, data, len);
    };
    handlers_[MSG_ID_FRIEND_DELETE_REQ] = [this](uint64_t player_id, const uint8_t* data, size_t len) {
        friend_manager_->handle_delete(player_id, data, len);
    };
    handlers_[MSG_ID_FRIEND_LIST_REQ] = [this](uint64_t player_id, const uint8_t* data, size_t len) {
        friend_manager_->handle_list(player_id, data, len);
    };

    // Friend chat messages (8020-8024)
    handlers_[MSG_ID_FRIEND_CHAT_REQ] = [](uint64_t player_id, const uint8_t* data, size_t len) {
        SPDLOG_INFO("[FriendService]FriendChatReq from player={}", player_id);
        // TODO: implement ChatManager::handle_send
    };
    handlers_[MSG_ID_FRIEND_CHAT_HISTORY_REQ] = [](uint64_t player_id, const uint8_t* data, size_t len) {
        SPDLOG_INFO("[FriendService]FriendChatHistoryReq from player={}", player_id);
        // TODO: implement ChatManager::handle_history
    };

    // Friend gift messages (8030-8032)
    handlers_[MSG_ID_FRIEND_GIFT_REQ] = [](uint64_t player_id, const uint8_t* data, size_t len) {
        SPDLOG_INFO("[FriendService]FriendGiftReq from player={}", player_id);
        // TODO: implement GiftManager::handle_send
    };

    // Friend visit messages (8040-8043)
    handlers_[MSG_ID_FRIEND_VISIT_REQ] = [](uint64_t player_id, const uint8_t* data, size_t len) {
        SPDLOG_INFO("[FriendService]FriendVisitReq from player={}", player_id);
        // TODO: implement VisitManager::handle_visit
    };
    handlers_[MSG_ID_FRIEND_VISIT_ACTION_REQ] = [](uint64_t player_id, const uint8_t* data, size_t len) {
        SPDLOG_INFO("[FriendService]FriendVisitActionReq from player={}", player_id);
        // TODO: implement VisitManager::handle_action
    };

    // Friend recommend messages (8050-8051)
    handlers_[MSG_ID_FRIEND_RECOMMEND_REQ] = [](uint64_t player_id, const uint8_t* data, size_t len) {
        SPDLOG_INFO("[FriendService]FriendRecommendReq from player={}", player_id);
        // TODO: implement RecommendManager::handle_recommend
    };

    // Friend block messages (8060-8063)
    handlers_[MSG_ID_FRIEND_BLOCK_REQ] = [this](uint64_t player_id, const uint8_t* data, size_t len) {
        friend_manager_->handle_block(player_id, data, len);
    };
    handlers_[MSG_ID_FRIEND_UNBLOCK_REQ] = [this](uint64_t player_id, const uint8_t* data, size_t len) {
        friend_manager_->handle_unblock(player_id, data, len);
    };

    SPDLOG_INFO("[FriendService]Registered {} message handlers", handlers_.size());
}

}  // namespace farm
