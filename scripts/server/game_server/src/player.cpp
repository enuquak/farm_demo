#include "player.h"

#include <iostream>

namespace farm {

Player::Player(uint64_t player_id, GateSession* gate_session)
    : player_id_(player_id)
    , gate_session_(gate_session)
    , join_time_(std::time(nullptr))
{
}

Player::~Player() {
}

void Player::set_player_data(const PlayerData& data) {
    player_data_ = data;
    dirty_ = true;
}

void Player::set_player_data(PlayerData&& data) {
    player_data_ = std::move(data);
    dirty_ = true;
}

void Player::set_level(int32_t level) {
    if (player_data_.level != level) {
        player_data_.level = level;
        dirty_ = true;
    }
}

void Player::set_gold(int64_t gold) {
    if (player_data_.gold != gold) {
        player_data_.gold = gold;
        dirty_ = true;
    }
}

void Player::set_experience(int64_t experience) {
    if (player_data_.experience != experience) {
        player_data_.experience = experience;
        dirty_ = true;
    }
}

void Player::set_inventory(const std::string& inventory) {
    if (player_data_.inventory != inventory) {
        player_data_.inventory = inventory;
        dirty_ = true;
    }
}

void Player::set_farm_state(const std::string& farm_state) {
    if (player_data_.farm_state != farm_state) {
        player_data_.farm_state = farm_state;
        dirty_ = true;
    }
}

void Player::set_extra_data(const std::string& extra_data) {
    if (player_data_.extra_data != extra_data) {
        player_data_.extra_data = extra_data;
        dirty_ = true;
    }
}

void Player::init_default_data() {
    player_data_.level = 1;
    player_data_.gold = 100;  // 初始金币
    player_data_.experience = 0;
    player_data_.inventory = "{}";  // 空背包
    player_data_.farm_state = "{}";  // 空农场
    player_data_.extra_data = "{}";  // 空扩展数据
    dirty_ = true;

    std::cout << "[Player] Initialized default data for player_id=" << player_id_ << std::endl;
}

}  // namespace farm
