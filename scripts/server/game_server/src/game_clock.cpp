#include "game_clock.h"
#include "game_scene_manager.h"
#include "player_manager.h"
#include "dbmgr_connection_manager.h"
#include "player.h"
#include "msg_ids.h"
#include "game_constants.h"
#include "log_macros.h"

#include "player.pb.h"
#include "dbmgr.pb.h"

#include <nlohmann/json.hpp>
#include <algorithm>

namespace farm {

// Reserved player_id for system/scene data persistence via DBMgr
static constexpr uint64_t SCENE_DATA_PLAYER_ID = 0;

GameClock::GameClock(PlayerManager* player_mgr,
                     GameSceneManager* scene_mgr,
                     DBMgrConnectionManager* dbmgr_mgr,
                     SendGameMsgFunc send_game_msg)
    : player_mgr_(player_mgr)
    , scene_mgr_(scene_mgr)
    , dbmgr_mgr_(dbmgr_mgr)
    , send_game_msg_(std::move(send_game_msg))
{
}

void GameClock::save_clock_data() {
    if (!dbmgr_mgr_->is_connected(0)) {
        SPDLOG_INFO("[Game]No DBMgr connected, skipping clock data save");
        return;
    }

    nlohmann::json clock_json;
    clock_json["day"] = clock_day_;
    clock_json["time_slot"] = clock_time_slot_;
    clock_json["elapsed"] = clock_elapsed_;

    std::string value = clock_json.dump();
    std::string key = "server:game_clock";

    auto callback = [](int32_t code, const uint8_t* /*data*/, size_t /*len*/) {
        if (code == 0) {
            SPDLOG_INFO("[Game]Clock data saved to DBMgr");
        } else {
            SPDLOG_ERROR("[Game]Clock data save failed: code={}", code);
        }
    };

    dbmgr_mgr_->send_player_data_req(
        SCENE_DATA_PLAYER_ID,
        static_cast<int32_t>(farm::PlayerDataOp::SET),
        key, value,
        std::move(callback));
}

void GameClock::load_clock_data() {
    if (!dbmgr_mgr_->is_connected(0)) {
        SPDLOG_INFO("[Game]No DBMgr connected, cannot load clock data");
        return;
    }

    std::string key = "server:game_clock";

    auto callback = [this](int32_t code, const uint8_t* value_data, size_t value_len) {
        if (code != 0 || !value_data || value_len == 0) {
            SPDLOG_INFO("[Game]No saved clock data, using defaults (day=1, slot=0)");
            return;
        }

        try {
            std::string json_str(reinterpret_cast<const char*>(value_data), value_len);
            nlohmann::json clock_json = nlohmann::json::parse(json_str);

            clock_day_ = clock_json.value("day", 1);
            clock_time_slot_ = clock_json.value("time_slot", 0);
            clock_elapsed_ = clock_json.value("elapsed", 0.0);

            SPDLOG_INFO("[Game]Loaded clock data from DBMgr: day={}, slot={}, elapsed={}",
                        clock_day_, clock_time_slot_, clock_elapsed_);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[Game]Error loading clock data: {}", e.what());
        }
    };

    dbmgr_mgr_->send_player_data_req(
        SCENE_DATA_PLAYER_ID,
        static_cast<int32_t>(farm::PlayerDataOp::GET),
        key, "",
        std::move(callback));
}

void GameClock::update() {
    // Paused state: do not advance clock
    if (clock_paused_) {
        // But still check force sleep timeout
        if (force_sleep_pending_) {
            force_sleep_timeout_counter_++;
            if (force_sleep_timeout_counter_ >= 10) {
                SPDLOG_WARN("[Game]Force sleep timeout (10s), forcing completion");
                force_sleep_pending_ = false;
                force_sleep_timeout_counter_ = 0;

                // Force complete: switch scene + restore energy + advance day
                auto all_players = player_mgr_->get_all_players();
                for (Player* player : all_players) {
                    if (!player || player->data_state() != PlayerBizDataState::LOADED) continue;
                    complete_force_sleep(player);
                    SPDLOG_INFO("[Game]Force sleep timeout: player={} switched to house, day={}", player->player_id(), clock_day_);
                }
            }
        }
        return;
    }

    // Advance clock (update_game_logic calls once per second)
    clock_elapsed_ += 1.0;

    // Check if a slot should advance (60 seconds = 1 slot)
    while (clock_elapsed_ >= 60.0) {
        clock_elapsed_ -= 60.0;
        clock_time_slot_++;

        if (clock_time_slot_ >= 40) {
            // Day ended
            SPDLOG_INFO("[Game]Day {} ended (slot={})", clock_day_, clock_time_slot_);
            on_day_end();
            return;
        } else {
            SPDLOG_INFO("[Game]New slot: day={}, slot={}", clock_day_, clock_time_slot_);
            broadcast_clock_sync();
        }
    }
}

void GameClock::broadcast_clock_sync() {
    farm::ClockSync sync;
    sync.set_day(clock_day_);
    sync.set_time_slot(clock_time_slot_);
    sync.set_paused(clock_paused_);

    std::string sync_data;
    sync.SerializeToString(&sync_data);

    // Send to all online players
    auto all_players = player_mgr_->get_all_players();
    for (Player* player : all_players) {
        if (!player || player->data_state() != PlayerBizDataState::LOADED) continue;
        send_game_msg_(player->player_id(), MSG_ID_CLOCK_SYNC,
                       reinterpret_cast<const uint8_t*>(sync_data.data()),
                       sync_data.size());
    }

    SPDLOG_DEBUG("[Game]ClockSync broadcast: day={}, slot={}, paused={}",
                 clock_day_, clock_time_slot_, clock_paused_);
}

void GameClock::on_day_end() {
    // Pause clock
    clock_paused_ = true;
    force_sleep_pending_ = true;
    force_sleep_timeout_counter_ = 0;

    // Send ForceSleepNotify to all online players
    farm::ForceSleepNotify notify;
    notify.set_day(clock_day_);

    std::string notify_data;
    notify.SerializeToString(&notify_data);

    auto all_players = player_mgr_->get_all_players();
    for (Player* player : all_players) {
        if (!player || player->data_state() != PlayerBizDataState::LOADED) continue;
        send_game_msg_(player->player_id(), MSG_ID_FORCE_SLEEP_NOTIFY,
                       reinterpret_cast<const uint8_t*>(notify_data.data()),
                       notify_data.size());
    }

    SPDLOG_INFO("[Game]Force sleep initiated: day={}, waiting for client ready", clock_day_);
}

void GameClock::handle_force_sleep_ready(uint64_t player_id,
                                          const uint8_t* payload, size_t payload_len) {
    farm::ForceSleepReady req;
    if (payload_len > 0 && !req.ParseFromArray(payload, static_cast<int>(payload_len))) {
        SPDLOG_ERROR("[Game]Failed to parse ForceSleepReady");
        return;
    }

    SPDLOG_INFO("[Game]ForceSleepReady received: player_id={}, day={}", player_id, req.day());

    if (!force_sleep_pending_) {
        SPDLOG_WARN("[Game]ForceSleepReady but no force sleep pending, ignoring");
        return;
    }

    // Clear force sleep state
    force_sleep_pending_ = false;
    force_sleep_timeout_counter_ = 0;

    // Get player
    Player* player = player_mgr_->get_player(player_id).value_or(nullptr);
    if (!player) {
        SPDLOG_ERROR("[Game]ForceSleepReady: player_id={} not found", player_id);
        return;
    }

    complete_force_sleep(player);
}

void GameClock::complete_force_sleep(Player* player) {
    uint64_t player_id = player->player_id();

    // 1. Freeze current scene
    const std::string& current_scene = player->get_scene_id();
    if (!current_scene.empty()) {
        SceneState* scene = scene_mgr_->get_scene(current_scene);
        if (scene) {
            scene->remove_player();
        }
    }

    // 2. Switch to house scene
    SceneState* house = scene_mgr_->get_or_create_scene("house");
    if (house->is_frozen()) house->thaw();
    house->add_player();

    // 3. Set player position near BED
    player->set_scene_id("house");
    player->set_pos_x(5.0f * 16);
    player->set_pos_y(6.0f * 16);
    player->set_dirty(true);

    // 4. Restore 50% of max energy
    int32_t current_energy = player->get_energy();
    int32_t max_energy = farm::MAX_ENERGY;
    int32_t restore = max_energy / 2;
    player->set_energy(std::min(current_energy + restore, max_energy));

    // 5. Advance day
    clock_day_++;
    clock_time_slot_ = 0;
    clock_elapsed_ = 0.0;

    // 6. Resume clock
    clock_paused_ = false;

    // 7. Send SceneChangeResp
    farm::SceneChangeResp scene_resp;
    scene_resp.set_code(0);
    scene_resp.set_msg("force sleep");
    scene_resp.set_target_scene("house");
    scene_resp.set_spawn_x(5);
    scene_resp.set_spawn_y(6);
    scene_resp.set_active_scene("house");

    std::string scene_resp_data;
    scene_resp.SerializeToString(&scene_resp_data);
    send_game_msg_(player_id, MSG_ID_SCENE_CHANGE_RESP,
                   reinterpret_cast<const uint8_t*>(scene_resp_data.data()),
                   scene_resp_data.size());

    // 8. Send ClockSync
    broadcast_clock_sync();

    // 9. Send EnergySync (via ItemUseResp with energy field)
    farm::ItemUseResp energy_resp;
    energy_resp.set_code(0);
    energy_resp.set_msg("force sleep restore");
    farm::EnergySync* energy_sync = energy_resp.mutable_energy();
    energy_sync->set_current(player->get_energy());
    energy_sync->set_max(max_energy);

    std::string energy_resp_data;
    energy_resp.SerializeToString(&energy_resp_data);
    send_game_msg_(player_id, MSG_ID_ITEM_USE_RESP,
                   reinterpret_cast<const uint8_t*>(energy_resp_data.data()),
                   energy_resp_data.size());

    SPDLOG_INFO("[Game]Force sleep completed: player={}, day={}, energy={}/{}",
                player_id, clock_day_, player->get_energy(), max_energy);
}

}  // namespace farm
