#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <unordered_map>

namespace farm {

class GateSession;

// 玩家数据加载状态
enum class PlayerDataState : int32_t {
    NOT_LOADED = 0,   // 未加载
    LOADING = 1,      // 加载中
    LOADED = 2,       // 已加载
    FAILED = 3        // 加载失败
};

// 玩家业务数据
struct PlayerData {
    int32_t level = 1;           // 等级
    int64_t gold = 0;            // 金币
    int64_t experience = 0;      // 经验值
    std::string inventory;       // 背包数据（JSON 格式）
    std::string farm_state;      // 农场状态（JSON 格式）
    std::string extra_data;      // 扩展数据（JSON 格式）
};

class Player {
public:
    Player(uint64_t player_id, GateSession* gate_session);
    ~Player();

    uint64_t player_id() const { return player_id_; }
    GateSession* gate_session() const { return gate_session_; }
    time_t join_time() const { return join_time_; }

    void set_gate_session(GateSession* session) { gate_session_ = session; }

    // 数据加载状态
    PlayerDataState data_state() const { return data_state_; }
    void set_data_state(PlayerDataState state) { data_state_ = state; }

    // 数据脏标记
    bool is_dirty() const { return dirty_; }
    void set_dirty(bool dirty) { dirty_ = dirty; }

    // 数据读写接口
    const PlayerData& player_data() const { return player_data_; }
    void set_player_data(const PlayerData& data);
    void set_player_data(PlayerData&& data);

    // 单字段读写接口
    int32_t get_level() const { return player_data_.level; }
    void set_level(int32_t level);

    int64_t get_gold() const { return player_data_.gold; }
    void set_gold(int64_t gold);

    int64_t get_experience() const { return player_data_.experience; }
    void set_experience(int64_t experience);

    const std::string& get_inventory() const { return player_data_.inventory; }
    void set_inventory(const std::string& inventory);

    const std::string& get_farm_state() const { return player_data_.farm_state; }
    void set_farm_state(const std::string& farm_state);

    const std::string& get_extra_data() const { return player_data_.extra_data; }
    void set_extra_data(const std::string& extra_data);

    // 初始化默认数据（新玩家）
    void init_default_data();

private:
    uint64_t player_id_;
    GateSession* gate_session_;
    time_t join_time_;

    // 玩家数据
    PlayerData player_data_;
    PlayerDataState data_state_ = PlayerDataState::NOT_LOADED;
    bool dirty_ = false;
};

}  // namespace farm
