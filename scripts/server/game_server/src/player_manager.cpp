#include "player_manager.h"
#include "gate_session.h"
#include "dbmgr_connection_manager.h"
#include "internal_msg_ids.h"

#include <cstring>

#include "dbmgr.pb.h"
#include "log_macros.h"

namespace farm {

bool PlayerManager::add_player(uint64_t player_id, GateSession* gate_session) {
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        return false;
    }
    // Note: timer not started here — data state is NOT_LOADED.
    // Timer starts after data loads (in handle_player_data_loaded).
    players_[player_id] = std::make_unique<Player>(player_id, gate_session, dbmgr_mgr_);
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
    auto player = std::make_unique<Player>(player_id, gate_session, dbmgr_mgr_);
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
    auto it = players_.find(player_id);
    if (it == players_.end()) return;

    it->second->stop_save_timer();
    if (offline_callback_) {
        offline_callback_(player_id);
    }
    players_.erase(it);
}

void PlayerManager::remove_player_with_save(uint64_t player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;

    it->second->stop_save_timer();
    it->second->save_full();

    if (offline_callback_) {
        offline_callback_(player_id);
    }
    players_.erase(it);
}

std::optional<Player*> PlayerManager::get_player(uint64_t player_id) const {
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        return it->second.get();
    }
    return std::nullopt;
}

std::vector<Player*> PlayerManager::get_all_players() const {
    std::vector<Player*> result;
    result.reserve(players_.size());
    for (auto& [player_id, player] : players_) {
        result.push_back(player.get());
    }
    return result;
}

void PlayerManager::remove_players_by_gate(GateSession* gate_session) {
    std::vector<uint64_t> to_remove;
    for (auto& [player_id, player] : players_) {
        if (player->gate_session() == gate_session) {
            to_remove.push_back(player_id);
        }
    }
    for (uint64_t pid : to_remove) {
        SPDLOG_INFO("[Player]Removing player {} due to Gate disconnect", pid);
        auto it = players_.find(pid);
        if (it != players_.end()) {
            it->second->stop_save_timer();
            if (offline_callback_) {
                offline_callback_(pid);
            }
            players_.erase(it);
        }
    }
}

void PlayerManager::remove_players_by_gate_with_save(GateSession* gate_session) {
    std::vector<uint64_t> to_remove;
    for (auto& [player_id, player] : players_) {
        if (player->gate_session() == gate_session) {
            to_remove.push_back(player_id);
        }
    }
    for (uint64_t pid : to_remove) {
        SPDLOG_INFO("[Player]Removing player {} due to Gate disconnect (with save)", pid);
        auto it = players_.find(pid);
        if (it != players_.end()) {
            it->second->stop_save_timer();
            it->second->save_full();
            if (offline_callback_) {
                offline_callback_(pid);
            }
            players_.erase(it);
        }
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
        // 数据加载成功，从 JSON 解析
        std::string data_str(reinterpret_cast<const char*>(value_data), value_len);

        if (player->load_from_json(data_str)) {
            player->set_data_state(PlayerBizDataState::LOADED);
            player->start_save_timer(base_);
            SPDLOG_INFO("[Player]Data loaded from DB for player_id={}", player_id);
        } else {
            // JSON 解析失败，数据可能损坏
            SPDLOG_ERROR("[Player]Corrupted data for player_id={}, falling back to defaults", player_id);
            player->init_default_data();
            player->set_data_state(PlayerBizDataState::LOADED);
            player->start_save_timer(base_);
            // 立即保存默认数据，防止下次加载仍然读到损坏数据
            player->save_full();
        }
    } else if (code == 0 && (!value_data || value_len == 0)) {
        // 数据为空（新玩家），初始化默认数据
        SPDLOG_INFO("[Player]No data found for player_id={}, initializing default data", player_id);

        player->init_default_data();
        player->set_data_state(PlayerBizDataState::LOADED);
        player->start_save_timer(base_);

        // 保存默认数据到 DBMgr
        save_player_data(player_id);
    } else {
        // 数据加载失败
        SPDLOG_ERROR("[Player]Data load failed for player_id={} code={}", player_id, code);

        // 使用默认数据作为降级策略（不启动自动保存定时器，避免用默认数据覆盖真实数据）
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

void PlayerManager::save_player_data(uint64_t player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;
    it->second->save_full();
}

void PlayerManager::save_all_players() {
    SPDLOG_INFO("[Player]Saving all players data...");
    for (auto& [player_id, player] : players_) {
        player->save_full();
    }
    SPDLOG_INFO("[Player]All players data save requests sent");
}

}  // namespace farm
