#include "friend_manager.h"
#include "friend_constants.h"
#include "redis_connection.h"
#include "game_session.h"
#include "message_ids.h"
#include "log_macros.h"

#include "friend.pb.h"

#include <ctime>
#include <sstream>
#include <algorithm>

namespace farm {

FriendManager::FriendManager(RedisConnection* redis, GameSession* session)
    : redis_(redis)
    , session_(session)
{
}

// ===========================================
// Helper: parse player_info string "role_name|level|scene_id"
// ===========================================

static bool parse_player_info(const std::string& info,
                              std::string& role_name,
                              int32_t& level,
                              std::string& scene_id) {
    if (info.empty()) return false;

    std::istringstream ss(info);
    std::string level_str;

    if (!std::getline(ss, role_name, '|')) return false;
    if (!std::getline(ss, level_str, '|')) return false;
    if (!std::getline(ss, scene_id, '|')) {
        // scene_id may be missing for older entries
        scene_id = "";
    }

    try {
        level = std::stoi(level_str);
    } catch (...) {
        level = 1;
    }

    return true;
}

void FriendManager::fill_friend_info(uint64_t player_id, void* friend_info_out) {
    auto* info = static_cast<FriendInfo*>(friend_info_out);
    info->set_player_id(player_id);

    std::string raw = redis_->get(redis_key::player_info(player_id));
    if (!raw.empty()) {
        std::string role_name, scene_id;
        int32_t level = 1;
        if (parse_player_info(raw, role_name, level, scene_id)) {
            info->set_role_name(role_name);
            info->set_level(level);
            info->set_scene_id(scene_id);
        }
    }

    std::string online = redis_->get(redis_key::player_online(player_id));
    info->set_online(online == "1");
}

// ===========================================
// handle_search
// ===========================================

void FriendManager::handle_search(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendSearchReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[FriendManager]Failed to parse FriendSearchReq from player={}", player_id);
        return;
    }

    FriendSearchResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    const std::string& keyword = req.keyword();
    if (keyword.empty()) {
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_SEARCH_RESP, resp_data);
        return;
    }

    // Try to parse keyword as player_id for direct lookup
    try {
        uint64_t target_id = std::stoull(keyword);
        std::string info_raw = redis_->get(redis_key::player_info(target_id));
        if (!info_raw.empty()) {
            auto* result = resp.add_results();
            fill_friend_info(target_id, result);
        }
    } catch (...) {
        // Not a numeric ID, search by role_name prefix
        // Scan player:*:info keys and match by name
        auto keys = redis_->keys("player:*:info");
        size_t count = 0;
        for (const auto& key : keys) {
            if (count >= MAX_SEARCH_RESULTS) break;

            // Extract player_id from key "player:{pid}:info"
            size_t colon1 = key.find(':');
            size_t colon2 = key.find(':', colon1 + 1);
            if (colon1 == std::string::npos || colon2 == std::string::npos) continue;

            std::string pid_str = key.substr(colon1 + 1, colon2 - colon1 - 1);
            uint64_t pid = 0;
            try {
                pid = std::stoull(pid_str);
            } catch (...) {
                continue;
            }

            // Skip self
            if (pid == player_id) continue;

            std::string info_raw = redis_->get(key);
            std::string role_name, scene_id;
            int32_t level = 1;
            if (!parse_player_info(info_raw, role_name, level, scene_id)) continue;

            // Case-insensitive prefix match
            std::string lower_name = role_name;
            std::string lower_kw = keyword;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            std::transform(lower_kw.begin(), lower_kw.end(), lower_kw.begin(), ::tolower);

            if (lower_name.find(lower_kw) != std::string::npos) {
                auto* result = resp.add_results();
                result->set_player_id(pid);
                result->set_role_name(role_name);
                result->set_level(level);
                result->set_scene_id(scene_id);
                std::string online = redis_->get(redis_key::player_online(pid));
                result->set_online(online == "1");
                ++count;
            }
        }
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_SEARCH_RESP, resp_data);
    SPDLOG_INFO("[FriendManager]Player={} searched '{}', {} results",
                player_id, keyword, resp.results_size());
}

// ===========================================
// handle_add
// ===========================================

void FriendManager::handle_add(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendAddReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[FriendManager]Failed to parse FriendAddReq from player={}", player_id);
        return;
    }

    uint64_t target_id = req.target_id();

    FriendAddResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Cannot add self
    if (target_id == player_id) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::SELF_OPERATION));
        resp.set_msg("Cannot add yourself as friend");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_ADD_RESP, resp_data);
        return;
    }

    // Check target exists
    std::string target_info = redis_->get(redis_key::player_info(target_id));
    if (target_info.empty()) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::PLAYER_NOT_FOUND));
        resp.set_msg("Player not found");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_ADD_RESP, resp_data);
        return;
    }

    // Already friends?
    if (is_friend(player_id, target_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::ALREADY_FRIENDS));
        resp.set_msg("Already friends");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_ADD_RESP, resp_data);
        return;
    }

    // Friend list full?
    if (get_friend_count(player_id) >= MAX_FRIENDS) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::FRIEND_LIST_FULL));
        resp.set_msg("Your friend list is full");
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_ADD_RESP, resp_data);
        return;
    }

    // Store the request: target's requests hash, field=sender_id, value=timestamp
    std::string ts = std::to_string(static_cast<uint64_t>(std::time(nullptr)));
    redis_->hset(redis_key::friend_requests(target_id),
                 std::to_string(player_id), ts);
    redis_->set(redis_key::dirty(target_id), "1");

    // Send response to sender
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_ADD_RESP, resp_data);

    // Notify target if online
    std::string target_online = redis_->get(redis_key::player_online(target_id));
    if (target_online == "1") {
        FriendAddNotify notify;
        auto* request = notify.mutable_request();
        request->set_sender_id(player_id);
        request->set_timestamp(std::stoull(ts));

        // Fill sender info
        std::string sender_info_raw = redis_->get(redis_key::player_info(player_id));
        if (!sender_info_raw.empty()) {
            std::string sender_name, sender_scene;
            int32_t sender_level = 1;
            if (parse_player_info(sender_info_raw, sender_name, sender_level, sender_scene)) {
                request->set_sender_name(sender_name);
                request->set_sender_level(sender_level);
            }
        }

        std::string notify_data;
        notify.SerializeToString(&notify_data);
        session_->send_to_client(target_id, MSG_ID_FRIEND_ADD_NOTIFY, notify_data);
    }

    SPDLOG_INFO("[FriendManager]Player={} sent friend request to player={}",
                player_id, target_id);
}

// ===========================================
// handle_accept
// ===========================================

void FriendManager::handle_accept(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendAcceptReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[FriendManager]Failed to parse FriendAcceptReq from player={}", player_id);
        return;
    }

    uint64_t sender_id = req.sender_id();

    FriendAcceptResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Cannot accept self
    if (sender_id == player_id) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::SELF_OPERATION));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_ACCEPT_RESP, resp_data);
        return;
    }

    // Check request exists
    std::string request_ts = redis_->hget(redis_key::friend_requests(player_id),
                                           std::to_string(sender_id));
    if (request_ts.empty()) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::REQUEST_NOT_FOUND));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_ACCEPT_RESP, resp_data);
        return;
    }

    // Get sender info
    std::string sender_info_raw = redis_->get(redis_key::player_info(sender_id));
    if (sender_info_raw.empty()) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::PLAYER_NOT_FOUND));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_ACCEPT_RESP, resp_data);
        return;
    }

    // Friend list full?
    if (get_friend_count(player_id) >= MAX_FRIENDS) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::FRIEND_LIST_FULL));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_ACCEPT_RESP, resp_data);
        return;
    }

    // Remove request
    redis_->hdel(redis_key::friend_requests(player_id), std::to_string(sender_id));

    // If target also sent a request to sender, remove that too
    redis_->hdel(redis_key::friend_requests(sender_id), std::to_string(player_id));

    // Add bidirectional friendship
    redis_->sadd(redis_key::friend_set(player_id), std::to_string(sender_id));
    redis_->sadd(redis_key::friend_set(sender_id), std::to_string(player_id));

    // Mark both dirty
    redis_->set(redis_key::dirty(player_id), "1");
    redis_->set(redis_key::dirty(sender_id), "1");

    // Build new_friend info for response
    auto* new_friend = resp.mutable_new_friend();
    fill_friend_info(sender_id, new_friend);

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_ACCEPT_RESP, resp_data);

    // Notify sender if online
    std::string sender_online = redis_->get(redis_key::player_online(sender_id));
    if (sender_online == "1") {
        FriendAcceptResp notify;
        notify.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));
        auto* notify_friend = notify.mutable_new_friend();
        fill_friend_info(player_id, notify_friend);

        std::string notify_data;
        notify.SerializeToString(&notify_data);
        session_->send_to_client(sender_id, MSG_ID_FRIEND_ACCEPT_RESP, notify_data);
    }

    SPDLOG_INFO("[FriendManager]Player={} accepted friend request from player={}",
                player_id, sender_id);
}

// ===========================================
// handle_reject
// ===========================================

void FriendManager::handle_reject(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendRejectReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[FriendManager]Failed to parse FriendRejectReq from player={}", player_id);
        return;
    }

    uint64_t sender_id = req.sender_id();

    FriendRejectResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Cannot reject self
    if (sender_id == player_id) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::SELF_OPERATION));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_REJECT_RESP, resp_data);
        return;
    }

    // Check request exists
    std::string request_ts = redis_->hget(redis_key::friend_requests(player_id),
                                           std::to_string(sender_id));
    if (request_ts.empty()) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::REQUEST_NOT_FOUND));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_REJECT_RESP, resp_data);
        return;
    }

    // Remove request
    redis_->hdel(redis_key::friend_requests(player_id), std::to_string(sender_id));
    redis_->set(redis_key::dirty(player_id), "1");

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_REJECT_RESP, resp_data);

    SPDLOG_INFO("[FriendManager]Player={} rejected friend request from player={}",
                player_id, sender_id);
}

// ===========================================
// handle_delete
// ===========================================

void FriendManager::handle_delete(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendDeleteReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[FriendManager]Failed to parse FriendDeleteReq from player={}", player_id);
        return;
    }

    uint64_t target_id = req.target_id();

    FriendDeleteResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Cannot delete self
    if (target_id == player_id) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::SELF_OPERATION));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_DELETE_RESP, resp_data);
        return;
    }

    // Check target exists
    std::string target_info = redis_->get(redis_key::player_info(target_id));
    if (target_info.empty()) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::PLAYER_NOT_FOUND));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_DELETE_RESP, resp_data);
        return;
    }

    // Not friends?
    if (!is_friend(player_id, target_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::NOT_FRIENDS));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_DELETE_RESP, resp_data);
        return;
    }

    // Remove bidirectional friendship
    redis_->srem(redis_key::friend_set(player_id), std::to_string(target_id));
    redis_->srem(redis_key::friend_set(target_id), std::to_string(player_id));

    // Mark both dirty
    redis_->set(redis_key::dirty(player_id), "1");
    redis_->set(redis_key::dirty(target_id), "1");

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_DELETE_RESP, resp_data);

    SPDLOG_INFO("[FriendManager]Player={} deleted friend player={}",
                player_id, target_id);
}

// ===========================================
// handle_list
// ===========================================

void FriendManager::handle_list(uint64_t player_id, const uint8_t* data, size_t len) {
    // FriendListReq has no fields, parse is optional but do it for consistency
    FriendListReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[FriendManager]Failed to parse FriendListReq from player={}", player_id);
        return;
    }

    FriendListResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Get all friends
    auto friend_ids = redis_->smembers(redis_key::friend_set(player_id));
    for (const auto& fid_str : friend_ids) {
        uint64_t fid = 0;
        try {
            fid = std::stoull(fid_str);
        } catch (...) {
            continue;
        }
        auto* fi = resp.add_friends();
        fill_friend_info(fid, fi);
    }

    // Get pending requests
    auto requests = redis_->hgetall(redis_key::friend_requests(player_id));
    for (const auto& [sender_id_str, timestamp_str] : requests) {
        uint64_t sender_id = 0;
        try {
            sender_id = std::stoull(sender_id_str);
        } catch (...) {
            continue;
        }

        auto* ri = resp.add_pending_requests();
        ri->set_sender_id(sender_id);
        ri->set_timestamp(std::stoull(timestamp_str));

        // Fill sender info
        std::string sender_info_raw = redis_->get(redis_key::player_info(sender_id));
        if (!sender_info_raw.empty()) {
            std::string sender_name, sender_scene;
            int32_t sender_level = 1;
            if (parse_player_info(sender_info_raw, sender_name, sender_level, sender_scene)) {
                ri->set_sender_name(sender_name);
                ri->set_sender_level(sender_level);
            }
        }
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_LIST_RESP, resp_data);

    SPDLOG_INFO("[FriendManager]Player={} listed {} friends, {} pending requests",
                player_id, resp.friends_size(), resp.pending_requests_size());
}

// ===========================================
// handle_block
// ===========================================

void FriendManager::handle_block(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendBlockReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[FriendManager]Failed to parse FriendBlockReq from player={}", player_id);
        return;
    }

    uint64_t target_id = req.target_id();

    FriendBlockResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Cannot block self
    if (target_id == player_id) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::SELF_OPERATION));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_BLOCK_RESP, resp_data);
        return;
    }

    // Check target exists
    std::string target_info = redis_->get(redis_key::player_info(target_id));
    if (target_info.empty()) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::PLAYER_NOT_FOUND));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_BLOCK_RESP, resp_data);
        return;
    }

    // Add to blocked set
    redis_->sadd(redis_key::friend_blocked(player_id), std::to_string(target_id));

    // If they were friends, remove friendship
    if (is_friend(player_id, target_id)) {
        redis_->srem(redis_key::friend_set(player_id), std::to_string(target_id));
        redis_->srem(redis_key::friend_set(target_id), std::to_string(player_id));
        redis_->set(redis_key::dirty(target_id), "1");
    }

    // Remove any pending request from target
    redis_->hdel(redis_key::friend_requests(player_id), std::to_string(target_id));

    redis_->set(redis_key::dirty(player_id), "1");

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_BLOCK_RESP, resp_data);

    SPDLOG_INFO("[FriendManager]Player={} blocked player={}", player_id, target_id);
}

// ===========================================
// handle_unblock
// ===========================================

void FriendManager::handle_unblock(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendUnblockReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[FriendManager]Failed to parse FriendUnblockReq from player={}", player_id);
        return;
    }

    uint64_t target_id = req.target_id();

    FriendUnblockResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Cannot unblock self
    if (target_id == player_id) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::SELF_OPERATION));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_UNBLOCK_RESP, resp_data);
        return;
    }

    // Remove from blocked set
    redis_->srem(redis_key::friend_blocked(player_id), std::to_string(target_id));
    redis_->set(redis_key::dirty(player_id), "1");

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_UNBLOCK_RESP, resp_data);

    SPDLOG_INFO("[FriendManager]Player={} unblocked player={}", player_id, target_id);
}

// ===========================================
// Online Status Management
// ===========================================

void FriendManager::set_player_online(uint64_t player_id, const std::string& role_name,
                                       int32_t level, const std::string& scene_id) {
    redis_->set(redis_key::player_online(player_id), "1");

    // Update player_info if provided
    if (!role_name.empty()) {
        std::string info = role_name + "|" + std::to_string(level) + "|" + scene_id;
        redis_->set(redis_key::player_info(player_id), info);
    }

    notify_friends_online(player_id);

    SPDLOG_INFO("[FriendManager]Player={} online (name={}, level={})",
                player_id, role_name, level);
}

void FriendManager::set_player_offline(uint64_t player_id) {
    redis_->set(redis_key::player_online(player_id), "0");

    notify_friends_offline(player_id);

    SPDLOG_INFO("[FriendManager]Player={} offline", player_id);
}

// ===========================================
// Relationship Checks
// ===========================================

bool FriendManager::is_friend(uint64_t player_id, uint64_t target_id) {
    return redis_->sismember(redis_key::friend_set(player_id), std::to_string(target_id));
}

bool FriendManager::is_blocked(uint64_t player_id, uint64_t target_id) {
    return redis_->sismember(redis_key::friend_blocked(player_id), std::to_string(target_id));
}

size_t FriendManager::get_friend_count(uint64_t player_id) {
    return redis_->scard(redis_key::friend_set(player_id));
}

// ===========================================
// Notifications
// ===========================================

void FriendManager::notify_friends_online(uint64_t player_id) {
    auto friend_ids = redis_->smembers(redis_key::friend_set(player_id));
    if (friend_ids.empty()) return;

    // Build notification
    FriendOnlineNotify notify;
    notify.set_player_id(player_id);

    std::string info_raw = redis_->get(redis_key::player_info(player_id));
    if (!info_raw.empty()) {
        std::string role_name, scene_id;
        int32_t level = 1;
        if (parse_player_info(info_raw, role_name, level, scene_id)) {
            notify.set_role_name(role_name);
            notify.set_scene_id(scene_id);
        }
    }

    std::string notify_data;
    notify.SerializeToString(&notify_data);

    for (const auto& fid_str : friend_ids) {
        uint64_t fid = 0;
        try {
            fid = std::stoull(fid_str);
        } catch (...) {
            continue;
        }

        // Only notify online friends
        std::string friend_online = redis_->get(redis_key::player_online(fid));
        if (friend_online == "1") {
            session_->send_to_client(fid, MSG_ID_FRIEND_ONLINE_NOTIFY, notify_data);
        }
    }
}

void FriendManager::notify_friends_offline(uint64_t player_id) {
    auto friend_ids = redis_->smembers(redis_key::friend_set(player_id));
    if (friend_ids.empty()) return;

    FriendOfflineNotify notify;
    notify.set_player_id(player_id);

    std::string notify_data;
    notify.SerializeToString(&notify_data);

    for (const auto& fid_str : friend_ids) {
        uint64_t fid = 0;
        try {
            fid = std::stoull(fid_str);
        } catch (...) {
            continue;
        }

        // Only notify online friends
        std::string friend_online = redis_->get(redis_key::player_online(fid));
        if (friend_online == "1") {
            session_->send_to_client(fid, MSG_ID_FRIEND_OFFLINE_NOTIFY, notify_data);
        }
    }
}

}  // namespace farm
