#include "player_manager.h"
#include "gate_session.h"
#include <iostream>

namespace farm {

bool PlayerManager::add_player(uint64_t player_id, GateSession* gate_session) {
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        // 玩家已存在（重复加入）
        return false;
    }
    players_[player_id] = std::make_unique<Player>(player_id, gate_session);
    return true;
}

void PlayerManager::remove_player(uint64_t player_id) {
    players_.erase(player_id);
}

Player* PlayerManager::get_player(uint64_t player_id) const {
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<Player*> PlayerManager::get_all_players() const {
    std::vector<Player*> result;
    result.reserve(players_.size());
    for (auto& kv : players_) {
        result.push_back(kv.second.get());
    }
    return result;
}

void PlayerManager::remove_players_by_gate(GateSession* gate_session) {
    std::vector<uint64_t> to_remove;
    for (auto& kv : players_) {
        if (kv.second->gate_session() == gate_session) {
            to_remove.push_back(kv.first);
        }
    }
    for (uint64_t pid : to_remove) {
        std::cout << "[PlayerManager] Removing player " << pid
                  << " due to Gate disconnect" << std::endl;
        players_.erase(pid);
    }
}

size_t PlayerManager::player_count() const {
    return players_.size();
}

}  // namespace farm
