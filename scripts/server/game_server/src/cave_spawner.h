#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <random>
#include <ctime>

namespace farm {

class ServerMonsterManager;

struct SpawnPoint {
    float x;
    float y;
};

struct MonsterDeathRecord {
    uint32_t monster_id;
    time_t death_time;
};

using SendGameMsgFunc = std::function<void(uint64_t, uint32_t, const uint8_t*, size_t)>;

class CaveSpawner {
public:
    CaveSpawner(ServerMonsterManager* monster_mgr);

    void configure(int level, int max_monsters,
                   const std::vector<std::string>& monster_types,
                   const std::vector<SpawnPoint>& spawn_points,
                   float respawn_delay, float spawn_check_interval,
                   float spawn_radius);

    void init_monsters(uint64_t player_id, float player_x, float player_y,
                       SendGameMsgFunc send_msg);

    void update(float dt, uint64_t player_id, float player_x, float player_y,
                SendGameMsgFunc send_msg);

    void on_monster_death(uint32_t monster_id);

    void freeze();
    void thaw();
    bool is_frozen() const { return frozen_; }

    int alive_count() const;

private:
    ServerMonsterManager* monster_mgr_;

    int level_ = 0;
    int max_monsters_ = 0;
    std::vector<std::string> monster_types_;
    std::vector<SpawnPoint> spawn_points_;
    float respawn_delay_ = 10.0f;
    float spawn_check_interval_ = 5.0f;
    float spawn_radius_ = 2.0f;

    std::vector<uint32_t> active_monster_ids_;
    std::vector<MonsterDeathRecord> death_records_;
    float spawn_timer_ = 0.0f;
    bool frozen_ = false;

    std::mt19937 rng_{std::random_device{}()};

    void try_spawn_monster(uint64_t player_id, float player_x, float player_y,
                           SendGameMsgFunc send_msg);
    SpawnPoint select_spawn_point(float player_x, float player_y);
};

}  // namespace farm
