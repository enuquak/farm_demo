#include "player.h"

#include "log_macros.h"
#include "dbmgr_connection_manager.h"
#include "dbmgr.pb.h"
#include <nlohmann/json.hpp>
#include <event2/event.h>

namespace farm {

Player::Player(uint64_t player_id, GateSession* gate_session,
               DBMgrConnectionManager* dbmgr_mgr)
    : player_id_(player_id)
    , gate_session_(gate_session)
    , join_time_(std::time(nullptr))
    , dbmgr_mgr_(dbmgr_mgr)
{
}

Player::~Player() {
    stop_save_timer();
}

void Player::mark_dirty(const std::string& field) {
    dirty_ = true;
    dirty_fields_.insert(field);
}

std::string Player::get_all_data_json() const {
    nlohmann::json data;
    data["role_name"] = player_data_.role_name;
    data["level"] = player_data_.level;
    data["gold"] = player_data_.gold;
    data["experience"] = player_data_.experience;
    data["pos_x"] = player_data_.pos_x;
    data["pos_y"] = player_data_.pos_y;
    data["pos_z"] = player_data_.pos_z;
    data["energy"] = player_data_.energy;
    data["scene_id"] = player_data_.scene_id;
    data["inventory"] = player_data_.inventory;
    data["farm_state"] = player_data_.farm_state;
    data["extra_data"] = player_data_.extra_data;
    data["max_hp"] = player_data_.max_hp;
    data["current_hp"] = player_data_.current_hp;
    data["attack_power"] = player_data_.attack_power;
    data["defense_power"] = player_data_.defense_power;
    data["combat_exp"] = player_data_.combat_exp;
    data["combat_level"] = player_data_.combat_level;
    data["equipped_weapon"] = player_data_.equipped_weapon;
    data["task_infos"] = player_data_.task_infos;
    return data.dump();
}

std::string Player::get_field_json(const std::string& field) const {
    nlohmann::json val;
    if (field == "role_name") val = player_data_.role_name;
    else if (field == "level") val = player_data_.level;
    else if (field == "gold") val = player_data_.gold;
    else if (field == "experience") val = player_data_.experience;
    else if (field == "pos_x") val = player_data_.pos_x;
    else if (field == "pos_y") val = player_data_.pos_y;
    else if (field == "pos_z") val = player_data_.pos_z;
    else if (field == "energy") val = player_data_.energy;
    else if (field == "scene_id") val = player_data_.scene_id;
    else if (field == "inventory") {
        // inventory is already a JSON string, parse it first to avoid double-encoding
        try { val = nlohmann::json::parse(player_data_.inventory); }
        catch (...) { val = player_data_.inventory; }
    }
    else if (field == "farm_state") {
        try { val = nlohmann::json::parse(player_data_.farm_state); }
        catch (...) { val = player_data_.farm_state; }
    }
    else if (field == "extra_data") {
        try { val = nlohmann::json::parse(player_data_.extra_data); }
        catch (...) { val = player_data_.extra_data; }
    }
    else if (field == "max_hp") val = player_data_.max_hp;
    else if (field == "current_hp") val = player_data_.current_hp;
    else if (field == "attack_power") val = player_data_.attack_power;
    else if (field == "defense_power") val = player_data_.defense_power;
    else if (field == "combat_exp") val = player_data_.combat_exp;
    else if (field == "combat_level") val = player_data_.combat_level;
    else if (field == "equipped_weapon") val = player_data_.equipped_weapon;
    else if (field == "task_infos") {
        return player_data_.task_infos.empty() ? "{}" : player_data_.task_infos;
    }
    else val = nullptr;
    return val.dump();
}

void Player::clear_dirty() {
    dirty_ = false;
    dirty_fields_.clear();
}

void Player::save_full() {
    if (!dbmgr_mgr_ || !dirty_) return;

    std::string value = get_all_data_json();

    auto callback = [pid = player_id_](int32_t code, const uint8_t*, size_t) {
        if (code == 0) {
            SPDLOG_INFO("[Player]Full save success for player_id={}", pid);
        } else {
            SPDLOG_ERROR("[Player]Full save failed for player_id={} code={}", pid, code);
        }
    };

    uint64_t request_id = dbmgr_mgr_->send_player_data_req(
        player_id_,
        static_cast<int32_t>(farm::PlayerDataOp::SET_ALL),
        "", value, std::move(callback));

    if (request_id == 0) {
        SPDLOG_ERROR("[Player]Failed to send full save for player_id={}", player_id_);
    } else {
        clear_dirty();
    }
}

void Player::save() {
    if (!dbmgr_mgr_ || dirty_fields_.empty()) return;

    auto callback = [pid = player_id_](int32_t code, const uint8_t*, size_t) {
        if (code == 0) {
            SPDLOG_INFO("[Player]Field save success for player_id={}", pid);
        } else {
            SPDLOG_ERROR("[Player]Field save failed for player_id={} code={}", pid, code);
        }
    };

    for (const auto& field : dirty_fields_) {
        std::string value = get_field_json(field);
        uint64_t request_id = dbmgr_mgr_->send_player_data_req(
            player_id_,
            static_cast<int32_t>(farm::PlayerDataOp::SET),
            field, value, callback);
        if (request_id == 0) {
            SPDLOG_ERROR("[Player]Failed to send field save '{}' for player_id={}", field, player_id_);
        }
    }

    clear_dirty();
}

void Player::save_field(const std::string& field) {
    if (!dbmgr_mgr_) return;

    std::string value = get_field_json(field);

    auto callback = [pid = player_id_, field](int32_t code, const uint8_t*, size_t) {
        if (code == 0) {
            SPDLOG_INFO("[Player]Field '{}' saved for player_id={}", field, pid);
        } else {
            SPDLOG_ERROR("[Player]Field '{}' save failed for player_id={} code={}", field, pid, code);
        }
    };

    uint64_t request_id = dbmgr_mgr_->send_player_data_req(
        player_id_,
        static_cast<int32_t>(farm::PlayerDataOp::SET),
        field, value, std::move(callback));

    if (request_id == 0) {
        SPDLOG_ERROR("[Player]Failed to send field '{}' save for player_id={}", field, player_id_);
    } else {
        dirty_fields_.erase(field);
        if (dirty_fields_.empty()) dirty_ = false;
    }
}

void Player::set_player_data(const PlayerBizData& data) {
    player_data_ = data;
    dirty_ = true;
    dirty_fields_ = {"role_name", "level", "gold", "experience",
                     "pos_x", "pos_y", "pos_z", "energy",
                     "scene_id", "inventory", "farm_state", "extra_data",
                     "max_hp", "current_hp", "attack_power", "defense_power",
                     "combat_exp", "combat_level", "equipped_weapon",
                     "task_infos"};
}

void Player::set_player_data(PlayerBizData&& data) {
    player_data_ = std::move(data);
    dirty_ = true;
    dirty_fields_ = {"role_name", "level", "gold", "experience",
                     "pos_x", "pos_y", "pos_z", "energy",
                     "scene_id", "inventory", "farm_state", "extra_data",
                     "max_hp", "current_hp", "attack_power", "defense_power",
                     "combat_exp", "combat_level", "equipped_weapon",
                     "task_infos"};
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

void Player::set_task_infos(const std::string& task_infos) {
    if (player_data_.task_infos != task_infos) {
        player_data_.task_infos = task_infos;
        mark_dirty("task_infos");
    }
}

void Player::set_max_hp(int32_t max_hp) {
    if (player_data_.max_hp != max_hp) {
        player_data_.max_hp = max_hp;
        mark_dirty("max_hp");
    }
}

void Player::set_current_hp(int32_t current_hp) {
    if (player_data_.current_hp != current_hp) {
        player_data_.current_hp = current_hp;
        mark_dirty("current_hp");
    }
}

void Player::set_attack_power(int32_t attack_power) {
    if (player_data_.attack_power != attack_power) {
        player_data_.attack_power = attack_power;
        mark_dirty("attack_power");
    }
}

void Player::set_defense_power(int32_t defense_power) {
    if (player_data_.defense_power != defense_power) {
        player_data_.defense_power = defense_power;
        mark_dirty("defense_power");
    }
}

void Player::set_combat_exp(int32_t combat_exp) {
    if (player_data_.combat_exp != combat_exp) {
        player_data_.combat_exp = combat_exp;
        mark_dirty("combat_exp");
    }
}

void Player::set_combat_level(int32_t combat_level) {
    if (player_data_.combat_level != combat_level) {
        player_data_.combat_level = combat_level;
        mark_dirty("combat_level");
    }
}

void Player::set_equipped_weapon(const std::string& weapon_id) {
    if (player_data_.equipped_weapon != weapon_id) {
        player_data_.equipped_weapon = weapon_id;
        mark_dirty("equipped_weapon");
    }
}

void Player::init_default_data() {
    player_data_.role_name = "";
    player_data_.level = 1;
    player_data_.gold = 100;  // 初始金币
    player_data_.experience = 0;
    player_data_.pos_x = DEFAULT_SPAWN_POS_X;
    player_data_.pos_y = DEFAULT_SPAWN_POS_Y;
    player_data_.pos_z = 0.0f;
    player_data_.scene_id = "farm_main";  // 默认场景
    player_data_.inventory = "{}";  // 空背包
    player_data_.farm_state = "{}";  // 空农场
    player_data_.extra_data = "{}";  // 空扩展数据
    player_data_.task_infos = "{}";  // 空任务数据
    dirty_ = true;
    dirty_fields_ = {"role_name", "level", "gold", "experience",
                     "pos_x", "pos_y", "pos_z", "energy",
                     "scene_id", "inventory", "farm_state", "extra_data",
                     "task_infos"};

    SPDLOG_INFO("[Player]Initialized default data for player_id={}", player_id_);
}

bool Player::load_from_json(const std::string& json_str) {
    try {
        auto json = nlohmann::json::parse(json_str);

        PlayerBizData data;
        data.role_name = json.value("role_name", "");
        data.level = json.value("level", 1);
        data.gold = json.value("gold", int64_t(0));
        data.experience = json.value("experience", int64_t(0));
        data.pos_x = json.value("pos_x", 0.0f);
        data.pos_y = json.value("pos_y", 0.0f);
        data.pos_z = json.value("pos_z", 0.0f);
        data.energy = json.value("energy", 100);
        data.scene_id = json.value("scene_id", "");

        // JSON 字符串字段：对象→dump()，字符串→直接用
        auto load_json_str = [&json](const std::string& key) -> std::string {
            if (json.contains(key)) {
                auto& v = json[key];
                if (v.is_string()) return v.get<std::string>();
                return v.dump();
            }
            return "{}";
        };
        data.inventory = load_json_str("inventory");
        data.farm_state = load_json_str("farm_state");
        data.extra_data = load_json_str("extra_data");
        data.task_infos = load_json_str("task_infos");
        data.equipped_weapon = load_json_str("equipped_weapon");

        // 战斗字段
        data.max_hp = json.value("max_hp", 100);
        data.current_hp = json.value("current_hp", 100);
        data.attack_power = json.value("attack_power", 0);
        data.defense_power = json.value("defense_power", 0);
        data.combat_exp = json.value("combat_exp", 0);
        data.combat_level = json.value("combat_level", 1);

        // 检查关键字段是否存在于 JSON 中
        static const std::vector<std::string> critical_fields = {
            "role_name", "level", "gold", "scene_id"
        };
        for (const auto& field : critical_fields) {
            if (!json.contains(field)) {
                SPDLOG_WARN("[Player]Missing field '{}' in loaded data for player_id={}",
                            field, player_id_);
            }
        }

        set_player_data(std::move(data));
        clear_dirty();  // 加载的数据不是脏数据
        return true;

    } catch (const std::exception& e) {
        SPDLOG_ERROR("[Player]load_from_json failed for player_id={} error={}",
                     player_id_, e.what());
        return false;
    }
}

void Player::start_save_timer(struct event_base* base) {
    if (save_timer_ || !base) return;

    base_ = base;
    save_timer_ = event_new(base_, -1, EV_PERSIST, on_save_timer, this);
    if (!save_timer_) {
        SPDLOG_ERROR("[Player]Failed to create save timer for player_id={}", player_id_);
        return;
    }

    struct timeval tv;
    tv.tv_sec = PLAYER_SAVE_INTERVAL;
    tv.tv_usec = 0;
    evtimer_add(save_timer_, &tv);

    SPDLOG_INFO("[Player]Save timer started for player_id={} interval={}s", player_id_, PLAYER_SAVE_INTERVAL);
}

void Player::stop_save_timer() {
    if (save_timer_) {
        event_del(save_timer_);
        event_free(save_timer_);
        save_timer_ = nullptr;
    }
}

void Player::on_save_timer(evutil_socket_t fd, short events, void* ctx) {
    auto* player = static_cast<Player*>(ctx);
    player->handle_save_timer();
}

void Player::handle_save_timer() {
    if (!dirty_) return;
    SPDLOG_INFO("[Player]Auto-save timer fired for player_id={}", player_id_);
    save_full();
}

}  // namespace farm
