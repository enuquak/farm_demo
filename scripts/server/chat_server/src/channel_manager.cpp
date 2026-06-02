#include "channel_manager.h"
#include "chat.pb.h"
#include "message_ids.h"
#include "log_macros.h"

#include <chrono>

namespace farm {

ChannelManager::ChannelManager(SendToPlayerFunc send_func)
    : send_func_(std::move(send_func)) {
}

void ChannelManager::add_player(uint64_t player_id, const std::string& player_name) {
    players_[player_id] = player_name;
    online_player_ids_.insert(player_id);
    SPDLOG_INFO("[Chat]Player online: id={}, name={}", player_id, player_name);
}

void ChannelManager::remove_player(uint64_t player_id) {
    players_.erase(player_id);
    online_player_ids_.erase(player_id);
    SPDLOG_INFO("[Chat]Player offline: id={}", player_id);
}

std::string ChannelManager::get_player_name(uint64_t player_id) const {
    auto it = players_.find(player_id);
    return it != players_.end() ? it->second : "";
}

ChannelConfig ChannelManager::get_channel_config(uint32_t channel_type) {
    switch (channel_type) {
        case 0:  // WORLD
            return {0, 100, 5.0f};
        case 1:  // PARTY
            return {1, 200, 3.0f};
        case 2:  // WHISPER
            return {2, 500, 1.0f};
        default:
            return {channel_type, 100, 5.0f};
    }
}

int ChannelManager::process_message(uint64_t sender_id, uint32_t channel_type,
                                     const std::string& content, uint64_t target_id) {
    // Validate sender is online
    auto sender_it = players_.find(sender_id);
    if (sender_it == players_.end()) {
        return 4;  // CHANNEL_NOT_FOUND (sender not registered)
    }

    // Validate channel type
    if (channel_type > 2) {
        return 4;  // CHANNEL_NOT_FOUND
    }

    // Validate message length
    ChannelConfig config = get_channel_config(channel_type);
    if (static_cast<int>(content.size()) > config.max_msg_length) {
        return 2;  // MSG_TOO_LONG
    }

    // Validate content not empty
    if (content.empty()) {
        return 2;  // MSG_TOO_LONG (treat empty as invalid)
    }

    std::string sender_name = sender_it->second;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    switch (channel_type) {
        case 0:  // WORLD
            broadcast_to_world(sender_id, sender_name, content, timestamp);
            break;
        case 1:  // PARTY - placeholder for future
            SPDLOG_WARN("[Chat]Party channel not implemented yet");
            break;
        case 2:  // WHISPER
            if (target_id == 0) return 3;  // TARGET_OFFLINE
            send_whisper(sender_id, sender_name, content, target_id, timestamp);
            break;
    }

    return 0;  // SUCCESS
}

void ChannelManager::broadcast_to_world(uint64_t sender_id, const std::string& sender_name,
                                         const std::string& content, uint64_t timestamp) {
    // Build ChatMessage proto
    ChatMessage msg;
    msg.set_channel_type(CHANNEL_WORLD);
    msg.set_sender_id(sender_id);
    msg.set_sender_name(sender_name);
    msg.set_content(content);
    msg.set_timestamp(timestamp);

    std::string payload;
    msg.SerializeToString(&payload);

    // Broadcast to all online players
    for (uint64_t pid : online_player_ids_) {
        send_func_(pid, MSG_ID_CHAT_MESSAGE,
                   reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    }

    SPDLOG_DEBUG("[Chat]World broadcast from {}: {}", sender_name, content);
}

void ChannelManager::send_whisper(uint64_t sender_id, const std::string& sender_name,
                                   const std::string& content, uint64_t target_id,
                                   uint64_t timestamp) {
    // Check target is online
    auto target_it = players_.find(target_id);
    if (target_it == players_.end()) {
        // Send error back to sender
        ChatSendResp resp;
        resp.set_code(CHAT_TARGET_OFFLINE);
        resp.set_msg("Player not online");
        std::string resp_payload;
        resp.SerializeToString(&resp_payload);
        send_func_(sender_id, MSG_ID_CHAT_SEND_RESP,
                   reinterpret_cast<const uint8_t*>(resp_payload.data()), resp_payload.size());
        return;
    }

    // Build ChatMessage
    ChatMessage msg;
    msg.set_channel_type(CHANNEL_WHISPER);
    msg.set_sender_id(sender_id);
    msg.set_sender_name(sender_name);
    msg.set_content(content);
    msg.set_timestamp(timestamp);
    msg.set_target_id(target_id);

    std::string payload;
    msg.SerializeToString(&payload);

    // Send to target
    send_func_(target_id, MSG_ID_CHAT_MESSAGE,
               reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    // Send to sender (so they see their own whisper)
    send_func_(sender_id, MSG_ID_CHAT_MESSAGE,
               reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

}  // namespace farm
