#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

// Forward declare hiredis types
struct redisContext;
struct redisReply;

namespace farm {

class RedisClient {
public:
    RedisClient();
    ~RedisClient();

    bool connect(const std::string& host, int port);
    void disconnect();
    bool is_connected() const { return ctx_ != nullptr; }

    // Team info operations
    bool set_team_info(uint64_t team_id, uint64_t leader_id, uint32_t status, int cave_level);
    bool get_team_info(uint64_t team_id, uint64_t& leader_id, uint32_t& status, int& cave_level);
    bool del_team_info(uint64_t team_id);

    // Team members operations
    bool add_team_member(uint64_t team_id, uint64_t player_id);
    bool remove_team_member(uint64_t team_id, uint64_t player_id);
    bool get_team_members(uint64_t team_id, std::vector<uint64_t>& members);
    bool is_team_member(uint64_t team_id, uint64_t player_id);
    size_t get_team_member_count(uint64_t team_id);

    // Player -> Team mapping
    bool set_player_team(uint64_t player_id, uint64_t team_id);
    bool get_player_team(uint64_t player_id, uint64_t& team_id);
    bool del_player_team(uint64_t player_id);

    // Invite operations
    bool set_invite(uint64_t player_id, uint64_t team_id, uint64_t inviter_id, uint64_t timestamp);
    bool get_invite(uint64_t player_id, uint64_t& team_id, uint64_t& inviter_id, uint64_t& timestamp);
    bool del_invite(uint64_t player_id);

    // Team ID generation
    uint64_t generate_team_id();

private:
    redisContext* ctx_;
};

}  // namespace farm
