#include "player_manager.h"
#include "gate_session.h"
#include "dbmgr_connection_manager.h"
#include "dbmgr_msg_ids.h"

#include <cstring>

#include "dbmgr.pb.h"
#include "player.pb.h"
#include "log_macros.h"

namespace farm {

bool PlayerManager::add_player(uint64_t player_id, GateSession* gate_session) {
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        // 玩家已存在（重复加入）
        return false;
    }
    players_[player_id] = std::make_unique<Player>(player_id, gate_session);
    return true;
}

bool PlayerManager::add_player_with_data_load(uint64_t player_id, GateSession* gate_session,
                                               PlayerJoinCallback callback) {
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        // 玩家已存在（重复加入）
        return false;
    }

    // 创建玩家对象
    auto player = std::make_unique<Player>(player_id, gate_session);
    player->set_data_state(PlayerBizDataState::LOADING);

    // 存储回调
    if (callback) {
        pending_join_callbacks_[player_id] = std::move(callback);
    }

    // 从 DBMgr 加载数据
    if (dbmgr_mgr_) {
        auto load_callback = [this, player_id](int32_t code, const uint8_t* value_data, size_t value_len) {
            handle_player_data_loaded(player_id, code, value_data, value_len);
        };

        uint64_t request_id = dbmgr_mgr_->send_player_data_req(
            player_id,
            static_cast<int32_t>(farm::PlayerDataOp::GET_ALL),
            "",  // key 为空，GET_ALL 不需要 key
            "",  // value 为空
            std::move(load_callback));

        if (request_id == 0) {
            // 发送失败
            SPDLOG_ERROR("[Player]Failed to send data load request for player_id={}", player_id);
            player->set_data_state(PlayerBizDataState::FAILED);

            // 调用回调通知失败
            if (callback) {
                callback(player_id, false, "Failed to send data load request");
                pending_join_callbacks_.erase(player_id);
            }

            // 仍然添加玩家，但数据状态为失败
            players_[player_id] = std::move(player);
            return true;
        }

        SPDLOG_INFO("[Player]Data load request sent for player_id={} request_id={}", player_id, request_id);
    } else {
        // 没有 DBMgr 连接，直接使用默认数据
        SPDLOG_INFO("[Player]No DBMgr manager, using default data for player_id={}", player_id);
        player->init_default_data();
        player->set_data_state(PlayerBizDataState::LOADED);

        // 调用回调通知成功
        if (callback) {
            callback(player_id, true, "Data loaded (default)");
            pending_join_callbacks_.erase(player_id);
        }
    }

    players_[player_id] = std::move(player);
    return true;
}

void PlayerManager::remove_player(uint64_t player_id) {
    players_.erase(player_id);
}

void PlayerManager::remove_player_with_save(uint64_t player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return;
    }

    // 保存数据
    save_player_data(player_id);

    // 移除玩家
    players_.erase(it);
}

Player* PlayerManager::get_player(uint64_t player_id) const {
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<Player*> PlayerManager::get_all_players() const {
    std::vector<Player*> result;
    result.reserve(players_.size());
    for (auto& kv : players_) {
        result.push_back(kv.second.get());
    }
    return result;
}

void PlayerManager::remove_players_by_gate(GateSession* gate_session) {
    std::vector<uint64_t> to_remove;
    for (auto& kv : players_) {
        if (kv.second->gate_session() == gate_session) {
            to_remove.push_back(kv.first);
        }
    }
    for (uint64_t pid : to_remove) {
        SPDLOG_INFO("[Player]Removing player {} due to Gate disconnect", pid);
        players_.erase(pid);
    }
}

void PlayerManager::remove_players_by_gate_with_save(GateSession* gate_session) {
    std::vector<uint64_t> to_remove;
    for (auto& kv : players_) {
        if (kv.second->gate_session() == gate_session) {
            to_remove.push_back(kv.first);
        }
    }
    for (uint64_t pid : to_remove) {
        SPDLOG_INFO("[Player]Removing player {} due to Gate disconnect (with save)", pid);
        save_player_data(pid);
        players_.erase(pid);
    }
}

size_t PlayerManager::player_count() const {
    return players_.size();
}

void PlayerManager::handle_player_data_loaded(uint64_t player_id, int32_t code,
                                               const uint8_t* value_data, size_t value_len) {
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        SPDLOG_INFO("[Player]Player not found for data loaded callback, player_id={}", player_id);
        return;
    }

    Player* player = it->second.get();

    if (code == 0 && value_data && value_len > 0) {
        // 数据加载成功，解析 PlayerData protobuf
        std::string data_str(reinterpret_cast<const char*>(value_data), value_len);
        farm::PlayerData player_data;
        if (player_data.ParseFromString(data_str)) {
            // 将 protobuf 数据转换为 PlayerBizData
            PlayerBizData data;
            data.role_name = player_data.role_name();
            data.level = player_data.level();
            data.gold = 0;  // PlayerData 中没有 gold 字段，使用默认值
            data.experience = player_data.exp();
            data.pos_x = player_data.pos_x();
            data.pos_y = player_data.pos_y();
            data.pos_z = player_data.pos_z();
            data.scene_id = player_data.scene_id();
            data.inventory = "{}";
            data.farm_state = "{}";
            data.extra_data = "{}";

            player->set_player_data(std::move(data));
            player->set_data_state(PlayerBizDataState::LOADED);

            SPDLOG_INFO("[Player]Data loaded successfully for player_id={} role_name={} level={} scene_id={}", player_id, player_data.role_name(), player_data.level(), player_data.scene_id());
        } else {
            // protobuf 解析失败，使用默认数据
            SPDLOG_ERROR("[Player]Failed to parse PlayerData proto for player_id={}", player_id);
            player->init_default_data();
            player->set_data_state(PlayerBizDataState::LOADED);
        }
    } else if (code == 0 && (!value_data || value_len == 0)) {
        // 数据为空（新玩家），初始化默认数据
        SPDLOG_INFO("[Player]No data found for player_id={}, initializing default data", player_id);

        player->init_default_data();
        player->set_data_state(PlayerBizDataState::LOADED);

        // 保存默认数据到 DBMgr
        save_player_data(player_id);
    } else {
        // 数据加载失败
        SPDLOG_ERROR("[Player]Data load failed for player_id={} code={}", player_id, code);

        player->set_data_state(PlayerBizDataState::FAILED);

        // 使用默认数据作为降级策略
        player->init_default_data();
        player->set_data_state(PlayerBizDataState::LOADED);
    }

    // 调用玩家加入回调
    auto callback_it = pending_join_callbacks_.find(player_id);
    if (callback_it != pending_join_callbacks_.end()) {
        if (callback_it->second) {
            callback_it->second(player_id, true, "Data loaded");
        }
        pending_join_callbacks_.erase(callback_it);
    }
}

void PlayerManager::handle_player_data_saved(uint64_t player_id, int32_t code) {
    if (code == 0) {
        SPDLOG_INFO("[Player]Data saved successfully for player_id={}", player_id);
    } else {
        SPDLOG_ERROR("[Player]Data save failed for player_id={} code={}", player_id, code);
    }
}

void PlayerManager::save_player_data(uint64_t player_id) {
    if (!dbmgr_mgr_) {
        SPDLOG_INFO("[Player]No DBMgr manager, skipping data save for player_id={}", player_id);
        return;
    }

    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return;
    }

    Player* player = it->second.get();

    // 检查数据是否需要保存
    if (!player->is_dirty()) {
        SPDLOG_INFO("[Player]Data not dirty, skipping save for player_id={}", player_id);
        return;
    }

    // 使用 protobuf 序列化玩家数据
    const PlayerBizData& data = player->player_data();
    farm::PlayerData player_data;
    player_data.set_player_id(player_id);
    player_data.set_role_name(data.role_name);
    player_data.set_level(data.level);
    player_data.set_exp(data.experience);
    player_data.set_pos_x(data.pos_x);
    player_data.set_pos_y(data.pos_y);
    player_data.set_pos_z(data.pos_z);
    player_data.set_scene_id(data.scene_id);
    player_data.set_created_at(static_cast<uint64_t>(std::time(nullptr)));

    std::string value;
    player_data.SerializeToString(&value);

    auto save_callback = [this, player_id](int32_t code, const uint8_t* /*value_data*/, size_t /*value_len*/) {
        handle_player_data_saved(player_id, code);
    };

    uint64_t request_id = dbmgr_mgr_->send_player_data_req(
        player_id,
        static_cast<int32_t>(farm::PlayerDataOp::SET_ALL),
        "",  // key 为空，SET_ALL 不需要 key
        value,
        std::move(save_callback));

    if (request_id == 0) {
        SPDLOG_ERROR("[Player]Failed to send data save request for player_id={}", player_id);
    } else {
        SPDLOG_INFO("[Player]Data save request sent for player_id={} request_id={}", player_id, request_id);
        player->set_dirty(false);
    }
}

void PlayerManager::save_all_players() {
    SPDLOG_INFO("[Player]Saving all players data...");

    for (auto& kv : players_) {
        save_player_data(kv.first);
    }

    SPDLOG_INFO("[Player]All players data save requests sent");
}

}  // namespace farm
