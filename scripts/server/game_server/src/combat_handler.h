#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <vector>

namespace farm {

class Player;
class ServerMonsterManager;
class DropItemManager;

using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;
using QueryTeamMembersFunc = std::function<std::vector<uint64_t>(uint64_t player_id)>;

struct AttackResult {
    int32_t monster_id;
    int32_t damage;
    bool is_kill;
};

class CombatHandler {
public:
    CombatHandler(ServerMonsterManager* monster_mgr, DropItemManager* drop_mgr);

    // 设置队伍查询回调
    void set_team_query_func(QueryTeamMembersFunc func) { query_team_func_ = func; }

    // 处理攻击请求
    void handle_attack_req(uint64_t player_id,
                           const uint8_t* payload, size_t payload_len,
                           Player* player, SendGameMsgFunc send_msg);

    // 怪物攻击玩家
    void monster_attack_player(uint32_t monster_id, Player* player,
                                SendGameMsgFunc send_msg);

    // 分配经验值（支持队伍分配）
    void distribute_exp(uint64_t killer_id, int32_t exp, Player* player,
                        SendGameMsgFunc send_msg);

private:
    ServerMonsterManager* monster_mgr_;
    DropItemManager* drop_mgr_;
    QueryTeamMembersFunc query_team_func_;
};

}  // namespace farm
