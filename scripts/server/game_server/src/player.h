#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <event2/event.h>
#include "game_constants.h"

namespace farm {

class GateSession;
class DBMgrConnectionManager;

// 玩家数据加载状态
enum class PlayerBizDataState : int32_t {
    NOT_LOADED = 0,   // 未加载
    LOADING = 1,      // 加载中
    LOADED = 2,       // 已加载
    FAILED = 3        // 加载失败
};

// 玩家业务数据
struct PlayerBizData {
    std::string role_name;       // 角色名
    int32_t level = 1;           // 等级
    int64_t gold = 0;            // 金币
    int64_t experience = 0;      // 经验值
    float pos_x = 0.0f;          // 位置 X
    float pos_y = 0.0f;          // 位置 Y
    float pos_z = 0.0f;          // 位置 Z
    int32_t energy = 100;        // 体力值
    std::string scene_id;        // 场景 ID
    std::string inventory;       // 背包数据（JSON 格式）
    std::string farm_state;      // 农场状态（JSON 格式）
    std::string extra_data;      // 扩展数据（JSON 格式）
    int32_t max_hp = 100;        // 最大生命值
    int32_t current_hp = 100;    // 当前生命值
    int32_t attack_power = 0;    // 额外攻击力（装备加成）
    int32_t defense_power = 0;   // 额外防御力（装备加成）
    int32_t combat_exp = 0;      // 战斗经验值
    int32_t combat_level = 1;    // 战斗等级
    std::string equipped_weapon; // 装备的武器ID（JSON）
    std::string task_infos;      // 任务进度数据（JSON 格式）
};

class Player {
public:
    Player(uint64_t player_id, GateSession* gate_session,
           DBMgrConnectionManager* dbmgr_mgr = nullptr);
    ~Player();

    uint64_t player_id() const { return player_id_; }
    GateSession* gate_session() const { return gate_session_; }
    time_t join_time() const { return join_time_; }

    void set_gate_session(GateSession* session) { gate_session_ = session; }
    void set_dbmgr_mgr(DBMgrConnectionManager* mgr) { dbmgr_mgr_ = mgr; }

    // 数据加载状态
    PlayerBizDataState data_state() const { return data_state_; }
    void set_data_state(PlayerBizDataState state) { data_state_ = state; }

    // 数据脏标记
    bool is_dirty() const { return dirty_; }
    void set_dirty(bool dirty) { dirty_ = dirty; }

    // 数据读写接口
    const PlayerBizData& player_data() const { return player_data_; }
    void set_player_data(const PlayerBizData& data);
    void set_player_data(PlayerBizData&& data);

    // 单字段读写接口
    const std::string& get_role_name() const { return player_data_.role_name; }
    void set_role_name(const std::string& role_name);

    int32_t get_level() const { return player_data_.level; }
    void set_level(int32_t level);

    int64_t get_gold() const { return player_data_.gold; }
    void set_gold(int64_t gold);

    int64_t get_experience() const { return player_data_.experience; }
    void set_experience(int64_t experience);
    void add_exp(int64_t exp) { set_experience(player_data_.experience + exp); }
    int64_t get_exp() const { return player_data_.experience; }

    float get_pos_x() const { return player_data_.pos_x; }
    void set_pos_x(float pos_x);

    float get_pos_y() const { return player_data_.pos_y; }
    void set_pos_y(float pos_y);

    float get_pos_z() const { return player_data_.pos_z; }
    void set_pos_z(float pos_z);

    const std::string& get_scene_id() const { return player_data_.scene_id; }
    void set_scene_id(const std::string& scene_id);

    const std::string& get_inventory() const { return player_data_.inventory; }
    void set_inventory(const std::string& inventory);

    int32_t get_energy() const { return player_data_.energy; }
    void set_energy(int32_t energy);

    const std::string& get_farm_state() const { return player_data_.farm_state; }
    void set_farm_state(const std::string& farm_state);

    const std::string& get_extra_data() const { return player_data_.extra_data; }
    void set_extra_data(const std::string& extra_data);

    int32_t get_max_hp() const { return player_data_.max_hp; }
    void set_max_hp(int32_t max_hp);

    int32_t get_current_hp() const { return player_data_.current_hp; }
    void set_current_hp(int32_t current_hp);

    int32_t get_attack_power() const { return player_data_.attack_power; }
    void set_attack_power(int32_t attack_power);

    int32_t get_defense_power() const { return player_data_.defense_power; }
    void set_defense_power(int32_t defense_power);

    int32_t get_combat_exp() const { return player_data_.combat_exp; }
    void set_combat_exp(int32_t combat_exp);

    int32_t get_combat_level() const { return player_data_.combat_level; }
    void set_combat_level(int32_t combat_level);

    const std::string& get_equipped_weapon() const { return player_data_.equipped_weapon; }
    void set_equipped_weapon(const std::string& weapon_id);

    const std::string& get_task_infos() const { return player_data_.task_infos; }
    void set_task_infos(const std::string& task_infos);

    // 初始化默认数据（新玩家）
    void init_default_data();

    // 从 JSON 字符串加载数据（DBMgr 返回的 BSON/JSON 数据）
    // 返回 true 表示解析成功，false 表示解析失败
    bool load_from_json(const std::string& json_str);

    // 存盘接口
    void save();                              // 差量存盘：遍历 dirty_fields_ 发 SET
    void save_field(const std::string& field); // 单字段存盘：发 SET
    void save_full();                         // 全量存盘：发 SET_ALL

    // 自动存盘定时器
    void start_save_timer(struct event_base* base);
    void stop_save_timer();

    // 是否有脏数据
    bool has_dirty_fields() const { return !dirty_fields_.empty(); }
    const std::unordered_set<std::string>& dirty_fields() const { return dirty_fields_; }

private:
    uint64_t player_id_;
    GateSession* gate_session_;
    time_t join_time_;

    // 玩家数据
    PlayerBizData player_data_;
    PlayerBizDataState data_state_ = PlayerBizDataState::NOT_LOADED;
    bool dirty_ = false;
    std::unordered_set<std::string> dirty_fields_;
    DBMgrConnectionManager* dbmgr_mgr_ = nullptr;

    // 自动存盘定时器
    struct event* save_timer_ = nullptr;
    struct event_base* base_ = nullptr;

    void mark_dirty(const std::string& field);
    std::string get_field_json(const std::string& field) const;
    std::string get_all_data_json() const;
    void clear_dirty();

    static void on_save_timer(evutil_socket_t fd, short events, void* ctx);
    void handle_save_timer();
};

}  // namespace farm
