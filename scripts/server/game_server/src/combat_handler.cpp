#include "combat_handler.h"
#include "monster_manager.h"
#include "player.h"
#include "message_ids.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>
#include <cmath>
#include <random>

namespace farm {

CombatHandler::CombatHandler(ServerMonsterManager* monster_mgr, DropItemManager* drop_mgr)
    : monster_mgr_(monster_mgr), drop_mgr_(drop_mgr) {}

void CombatHandler::handle_attack_req(uint64_t player_id,
                                       const uint8_t* payload, size_t payload_len,
                                       Player* player, SendGameMsgFunc send_msg) {
    // 解析 AttackReq
    // 简化：使用 JSON 格式
    try {
        std::string json_str(reinterpret_cast<const char*>(payload), payload_len);
        nlohmann::json req = nlohmann::json::parse(json_str);

        std::string weapon_id = req.value("weapon_id", "");
        float dir_x = req.value("dir_x", 0.0f);
        float dir_y = req.value("dir_y", 1.0f);

        // 获取玩家位置
        float px = player->get_pos_x();
        float py = player->get_pos_y();
        int32_t player_attack = player->get_attack_power();

        // 武器攻击力（简化：从配置读取）
        int32_t weapon_attack = 10;  // 默认木剑
        float attack_range = 1.5f * 16.0f;  // 1.5 tiles
        float knockback = 2.0f;

        // 攻击范围检测
        float attack_cx = px + dir_x * attack_range * 0.6f;
        float attack_cy = py + dir_y * attack_range * 0.6f;

        auto monsters = monster_mgr_->get_monsters_in_range(attack_cx, attack_cy, attack_range);

        nlohmann::json resp;
        resp["hits"] = nlohmann::json::array();

        static std::mt19937 rng(std::random_device{}());

        for (auto* m : monsters) {
            if (m->hp <= 0) continue;

            // 计算伤害
            int32_t base_damage = weapon_attack + player_attack - m->defense;
            std::uniform_real_distribution<float> dist(0.9f, 1.1f);
            int32_t damage = std::max(1, static_cast<int>(base_damage * dist(rng)));

            // 应用伤害
            int32_t remaining_hp = monster_mgr_->damage_monster(m->monster_id, damage);
            bool is_kill = remaining_hp <= 0;

            nlohmann::json hit;
            hit["monster_id"] = m->monster_id;
            hit["damage"] = damage;
            hit["remaining_hp"] = remaining_hp;
            hit["is_kill"] = is_kill;
            resp["hits"].push_back(hit);

            SPDLOG_INFO("[Combat]Player {} hit monster {} for {} damage, hp={}, kill={}",
                        player_id, m->monster_id, damage, remaining_hp, is_kill);
        }

        // 发送 AttackNotify
        std::string resp_str = resp.dump();
        send_msg(player_id, MSG_ID_ATTACK_NOTIFY,
                 reinterpret_cast<const uint8_t*>(resp_str.data()), resp_str.size());

    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Combat]Failed to parse AttackReq: {}", e.what());
    }
}

void CombatHandler::monster_attack_player(uint32_t monster_id, Player* player,
                                            SendGameMsgFunc send_msg) {
    const auto* m = monster_mgr_->get_monster(monster_id);
    if (!m || m->hp <= 0) return;

    int32_t damage = std::max(1, m->attack - player->get_defense_power());
    int32_t new_hp = player->get_current_hp() - damage;
    if (new_hp < 0) new_hp = 0;

    player->set_current_hp(new_hp);

    // 发送 HP 更新
    nlohmann::json hp_update;
    hp_update["current_hp"] = new_hp;
    hp_update["max_hp"] = player->get_max_hp();
    hp_update["damage"] = damage;
    hp_update["source"] = "monster";
    hp_update["monster_id"] = monster_id;

    std::string data = hp_update.dump();
    send_msg(player->player_id(), MSG_ID_PLAYER_HP_UPDATE,
             reinterpret_cast<const uint8_t*>(data.data()), data.size());

    SPDLOG_INFO("[Combat]Monster {} hit player {} for {} damage, hp={}",
                monster_id, player->player_id(), damage, new_hp);

    if (new_hp <= 0) {
        // 玩家死亡
        nlohmann::json death_notify;
        death_notify["player_id"] = player->player_id();
        std::string death_data = death_notify.dump();
        send_msg(player->player_id(), MSG_ID_PLAYER_DEATH_NOTIFY,
                 reinterpret_cast<const uint8_t*>(death_data.data()), death_data.size());
    }
}

void CombatHandler::distribute_exp(uint64_t killer_id, int32_t exp, Player* player,
                                    SendGameMsgFunc send_msg) {
    if (exp <= 0) return;

    // 查询击杀者是否在队伍中
    std::vector<uint64_t> team_members;
    if (query_team_func_) {
        team_members = query_team_func_(killer_id);
    }

    if (team_members.empty()) {
        // 不在队伍中，击杀者独享经验
        player->add_exp(exp);

        // 发送经验更新
        nlohmann::json exp_update;
        exp_update["exp_gained"] = exp;
        exp_update["total_exp"] = player->get_exp();
        exp_update["level"] = player->get_level();

        std::string data = exp_update.dump();
        send_msg(killer_id, MSG_ID_COMBAT_EXP_UPDATE,
                 reinterpret_cast<const uint8_t*>(data.data()), data.size());

        SPDLOG_INFO("[Combat]Player {} gained {} exp (solo)", killer_id, exp);
    } else {
        // 在队伍中，平均分配经验
        // TODO: 过滤在同一矿洞层的在线成员
        int32_t exp_per_member = exp / static_cast<int32_t>(team_members.size());

        for (uint64_t member_id : team_members) {
            // TODO: 获取成员 Player 对象并添加经验
            // 目前只给击杀者添加经验
            if (member_id == killer_id) {
                player->add_exp(exp_per_member);

                // 发送经验更新
                nlohmann::json exp_update;
                exp_update["exp_gained"] = exp_per_member;
                exp_update["total_exp"] = player->get_exp();
                exp_update["level"] = player->get_level();
                exp_update["shared"] = true;
                exp_update["team_size"] = team_members.size();

                std::string data = exp_update.dump();
                send_msg(killer_id, MSG_ID_COMBAT_EXP_UPDATE,
                         reinterpret_cast<const uint8_t*>(data.data()), data.size());

                SPDLOG_INFO("[Combat]Player {} gained {} exp (team, {} members)",
                            killer_id, exp_per_member, team_members.size());
            }
            // TODO: 给其他在线队员也添加经验
        }
    }
}

}  // namespace farm
