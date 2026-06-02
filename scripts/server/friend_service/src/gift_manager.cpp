#include "gift_manager.h"
#include "friend_constants.h"
#include "friend_manager.h"
#include "redis_connection.h"
#include "game_session.h"
#include "message_ids.h"
#include "log_macros.h"

#include "friend.pb.h"

namespace farm {

GiftManager::GiftManager(RedisConnection* redis, GameSession* session,
                         FriendManager* friend_mgr)
    : redis_(redis)
    , session_(session)
    , friend_mgr_(friend_mgr)
{
}

// ===========================================
// handle_send
// ===========================================

void GiftManager::handle_send(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendGiftReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[GiftManager]Failed to parse FriendGiftReq from player={}", player_id);
        return;
    }

    uint64_t receiver_id = req.receiver_id();
    int32_t item_id = req.item_id();
    int32_t count = req.count();

    FriendGiftResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Validate count > 0
    if (count <= 0) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::GIFT_COUNT_INVALID));
        resp.set_msg("Gift count must be positive");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_GIFT_RESP, resp_data);
        return;
    }

    // Validate: must be friends
    if (!friend_mgr_->is_friend(player_id, receiver_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::NOT_FRIENDS));
        resp.set_msg("Target is not your friend");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_GIFT_RESP, resp_data);
        return;
    }

    // Build notify for receiver
    FriendGiftNotify notify;
    notify.set_sender_id(player_id);
    notify.set_item_id(item_id);
    notify.set_count(count);

    // Fill sender name from player_info
    std::string info_raw = redis_->get(redis_key::player_info(player_id));
    if (!info_raw.empty()) {
        size_t pipe1 = info_raw.find('|');
        if (pipe1 != std::string::npos) {
            notify.set_sender_name(info_raw.substr(0, pipe1));
        }
    }

    std::string notify_data;
    notify.SerializeToString(&notify_data);

    // Notify receiver (if online)
    std::string receiver_online = redis_->get(redis_key::player_online(receiver_id));
    if (receiver_online == "1") {
        session_->send_to_client(receiver_id, MSG_ID_FRIEND_GIFT_NOTIFY, notify_data);
    }
    // NOTE: For MVP, offline gift delivery is not implemented.
    // Actual item transfer requires Game Server cooperation.

    // Send response to sender
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_GIFT_RESP, resp_data);

    SPDLOG_INFO("[GiftManager]Player={} sent gift (item={}, count={}) to player={}",
                player_id, item_id, count, receiver_id);
}

}  // namespace farm
