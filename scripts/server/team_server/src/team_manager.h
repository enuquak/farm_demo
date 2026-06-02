#pragma once

#include "redis_client.h"
#include "team.pb.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace farm {

struct TeamInfo {
    uint64_t team_id;
    uint64_t leader_id;
    std::vector<uint64_t> members;  // leader at index 0
    TeamStatus status;
    int cave_level;
    uint64_t created_at;
};

enum class TeamStatus : int32_t {
    IDLE = 0,
    IN_CAVE = 1,
};

// Callback to send a message to a player via GateServer
using SendToPlayerFunc = std::function<void(uint64_t player_id, uint32_t msg_id,
                                            const uint8_t* payload, size_t len)>;

class TeamManager {
public:
    explicit TeamManager(SendToPlayerFunc send_func);
    ~TeamManager() = default;

    // Initialize Redis connection
    bool init(const std::string& redis_host, int redis_port);

    // Core operations
    int create_team(uint64_t player_id);
    int disband_team(uint64_t player_id);
    int invite_player(uint64_t inviter_id, uint64_t target_id);
    int accept_invite(uint64_t player_id, uint64_t team_id);
    int reject_invite(uint64_t player_id, uint64_t team_id);
    int leave_team(uint64_t player_id);
    int kick_member(uint64_t leader_id, uint64_t target_id);

    // Query
    TeamInfo* get_team(uint64_t team_id);
    TeamInfo* get_player_team(uint64_t player_id);
    bool is_in_team(uint64_t player_id);

    // Invite timeout check (called periodically)
    void check_invite_timeouts();

    // Notify helpers
    void notify_member_update(uint64_t team_id, uint32_t update_type,
                              uint64_t member_id, const std::string& member_name,
                              uint32_t member_level);
    void notify_leader_change(uint64_t team_id, uint64_t new_leader_id,
                              const std::string& new_leader_name);
    void notify_status_update(uint64_t team_id, uint32_t status, int cave_level);

private:
    void load_team_from_redis(uint64_t team_id);
    void remove_team_from_memory(uint64_t team_id);

    SendToPlayerFunc send_func_;
    RedisClient redis_;

    // In-memory cache: team_id -> TeamInfo
    std::unordered_map<uint64_t, TeamInfo> teams_;
    // Player -> team_id mapping (in-memory for fast lookup)
    std::unordered_map<uint64_t, uint64_t> player_team_map_;

    static constexpr uint64_t MAX_TEAM_SIZE = 4;
    static constexpr uint64_t INVITE_TIMEOUT_SEC = 60;
};

}  // namespace farm
