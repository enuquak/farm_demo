#include "monster_manager.h"
#include "message_ids.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>

namespace farm {

ServerMonsterManager::ServerMonsterManager() = default;

void ServerMonsterManager::load_definitions(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f.is_open()) {
        SPDLOG_WARN("[MonsterManager]Cannot open monster defs: {}", json_path);
        return;
    }

    nlohmann::json j;
    f >> j;

    for (const auto& m : j["monsters"]) {
        MonsterDef def;
        def.id = m["id"];
        def.name = m["name"];
        def.hp = m["hp"];
        def.attack = m["attack"];
        def.defense = m["defense"];
        def.speed = m["speed"];
        def.exp_reward = m["exp_reward"];
        def.aggro_range = m["aggro_range"];
        def.attack_range = m["attack_range"];
        def.attack_cooldown = m["attack_cooldown"];
        monster_defs_[def.id] = def;
    }

    SPDLOG_INFO("[MonsterManager]Loaded {} monster definitions", monster_defs_.size());
}

uint32_t ServerMonsterManager::spawn_monster(const std::string& monster_type, float x, float y) {
    auto it = monster_defs_.find(monster_type);
    if (it == monster_defs_.end()) {
        SPDLOG_WARN("[MonsterManager]Unknown monster type: {}", monster_type);
        return 0;
    }

    const auto& def = it->second;
    uint32_t id = next_monster_id_++;

    ServerMonster monster;
    monster.monster_id = id;
    monster.monster_type = monster_type;
    monster.x = x;
    monster.y = y;
    monster.hp = def.hp;
    monster.max_hp = def.hp;
    monster.attack = def.attack;
    monster.defense = def.defense;
    monster.speed = def.speed;
    monster.exp_reward = def.exp_reward;
    monster.last_attack_time = 0;
    monster.spawn_time = std::time(nullptr);

    monsters_[id] = monster;
    SPDLOG_INFO("[MonsterManager]Spawned monster_id={} type={} at ({},{})", id, monster_type, x, y);
    return id;
}

void ServerMonsterManager::remove_monster(uint32_t monster_id) {
    monsters_.erase(monster_id);
}

ServerMonster* ServerMonsterManager::get_monster(uint32_t monster_id) {
    auto it = monsters_.find(monster_id);
    return it != monsters_.end() ? &it->second : nullptr;
}

const ServerMonster* ServerMonsterManager::get_monster(uint32_t monster_id) const {
    auto it = monsters_.find(monster_id);
    return it != monsters_.end() ? &it->second : nullptr;
}

std::vector<ServerMonster*> ServerMonsterManager::get_monsters_in_range(float x, float y, float range) {
    std::vector<ServerMonster*> result;
    for (auto& [id, m] : monsters_) {
        float dx = m.x - x;
        float dy = m.y - y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= range) {
            result.push_back(&m);
        }
    }
    return result;
}

void ServerMonsterManager::update(float dt, uint64_t player_id,
                                   float player_x, float player_y,
                                   SendGameMsgFunc send_msg) {
    time_t now = std::time(nullptr);

    for (auto& [id, m] : monsters_) {
        if (m.hp <= 0) continue;

        float dx = player_x - m.x;
        float dy = player_y - m.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        auto def_it = monster_defs_.find(m.monster_type);
        if (def_it == monster_defs_.end()) continue;

        const auto& def = def_it->second;
        float aggro_range = def.aggro_range * 16.0f;  // TILE_SIZE = 16
        float attack_range = def.attack_range * 16.0f;

        if (dist < aggro_range) {
            // 追击
            if (dist > attack_range && dist > 0) {
                float nx = dx / dist;
                float ny = dy / dist;
                m.x += nx * m.speed * dt;
                m.y += ny * m.speed * dt;
            }
            // 攻击
            else if (dist <= attack_range) {
                float cooldown = def.attack_cooldown;
                if (difftime(now, m.last_attack_time) >= cooldown) {
                    m.last_attack_time = now;
                    // 通知客户端怪物攻击（伤害由客户端计算展示，服务器验证）
                    // 这里简化：服务器直接通知
                }
            }
        }
    }
}

int32_t ServerMonsterManager::damage_monster(uint32_t monster_id, int32_t damage) {
    auto* m = get_monster(monster_id);
    if (!m || m->hp <= 0) return 0;

    m->hp -= damage;
    if (m->hp < 0) m->hp = 0;

    return m->hp;
}

bool ServerMonsterManager::is_monster_dead(uint32_t monster_id) const {
    const auto* m = get_monster(monster_id);
    return m && m->hp <= 0;
}

}  // namespace farm
