#include "visit_manager.h"
#include "friend_constants.h"
#include "friend_manager.h"
#include "redis_connection.h"
#include "game_session.h"
#include "message_ids.h"
#include "log_macros.h"

#include "friend.pb.h"

namespace farm {

VisitManager::VisitManager(RedisConnection* redis, GameSession* session,
                           FriendManager* friend_mgr)
    : redis_(redis)
    , session_(session)
    , friend_mgr_(friend_mgr)
{
}

// ===========================================
// handle_visit
// ===========================================

void VisitManager::handle_visit(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendVisitReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[VisitManager]Failed to parse FriendVisitReq from player={}", player_id);
        return;
    }

    uint64_t owner_id = req.owner_id();

    FriendVisitResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Cannot visit self
    if (owner_id == player_id) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::SELF_OPERATION));
        resp.set_scene_id("");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_VISIT_RESP, resp_data);
        return;
    }

    // Validate: must be friends
    if (!friend_mgr_->is_friend(player_id, owner_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::VISIT_NOT_AUTHORIZED));
        resp.set_scene_id("");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_VISIT_RESP, resp_data);
        return;
    }

    // Add player to active visitors set
    redis_->sadd(redis_key::farm_visit_active(owner_id), std::to_string(player_id));

    // Fill owner info for response
    std::string info_raw = redis_->get(redis_key::player_info(owner_id));
    if (!info_raw.empty()) {
        // Parse "role_name|level|scene_id"
        size_t pipe1 = info_raw.find('|');
        size_t pipe2 = info_raw.find('|', pipe1 + 1);
        if (pipe1 != std::string::npos) {
            resp.set_owner_name(info_raw.substr(0, pipe1));
        }
        if (pipe2 != std::string::npos) {
            resp.set_scene_id(info_raw.substr(pipe2 + 1));
        }
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_VISIT_RESP, resp_data);

    SPDLOG_INFO("[VisitManager]Player={} visiting farm of player={}", player_id, owner_id);
}

// ===========================================
// handle_action
// ===========================================

void VisitManager::handle_action(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendVisitActionReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[VisitManager]Failed to parse FriendVisitActionReq from player={}", player_id);
        return;
    }

    uint64_t owner_id = req.owner_id();
    int32_t action_type = req.action_type();
    int32_t target_x = req.target_x();
    int32_t target_y = req.target_y();

    FriendVisitActionResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Verify player is in active visitors set
    if (!redis_->sismember(redis_key::farm_visit_active(owner_id), std::to_string(player_id))) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::VISIT_ACTION_FAILED));
        resp.set_msg("Not an active visitor");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_VISIT_ACTION_RESP, resp_data);
        return;
    }

    // Validate action type
    if (action_type < static_cast<int32_t>(VisitAction::WATER) ||
        action_type > static_cast<int32_t>(VisitAction::STEAL)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::VISIT_ACTION_FAILED));
        resp.set_msg("Invalid action type");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_VISIT_ACTION_RESP, resp_data);
        return;
    }

    // NOTE: Actual farm action execution requires Game Server cooperation.
    // For MVP, we just acknowledge the action was accepted.

    SPDLOG_INFO("[VisitManager]Player={} action={} at ({},{}) on farm of player={}",
                player_id, action_type, target_x, target_y, owner_id);

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_VISIT_ACTION_RESP, resp_data);
}

}  // namespace farm
