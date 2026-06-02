#include "chat_manager.h"
#include "friend_constants.h"
#include "friend_manager.h"
#include "redis_connection.h"
#include "game_session.h"
#include "message_ids.h"
#include "log_macros.h"

#include "friend.pb.h"

#include <ctime>
#include <sstream>

namespace farm {

ChatManager::ChatManager(RedisConnection* redis, GameSession* session,
                         FriendManager* friend_mgr)
    : redis_(redis)
    , session_(session)
    , friend_mgr_(friend_mgr)
{
}

// ===========================================
// handle_send
// ===========================================

void ChatManager::handle_send(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendChatReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[ChatManager]Failed to parse FriendChatReq from player={}", player_id);
        return;
    }

    uint64_t receiver_id = req.receiver_id();
    int32_t msg_type = req.msg_type();
    const std::string& content = req.content();

    FriendChatResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Validate content length
    if (content.size() > MAX_CHAT_MSG_LENGTH) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::CHAT_MSG_TOO_LONG));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_CHAT_RESP, resp_data);
        return;
    }

    // Validate: must be friends
    if (!friend_mgr_->is_friend(player_id, receiver_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::NOT_FRIENDS));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_CHAT_RESP, resp_data);
        return;
    }

    // Validate: receiver not blocked by sender
    if (friend_mgr_->is_blocked(player_id, receiver_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::BLOCKED));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_CHAT_RESP, resp_data);
        return;
    }

    // Validate: sender not blocked by receiver
    if (friend_mgr_->is_blocked(receiver_id, player_id)) {
        resp.set_code(static_cast<int32_t>(FriendErrorCode::BLOCKED));
        std::string resp_data;
        resp.SerializeToString(&resp_data);
        session_->send_to_client(player_id, MSG_ID_FRIEND_CHAT_RESP, resp_data);
        return;
    }

    uint64_t now = static_cast<uint64_t>(std::time(nullptr));

    // Build notification
    std::string notify_data = build_chat_notify(player_id, msg_type, content, now);

    // Check if receiver is online
    std::string receiver_online = redis_->get(redis_key::player_online(receiver_id));
    if (receiver_online == "1") {
        // Send directly to receiver
        session_->send_to_client(receiver_id, MSG_ID_FRIEND_CHAT_NOTIFY, notify_data);
    } else {
        // Store as offline message using hash with index
        std::string counter_key = redis_key::chat_offline(receiver_id) + ":cnt";
        std::string cnt_str = redis_->get(counter_key);
        int64_t cnt = 0;
        if (!cnt_str.empty()) {
            try { cnt = std::stoll(cnt_str); } catch (...) { cnt = 0; }
        }

        // Cap at MAX_OFFLINE_MESSAGES: if at capacity, shift messages
        if (cnt >= static_cast<int64_t>(MAX_OFFLINE_MESSAGES)) {
            // Remove oldest (field "msg:0") and shift
            redis_->hdel(redis_key::chat_offline(receiver_id), "msg:0");
            // Shift remaining messages down by 1
            for (int64_t i = 1; i < cnt; ++i) {
                std::string old_val = redis_->hget(redis_key::chat_offline(receiver_id),
                                                    "msg:" + std::to_string(i));
                if (!old_val.empty()) {
                    redis_->hset(redis_key::chat_offline(receiver_id),
                                 "msg:" + std::to_string(i - 1), old_val);
                }
            }
            cnt = MAX_OFFLINE_MESSAGES - 1;
        }

        // Store the new message
        redis_->hset(redis_key::chat_offline(receiver_id),
                     "msg:" + std::to_string(cnt), notify_data);
        redis_->set(counter_key, std::to_string(cnt + 1));
    }

    // Store to chat history
    store_to_history(player_id, receiver_id, msg_type, content, now);

    // Send response to sender
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_CHAT_RESP, resp_data);

    SPDLOG_INFO("[ChatManager]Player={} sent chat to player={}, type={}",
                player_id, receiver_id, msg_type);
}

// ===========================================
// handle_history
// ===========================================

void ChatManager::handle_history(uint64_t player_id, const uint8_t* data, size_t len) {
    FriendChatHistoryReq req;
    if (len > 0 && !req.ParseFromArray(data, static_cast<int>(len))) {
        SPDLOG_ERROR("[ChatManager]Failed to parse FriendChatHistoryReq from player={}", player_id);
        return;
    }

    uint64_t target_id = req.target_id();

    FriendChatHistoryResp resp;
    resp.set_code(static_cast<int32_t>(FriendErrorCode::SUCCESS));

    // Load from Redis chat history hash
    // Key uses ordered pair (min,max) to deduplicate conversation
    uint64_t pid1 = player_id;
    uint64_t pid2 = target_id;
    if (pid1 > pid2) std::swap(pid1, pid2);

    std::string hist_key = "chat:history:" + std::to_string(pid1) + ":" + std::to_string(pid2);
    auto messages = redis_->hgetall(hist_key);

    // Parse and sort by timestamp
    struct MsgEntry {
        uint64_t ts;
        std::string data;
    };
    std::vector<MsgEntry> entries;
    for (const auto& [field, value] : messages) {
        // value format: "sender_id|receiver_id|msg_type|content|timestamp"
        std::istringstream ss(value);
        std::string part;
        MsgEntry entry;
        entry.data = value;

        std::getline(ss, part, '|');
        try { entry.ts = std::stoull(part); } catch (...) { continue; }
        // Actually the timestamp is the last field, let's extract from the end
        // Format: sender_id|receiver_id|msg_type|timestamp|content
        // Re-parse properly below
        entries.push_back(entry);
    }

    // Re-parse messages properly
    for (const auto& [field, value] : messages) {
        // value format: sender_id:receiver_id:msg_type:timestamp:content
        // Use ':' as delimiter since content may contain '|'
        std::istringstream ss(value);
        std::string sender_str, recv_str, type_str, ts_str, msg_content;
        if (!std::getline(ss, sender_str, ':')) continue;
        if (!std::getline(ss, recv_str, ':')) continue;
        if (!std::getline(ss, type_str, ':')) continue;
        if (!std::getline(ss, ts_str, ':')) continue;
        std::getline(ss, msg_content);  // rest of string

        auto* msg = resp.add_messages();
        try {
            msg->set_sender_id(std::stoull(sender_str));
            msg->set_receiver_id(std::stoull(recv_str));
            msg->set_msg_type(std::stoi(type_str));
            msg->set_timestamp(std::stoull(ts_str));
        } catch (...) {
            continue;
        }
        msg->set_content(msg_content);
    }

    std::string resp_data;
    resp.SerializeToString(&resp_data);
    session_->send_to_client(player_id, MSG_ID_FRIEND_CHAT_HISTORY_RESP, resp_data);

    SPDLOG_INFO("[ChatManager]Player={} requested history with player={}, {} messages",
                player_id, target_id, resp.messages_size());
}

// ===========================================
// deliver_offline_messages
// ===========================================

void ChatManager::deliver_offline_messages(uint64_t player_id) {
    auto messages = redis_->hgetall(redis_key::chat_offline(player_id));
    if (messages.empty()) return;

    SPDLOG_INFO("[ChatManager]Delivering {} offline messages to player={}",
                messages.size(), player_id);

    // Send each stored notification
    for (const auto& [field, notify_data] : messages) {
        session_->send_to_client(player_id, MSG_ID_FRIEND_CHAT_NOTIFY, notify_data);
    }

    // Clear offline messages
    redis_->del(redis_key::chat_offline(player_id));
    redis_->del(redis_key::chat_offline(player_id) + ":cnt");
}

// ===========================================
// Helpers
// ===========================================

std::string ChatManager::build_chat_notify(uint64_t sender_id, int32_t msg_type,
                                            const std::string& content, uint64_t timestamp) {
    FriendChatNotify notify;
    notify.set_sender_id(sender_id);
    notify.set_msg_type(msg_type);
    notify.set_content(content);
    notify.set_timestamp(timestamp);

    // Fill sender name from player_info
    std::string info_raw = redis_->get(redis_key::player_info(sender_id));
    if (!info_raw.empty()) {
        // Parse "role_name|level|scene_id"
        size_t pipe1 = info_raw.find('|');
        if (pipe1 != std::string::npos) {
            notify.set_sender_name(info_raw.substr(0, pipe1));
        }
    }

    std::string data;
    notify.SerializeToString(&data);
    return data;
}

void ChatManager::store_to_history(uint64_t sender_id, uint64_t receiver_id,
                                    int32_t msg_type, const std::string& content,
                                    uint64_t timestamp) {
    // Use ordered pair for conversation key
    uint64_t pid1 = sender_id;
    uint64_t pid2 = receiver_id;
    if (pid1 > pid2) std::swap(pid1, pid2);

    std::string hist_key = "chat:history:" + std::to_string(pid1) + ":" + std::to_string(pid2);

    // Store as hash field = timestamp (unique), value = serialized message
    std::string value = std::to_string(sender_id) + ":" +
                        std::to_string(receiver_id) + ":" +
                        std::to_string(msg_type) + ":" +
                        std::to_string(timestamp) + ":" +
                        content;

    redis_->hset(hist_key, std::to_string(timestamp), value);

    // Mark dirty for persistence
    redis_->set(redis_key::dirty(sender_id), "1");
}

}  // namespace farm
