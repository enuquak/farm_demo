#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace farm {

// Callback to send a message to a specific player via GateServer
using SendToPlayerFunc = std::function<void(uint64_t player_id, uint32_t msg_id,
                                            const uint8_t* payload, size_t len)>;

// Channel configuration
struct ChannelConfig {
    uint32_t channel_type;      // 0=world, 1=party, 2=whisper
    int max_msg_length;         // Max message length
    float cooldown_sec;         // Send cooldown
};

class ChannelManager {
public:
    explicit ChannelManager(SendToPlayerFunc send_func);
    ~ChannelManager() = default;

    // Player lifecycle
    void add_player(uint64_t player_id, const std::string& player_name);
    void remove_player(uint64_t player_id);

    // Process a chat message: validate, then broadcast
    // Returns error code (0 = success)
    int process_message(uint64_t sender_id, uint32_t channel_type,
                        const std::string& content, uint64_t target_id);

    // Get player name (returns empty string if not found)
    std::string get_player_name(uint64_t player_id) const;

    // Get channel config
    static ChannelConfig get_channel_config(uint32_t channel_type);

private:
    void broadcast_to_world(uint64_t sender_id, const std::string& sender_name,
                            const std::string& content, uint64_t timestamp);
    void send_whisper(uint64_t sender_id, const std::string& sender_name,
                      const std::string& content, uint64_t target_id, uint64_t timestamp);

    SendToPlayerFunc send_func_;

    // Online players: player_id -> player_name
    std::unordered_map<uint64_t, std::string> players_;

    // Online player IDs (for fast iteration during broadcast)
    std::unordered_set<uint64_t> online_player_ids_;
};

}  // namespace farm
