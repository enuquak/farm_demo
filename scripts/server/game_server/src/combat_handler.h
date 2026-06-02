#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace farm {

class Player;
class ServerMonsterManager;
class DropItemManager;

using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;

struct AttackResult {
    int32_t monster_id;
    int32_t damage;
    bool is_kill;
};

class CombatHandler {
public:
    CombatHandler(ServerMonsterManager* monster_mgr, DropItemManager* drop_mgr);

    // 处理攻击请求
    void handle_attack_req(uint64_t player_id,
                           const uint8_t* payload, size_t payload_len,
                           Player* player, SendGameMsgFunc send_msg);

    // 怪物攻击玩家
    void monster_attack_player(uint32_t monster_id, Player* player,
                                SendGameMsgFunc send_msg);

private:
    ServerMonsterManager* monster_mgr_;
    DropItemManager* drop_mgr_;
};

}  // namespace farm
