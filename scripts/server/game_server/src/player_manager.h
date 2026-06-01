#pragma once

#include "player.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <functional>
#include <optional>

namespace farm {

class GateSession;
class DBMgrConnectionManager;

// 回调类型定义
using PlayerJoinCallback = std::function<void(uint64_t player_id, bool success, const std::string& msg)>;
using PlayerOfflineCallback = std::function<void(uint64_t player_id)>;

class PlayerManager {
public:
    PlayerManager() = default;
    ~PlayerManager() = default;

    // 设置 DBMgr 连接管理器（用于数据加载/保存）
    void set_dbmgr_manager(DBMgrConnectionManager* dbmgr_mgr) { dbmgr_mgr_ = dbmgr_mgr; }
    DBMgrConnectionManager* dbmgr_mgr() const { return dbmgr_mgr_; }

    void set_event_base(struct event_base* base) { base_ = base; }

    // Set callback for player offline events
    void set_offline_callback(PlayerOfflineCallback callback) {
        offline_callback_ = std::move(callback);
    }

    // 添加玩家（返回 true 成功，false 重复）
    bool add_player(uint64_t player_id, GateSession* gate_session);

    // 添加玩家并加载数据（异步）
    bool add_player_with_data_load(uint64_t player_id, GateSession* gate_session,
                                   PlayerJoinCallback callback);

    // 移除玩家
    void remove_player(uint64_t player_id);

    // 移除玩家并保存数据
    void remove_player_with_save(uint64_t player_id);

    // 查找玩家
    std::optional<Player*> get_player(uint64_t player_id) const;

    // 获取所有玩家
    std::vector<Player*> get_all_players() const;

    // 移除某个 GateSession 关联的所有玩家
    void remove_players_by_gate(GateSession* gate_session);

    // 移除某个 GateSession 关联的所有玩家并保存数据
    void remove_players_by_gate_with_save(GateSession* gate_session);

    // 保存所有玩家数据
    void save_all_players();

    // 当前在线玩家数
    size_t player_count() const;

    // 数据加载回调处理
    void handle_player_data_loaded(uint64_t player_id, int32_t code,
                                   const uint8_t* value_data, size_t value_len);

    // 数据保存回调处理
    void handle_player_data_saved(uint64_t player_id, int32_t code);

private:
    // 保存玩家数据到 DBMgr
    void save_player_data(uint64_t player_id);

    std::unordered_map<uint64_t, std::unique_ptr<Player>> players_;
    DBMgrConnectionManager* dbmgr_mgr_ = nullptr;
    struct event_base* base_ = nullptr;

    // 玩家加入回调（等待数据加载完成）
    std::unordered_map<uint64_t, PlayerJoinCallback> pending_join_callbacks_;

    PlayerOfflineCallback offline_callback_;
};

}  // namespace farm
