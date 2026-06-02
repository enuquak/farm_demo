#include "team_manager.h"
#include "message_ids.h"
#include "log_macros.h"

#include <chrono>
#include <algorithm>

namespace farm {

TeamManager::TeamManager(SendToPlayerFunc send_func)
    : send_func_(std::move(send_func))
{
}

bool TeamManager::init(const std::string& redis_host, int redis_port) {
    return redis_.connect(redis_host, redis_port);
}

int TeamManager::create_team(uint64_t player_id) {
    // Check if already in team
    if (is_in_team(player_id)) {
        return static_cast<int>(farm::TEAM_ALREADY_IN_TEAM);
    }

    uint64_t team_id = redis_.generate_team_id();
    if (team_id == 0) {
        SPDLOG_ERROR("[Team]Failed to generate team_id");
        return -1;
    }

    // Create in Redis
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    redis_.set_team_info(team_id, player_id, 0, 0);
    redis_.add_team_member(team_id, player_id);
    redis_.set_player_team(player_id, team_id);

    // Create in memory
    TeamInfo team;
    team.team_id = team_id;
    team.leader_id = player_id;
    team.members.push_back(player_id);
    team.status = TeamStatus::IDLE;
    team.cave_level = 0;
    team.created_at = now;
    teams_[team_id] = team;
    player_team_map_[player_id] = team_id;

    SPDLOG_INFO("[Team]Player {} created team {}", player_id, team_id);

    // Send response
    farm::TeamCreateResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    resp.set_team_id(team_id);
    std::string data;
    resp.SerializeToString(&data);
    send_func_(player_id, MSG_ID_TEAM_CREATE_RESP,
               reinterpret_cast<const uint8_t*>(data.data()), data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::disband_team(uint64_t player_id) {
    auto it = player_team_map_.find(player_id);
    if (it == player_team_map_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    uint64_t team_id = it->second;
    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    TeamInfo& team = team_it->second;
    if (team.leader_id != player_id) {
        return static_cast<int>(farm::TEAM_NOT_LEADER);
    }
    if (team.status == TeamStatus::IN_CAVE) {
        return static_cast<int>(farm::TEAM_IN_CAVE);
    }

    // Notify all members
    farm::TeamDisbandResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string data;
    resp.SerializeToString(&data);

    for (uint64_t mid : team.members) {
        send_func_(mid, MSG_ID_TEAM_DISBAND_RESP,
                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
        player_team_map_.erase(mid);
        redis_.del_player_team(mid);
    }

    // Clean up Redis
    for (uint64_t mid : team.members) {
        redis_.remove_team_member(team_id, mid);
    }
    redis_.del_team_info(team_id);

    // Remove from memory
    teams_.erase(team_it);

    SPDLOG_INFO("[Team]Team {} disbanded by player {}", team_id, player_id);
    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::invite_player(uint64_t inviter_id, uint64_t target_id) {
    if (inviter_id == target_id) {
        return static_cast<int>(farm::TEAM_SELF_OPERATION);
    }

    auto it = player_team_map_.find(inviter_id);
    if (it == player_team_map_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    uint64_t team_id = it->second;
    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    TeamInfo& team = team_it->second;
    if (team.leader_id != inviter_id) {
        return static_cast<int>(farm::TEAM_NOT_LEADER);
    }
    if (team.members.size() >= MAX_TEAM_SIZE) {
        return static_cast<int>(farm::TEAM_FULL);
    }
    if (is_in_team(target_id)) {
        return static_cast<int>(farm::TEAM_TARGET_ALREADY_IN_TEAM);
    }

    // Store invite in Redis
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    redis_.set_invite(target_id, team_id, inviter_id, now);

    SPDLOG_INFO("[Team]Player {} invited player {} to team {}",
                inviter_id, target_id, team_id);

    // Send invite notification to target
    farm::TeamInviteNotify notify;
    notify.set_team_id(team_id);
    notify.set_inviter_id(inviter_id);
    // TODO: set inviter_name and inviter_level from player data
    std::string notify_data;
    notify.SerializeToString(&notify_data);
    send_func_(target_id, MSG_ID_TEAM_INVITE_NOTIFY,
               reinterpret_cast<const uint8_t*>(notify_data.data()), notify_data.size());

    // Send response to inviter
    farm::TeamInviteResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_func_(inviter_id, MSG_ID_TEAM_INVITE_RESP,
               reinterpret_cast<const uint8_t*>(resp_data.data()), resp_data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::accept_invite(uint64_t player_id, uint64_t team_id) {
    // Check invite exists
    uint64_t inv_team_id = 0, inviter_id = 0, timestamp = 0;
    if (!redis_.get_invite(player_id, inv_team_id, inviter_id, timestamp)) {
        return static_cast<int>(farm::TEAM_INVITE_NOT_FOUND);
    }
    if (inv_team_id != team_id) {
        return static_cast<int>(farm::TEAM_INVITE_NOT_FOUND);
    }

    // Check if already in team (race condition)
    if (is_in_team(player_id)) {
        redis_.del_invite(player_id);
        return static_cast<int>(farm::TEAM_ALREADY_IN_TEAM);
    }

    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        redis_.del_invite(player_id);
        return static_cast<int>(farm::TEAM_INVITE_NOT_FOUND);
    }

    TeamInfo& team = team_it->second;
    if (team.members.size() >= MAX_TEAM_SIZE) {
        redis_.del_invite(player_id);
        return static_cast<int>(farm::TEAM_FULL);
    }

    // Add to team
    team.members.push_back(player_id);
    player_team_map_[player_id] = team_id;
    redis_.add_team_member(team_id, player_id);
    redis_.set_player_team(player_id, team_id);
    redis_.del_invite(player_id);

    SPDLOG_INFO("[Team]Player {} joined team {}", player_id, team_id);

    // Notify all members
    notify_member_update(team_id, 0 /* JOIN */, player_id, "", 0);

    // Send accept response with full member list
    farm::TeamAcceptResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    for (uint64_t mid : team.members) {
        auto* member = resp.add_members();
        member->set_player_id(mid);
        member->set_is_leader(mid == team.leader_id);
        // TODO: populate role_name, level, online, hp
    }
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_func_(player_id, MSG_ID_TEAM_ACCEPT_RESP,
               reinterpret_cast<const uint8_t*>(resp_data.data()), resp_data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::reject_invite(uint64_t player_id, uint64_t team_id) {
    redis_.del_invite(player_id);

    farm::TeamRejectResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string data;
    resp.SerializeToString(&data);
    send_func_(player_id, MSG_ID_TEAM_REJECT_RESP,
               reinterpret_cast<const uint8_t*>(data.data()), data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::leave_team(uint64_t player_id) {
    auto it = player_team_map_.find(player_id);
    if (it == player_team_map_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    uint64_t team_id = it->second;
    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        player_team_map_.erase(it);
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    TeamInfo& team = team_it->second;

    // Remove from team
    team.members.erase(
        std::remove(team.members.begin(), team.members.end(), player_id),
        team.members.end());
    player_team_map_.erase(player_id);
    redis_.remove_team_member(team_id, player_id);
    redis_.del_player_team(player_id);

    // If leader left, transfer to next member
    if (team.leader_id == player_id && !team.members.empty()) {
        uint64_t new_leader = team.members[0];  // earliest joiner
        team.leader_id = new_leader;
        redis_.set_team_info(team_id, new_leader,
                             static_cast<uint32_t>(team.status), team.cave_level);

        SPDLOG_INFO("[Team]Leader transferred to player {} in team {}",
                    new_leader, team_id);

        // Notify leader change
        notify_leader_change(team_id, new_leader, "");
    }

    // If team is empty, disband
    if (team.members.empty()) {
        redis_.del_team_info(team_id);
        teams_.erase(team_it);
        SPDLOG_INFO("[Team]Team {} auto-disbanded (empty)", team_id);
    } else {
        // Notify remaining members
        notify_member_update(team_id, 1 /* LEAVE */, player_id, "", 0);
    }

    // Send response to leaving player
    farm::TeamLeaveResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string data;
    resp.SerializeToString(&data);
    send_func_(player_id, MSG_ID_TEAM_LEAVE_RESP,
               reinterpret_cast<const uint8_t*>(data.data()), data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

int TeamManager::kick_member(uint64_t leader_id, uint64_t target_id) {
    if (leader_id == target_id) {
        return static_cast<int>(farm::TEAM_CANNOT_KICK_SELF);
    }

    auto it = player_team_map_.find(leader_id);
    if (it == player_team_map_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    uint64_t team_id = it->second;
    auto team_it = teams_.find(team_id);
    if (team_it == teams_.end()) {
        return static_cast<int>(farm::TEAM_NOT_IN_TEAM);
    }

    TeamInfo& team = team_it->second;
    if (team.leader_id != leader_id) {
        return static_cast<int>(farm::TEAM_NOT_LEADER);
    }

    // Check target is in team
    auto target_it = std::find(team.members.begin(), team.members.end(), target_id);
    if (target_it == team.members.end()) {
        return static_cast<int>(farm::TEAM_TARGET_NOT_FOUND);
    }

    // Remove target
    team.members.erase(target_it);
    player_team_map_.erase(target_id);
    redis_.remove_team_member(team_id, target_id);
    redis_.del_player_team(target_id);

    SPDLOG_INFO("[Team]Player {} kicked player {} from team {}",
                leader_id, target_id, team_id);

    // Notify kicked player
    farm::TeamMemberUpdate kick_notify;
    kick_notify.set_update_type(farm::TEAM_UPDATE_KICK);
    auto* member = kick_notify.mutable_member();
    member->set_player_id(target_id);
    std::string kick_data;
    kick_notify.SerializeToString(&kick_data);
    send_func_(target_id, MSG_ID_TEAM_MEMBER_UPDATE,
               reinterpret_cast<const uint8_t*>(kick_data.data()), kick_data.size());

    // Notify remaining members
    notify_member_update(team_id, 2 /* KICK */, target_id, "", 0);

    // Send response to leader
    farm::TeamKickResp resp;
    resp.set_code(farm::TEAM_SUCCESS);
    std::string resp_data;
    resp.SerializeToString(&resp_data);
    send_func_(leader_id, MSG_ID_TEAM_KICK_RESP,
               reinterpret_cast<const uint8_t*>(resp_data.data()), resp_data.size());

    return static_cast<int>(farm::TEAM_SUCCESS);
}

TeamInfo* TeamManager::get_team(uint64_t team_id) {
    auto it = teams_.find(team_id);
    return (it != teams_.end()) ? &it->second : nullptr;
}

TeamInfo* TeamManager::get_player_team(uint64_t player_id) {
    auto it = player_team_map_.find(player_id);
    if (it == player_team_map_.end()) return nullptr;
    return get_team(it->second);
}

bool TeamManager::is_in_team(uint64_t player_id) {
    return player_team_map_.find(player_id) != player_team_map_.end();
}

void TeamManager::check_invite_timeouts() {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // This is a simplified check - in production, iterate all pending invites
    // For now, invites are cleaned up on accept/reject/timeout via Redis TTL
}

void TeamManager::notify_member_update(uint64_t team_id, uint32_t update_type,
                                       uint64_t member_id, const std::string& member_name,
                                       uint32_t member_level) {
    auto* team = get_team(team_id);
    if (!team) return;

    farm::TeamMemberUpdate notify;
    notify.set_update_type(static_cast<farm::TeamUpdateType>(update_type));
    auto* member = notify.mutable_member();
    member->set_player_id(member_id);
    member->set_role_name(member_name);
    member->set_level(member_level);

    for (uint64_t mid : team->members) {
        auto* m = notify.add_all_members();
        m->set_player_id(mid);
        m->set_is_leader(mid == team->leader_id);
    }

    std::string data;
    notify.SerializeToString(&data);

    for (uint64_t mid : team->members) {
        send_func_(mid, MSG_ID_TEAM_MEMBER_UPDATE,
                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
}

void TeamManager::notify_leader_change(uint64_t team_id, uint64_t new_leader_id,
                                       const std::string& new_leader_name) {
    auto* team = get_team(team_id);
    if (!team) return;

    farm::TeamLeaderChange notify;
    notify.set_new_leader_id(new_leader_id);
    notify.set_new_leader_name(new_leader_name);

    std::string data;
    notify.SerializeToString(&data);

    for (uint64_t mid : team->members) {
        send_func_(mid, MSG_ID_TEAM_LEADER_CHANGE,
                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
}

void TeamManager::notify_status_update(uint64_t team_id, uint32_t status, int cave_level) {
    auto* team = get_team(team_id);
    if (!team) return;

    farm::TeamStatusUpdate notify;
    notify.set_status(status);
    notify.set_cave_level(cave_level);

    std::string data;
    notify.SerializeToString(&data);

    for (uint64_t mid : team->members) {
        send_func_(mid, MSG_ID_TEAM_STATUS_UPDATE,
                   reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
}

void TeamManager::load_team_from_redis(uint64_t team_id) {
    // Load team info from Redis into memory
    uint64_t leader_id = 0;
    uint32_t status = 0;
    int cave_level = 0;
    if (!redis_.get_team_info(team_id, leader_id, status, cave_level)) return;

    std::vector<uint64_t> members;
    if (!redis_.get_team_members(team_id, members)) return;

    TeamInfo team;
    team.team_id = team_id;
    team.leader_id = leader_id;
    team.members = members;
    team.status = static_cast<TeamStatus>(status);
    team.cave_level = cave_level;
    teams_[team_id] = team;

    for (uint64_t mid : members) {
        player_team_map_[mid] = team_id;
    }
}

void TeamManager::remove_team_from_memory(uint64_t team_id) {
    auto it = teams_.find(team_id);
    if (it == teams_.end()) return;
    for (uint64_t mid : it->second.members) {
        player_team_map_.erase(mid);
    }
    teams_.erase(it);
}

}  // namespace farm
