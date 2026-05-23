#pragma once

#include "player.h"
#include <unordered_map>
#include <memory>
#include <vector>

namespace farm {

class GateSession;

class PlayerManager {
public:
    PlayerManager() = default;
    ~PlayerManager() = default;

    // 添加玩家（返回 true 成功，false 重复）
    bool add_player(uint64_t player_id, GateSession* gate_session);

    // 移除玩家
    void remove_player(uint64_t player_id);

    // 查找玩家
    Player* get_player(uint64_t player_id) const;

    // 获取所有玩家
    std::vector<Player*> get_all_players() const;

    // 移除某个 GateSession 关联的所有玩家
    void remove_players_by_gate(GateSession* gate_session);

    // 当前在线玩家数
    size_t player_count() const;

private:
    std::unordered_map<uint64_t, std::unique_ptr<Player>> players_;
};

}  // namespace farm
