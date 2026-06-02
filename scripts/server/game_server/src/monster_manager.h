#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <ctime>
#include <functional>

namespace farm {

struct MonsterDef {
    std::string id;
    std::string name;
    int32_t hp;
    int32_t attack;
    int32_t defense;
    float speed;
    int32_t exp_reward;
    float aggro_range;
    float attack_range;
    float attack_cooldown;
};

struct ServerMonster {
    uint32_t monster_id;
    std::string monster_type;
    float x;                // 世界坐标（像素）
    float y;
    int32_t hp;
    int32_t max_hp;
    int32_t attack;
    int32_t defense;
    float speed;
    int32_t exp_reward;
    time_t last_attack_time;
    time_t spawn_time;
};

using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;

class ServerMonsterManager {
public:
    ServerMonsterManager();

    // 加载怪物定义
    void load_definitions(const std::string& json_path);

    // 生成怪物
    uint32_t spawn_monster(const std::string& monster_type, float x, float y);

    // 移除怪物
    void remove_monster(uint32_t monster_id);

    // 获取怪物
    ServerMonster* get_monster(uint32_t monster_id);
    const ServerMonster* get_monster(uint32_t monster_id) const;

    // 获取范围内怪物
    std::vector<ServerMonster*> get_monsters_in_range(float x, float y, float range);

    // 更新所有怪物（AI、攻击）
    void update(float dt, uint64_t player_id, float player_x, float player_y,
                SendGameMsgFunc send_msg);

    // 对怪物造成伤害
    int32_t damage_monster(uint32_t monster_id, int32_t damage);

    // 怪物是否死亡
    bool is_monster_dead(uint32_t monster_id) const;

    // 获取怪物数量
    size_t monster_count() const { return monsters_.size(); }

    // 多场景怪物管理
    void set_scene_monsters(const std::string& scene_id,
                           const std::vector<uint32_t>& monster_ids);
    std::vector<uint32_t> get_scene_monster_ids(const std::string& scene_id) const;
    void clear_scene_monsters(const std::string& scene_id);

private:
    std::unordered_map<std::string, MonsterDef> monster_defs_;
    std::unordered_map<uint32_t, ServerMonster> monsters_;
    uint32_t next_monster_id_ = 1;
    std::mt19937 rng_{std::random_device{}()};
    std::unordered_map<std::string, std::vector<uint32_t>> scene_monsters_;
};

}  // namespace farm
