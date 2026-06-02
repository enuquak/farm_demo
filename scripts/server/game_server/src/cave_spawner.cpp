#include "cave_spawner.h"
#include "monster_manager.h"
#include "message_ids.h"
#include "log_macros.h"

#include <nlohmann/json.hpp>
#include <ctime>
#include <cmath>
#include <algorithm>

namespace farm {

CaveSpawner::CaveSpawner(ServerMonsterManager* monster_mgr)
    : monster_mgr_(monster_mgr) {}

void CaveSpawner::configure(int level, int max_monsters,
                            const std::vector<std::string>& monster_types,
                            const std::vector<SpawnPoint>& spawn_points,
                            float respawn_delay, float spawn_check_interval,
                            float spawn_radius) {
    level_ = level;
    max_monsters_ = max_monsters;
    monster_types_ = monster_types;
    spawn_points_ = spawn_points;
    respawn_delay_ = respawn_delay;
    spawn_check_interval_ = spawn_check_interval;
    spawn_radius_ = spawn_radius;
}

void CaveSpawner::init_monsters(uint64_t player_id, float player_x, float player_y,
                                SendGameMsgFunc send_msg) {
    active_monster_ids_.clear();
    death_records_.clear();
    spawn_timer_ = 0.0f;

    int initial_count = std::min(max_monsters_, static_cast<int>(spawn_points_.size()));
    for (int i = 0; i < initial_count; ++i) {
        if (monster_types_.empty()) break;

        const auto& sp = spawn_points_[i % spawn_points_.size()];
        const auto& type = monster_types_[i % monster_types_.size()];

        uint32_t id = monster_mgr_->spawn_monster(type, sp.x * 16.0f, sp.y * 16.0f);
        if (id > 0) {
            active_monster_ids_.push_back(id);

            const auto* m = monster_mgr_->get_monster(id);
            if (m) {
                nlohmann::json notify;
                notify["monster_id"] = id;
                notify["monster_type"] = m->monster_type;
                notify["x"] = m->x;
                notify["y"] = m->y;
                notify["hp"] = m->hp;
                notify["max_hp"] = m->max_hp;

                std::string data = notify.dump();
                send_msg(player_id, MSG_ID_MONSTER_SPAWN_NOTIFY,
                         reinterpret_cast<const uint8_t*>(data.data()), data.size());
            }
        }
    }

    SPDLOG_INFO("[CaveSpawner]Initialized level {} with {} monsters",
                level_, active_monster_ids_.size());
}

void CaveSpawner::update(float dt, uint64_t player_id, float player_x, float player_y,
                         SendGameMsgFunc send_msg) {
    if (frozen_) return;

    spawn_timer_ += dt;
    if (spawn_timer_ < spawn_check_interval_) return;
    spawn_timer_ = 0.0f;

    active_monster_ids_.erase(
        std::remove_if(active_monster_ids_.begin(), active_monster_ids_.end(),
                       [this](uint32_t id) { return monster_mgr_->is_monster_dead(id); }),
        active_monster_ids_.end());

    try_spawn_monster(player_id, player_x, player_y, send_msg);
}

void CaveSpawner::on_monster_death(uint32_t monster_id) {
    death_records_.push_back({monster_id, std::time(nullptr)});
    SPDLOG_INFO("[CaveSpawner]Monster {} died in level {}", monster_id, level_);
}

void CaveSpawner::freeze() {
    frozen_ = true;
    SPDLOG_INFO("[CaveSpawner]Level {} frozen", level_);
}

void CaveSpawner::thaw() {
    frozen_ = false;
    spawn_timer_ = 0.0f;
    SPDLOG_INFO("[CaveSpawner]Level {} thawed", level_);
}

int CaveSpawner::alive_count() const {
    int count = 0;
    for (uint32_t id : active_monster_ids_) {
        if (!monster_mgr_->is_monster_dead(id)) {
            ++count;
        }
    }
    return count;
}

void CaveSpawner::try_spawn_monster(uint64_t player_id, float player_x, float player_y,
                                    SendGameMsgFunc send_msg) {
    if (alive_count() >= max_monsters_) return;
    if (monster_types_.empty() || spawn_points_.empty()) return;

    time_t now = std::time(nullptr);
    bool can_spawn = false;
    for (auto it = death_records_.begin(); it != death_records_.end(); ++it) {
        if (difftime(now, it->death_time) >= respawn_delay_) {
            death_records_.erase(it);
            can_spawn = true;
            break;
        }
    }

    if (!can_spawn) return;

    SpawnPoint sp = select_spawn_point(player_x, player_y);

    std::uniform_int_distribution<size_t> type_dist(0, monster_types_.size() - 1);
    const auto& type = monster_types_[type_dist(rng_)];

    uint32_t id = monster_mgr_->spawn_monster(type, sp.x * 16.0f, sp.y * 16.0f);
    if (id > 0) {
        active_monster_ids_.push_back(id);

        const auto* m = monster_mgr_->get_monster(id);
        if (m) {
            nlohmann::json notify;
            notify["monster_id"] = id;
            notify["monster_type"] = m->monster_type;
            notify["x"] = m->x;
            notify["y"] = m->y;
            notify["hp"] = m->hp;
            notify["max_hp"] = m->max_hp;

            std::string data = notify.dump();
            send_msg(player_id, MSG_ID_MONSTER_SPAWN_NOTIFY,
                     reinterpret_cast<const uint8_t*>(data.data()), data.size());

            SPDLOG_INFO("[CaveSpawner]Spawned monster {} type={} at ({},{})",
                        id, type, sp.x, sp.y);
        }
    }
}

SpawnPoint CaveSpawner::select_spawn_point(float player_x, float player_y) {
    float max_dist = 0;
    SpawnPoint best = spawn_points_[0];

    for (const auto& sp : spawn_points_) {
        float dx = sp.x * 16.0f - player_x;
        float dy = sp.y * 16.0f - player_y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > max_dist) {
            max_dist = dist;
            best = sp;
        }
    }

    std::uniform_real_distribution<float> offset(-spawn_radius_, spawn_radius_);
    best.x += offset(rng_);
    best.y += offset(rng_);

    return best;
}

}  // namespace farm
