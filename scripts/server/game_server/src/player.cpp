#include "player.h"

#include "log_macros.h"

namespace farm {

Player::Player(uint64_t player_id, GateSession* gate_session)
    : player_id_(player_id)
    , gate_session_(gate_session)
    , join_time_(std::time(nullptr))
{
}

Player::~Player() {
}

void Player::mark_dirty(const std::string& field) {
    dirty_ = true;
    dirty_fields_.insert(field);
}

void Player::set_player_data(const PlayerBizData& data) {
    player_data_ = data;
    dirty_ = true;
    dirty_fields_ = {"role_name", "level", "gold", "experience",
                     "pos_x", "pos_y", "pos_z", "energy",
                     "scene_id", "inventory", "farm_state", "extra_data"};
}

void Player::set_player_data(PlayerBizData&& data) {
    player_data_ = std::move(data);
    dirty_ = true;
    dirty_fields_ = {"role_name", "level", "gold", "experience",
                     "pos_x", "pos_y", "pos_z", "energy",
                     "scene_id", "inventory", "farm_state", "extra_data"};
}

void Player::set_role_name(const std::string& role_name) {
    if (player_data_.role_name != role_name) {
        player_data_.role_name = role_name;
        mark_dirty("role_name");
    }
}

void Player::set_level(int32_t level) {
    if (player_data_.level != level) {
        player_data_.level = level;
        mark_dirty("level");
    }
}

void Player::set_gold(int64_t gold) {
    if (player_data_.gold != gold) {
        player_data_.gold = gold;
        mark_dirty("gold");
    }
}

void Player::set_experience(int64_t experience) {
    if (player_data_.experience != experience) {
        player_data_.experience = experience;
        mark_dirty("experience");
    }
}

void Player::set_pos_x(float pos_x) {
    if (player_data_.pos_x != pos_x) {
        player_data_.pos_x = pos_x;
        mark_dirty("pos_x");
    }
}

void Player::set_pos_y(float pos_y) {
    if (player_data_.pos_y != pos_y) {
        player_data_.pos_y = pos_y;
        mark_dirty("pos_y");
    }
}

void Player::set_pos_z(float pos_z) {
    if (player_data_.pos_z != pos_z) {
        player_data_.pos_z = pos_z;
        mark_dirty("pos_z");
    }
}

void Player::set_scene_id(const std::string& scene_id) {
    if (player_data_.scene_id != scene_id) {
        player_data_.scene_id = scene_id;
        mark_dirty("scene_id");
    }
}

void Player::set_inventory(const std::string& inventory) {
    if (player_data_.inventory != inventory) {
        player_data_.inventory = inventory;
        mark_dirty("inventory");
    }
}

void Player::set_energy(int32_t energy) {
    if (player_data_.energy != energy) {
        player_data_.energy = energy;
        mark_dirty("energy");
    }
}

void Player::set_farm_state(const std::string& farm_state) {
    if (player_data_.farm_state != farm_state) {
        player_data_.farm_state = farm_state;
        mark_dirty("farm_state");
    }
}

void Player::set_extra_data(const std::string& extra_data) {
    if (player_data_.extra_data != extra_data) {
        player_data_.extra_data = extra_data;
        mark_dirty("extra_data");
    }
}

void Player::init_default_data() {
    player_data_.role_name = "";
    player_data_.level = 1;
    player_data_.gold = 100;  // 初始金币
    player_data_.experience = 0;
    player_data_.pos_x = 0.0f;
    player_data_.pos_y = 0.0f;
    player_data_.pos_z = 0.0f;
    player_data_.scene_id = "farm_main";  // 默认场景
    player_data_.inventory = "{}";  // 空背包
    player_data_.farm_state = "{}";  // 空农场
    player_data_.extra_data = "{}";  // 空扩展数据
    dirty_ = true;
    dirty_fields_ = {"role_name", "level", "gold", "experience",
                     "pos_x", "pos_y", "pos_z", "energy",
                     "scene_id", "inventory", "farm_state", "extra_data"};

    SPDLOG_INFO("[Player]Initialized default data for player_id={}", player_id_);
}

}  // namespace farm
