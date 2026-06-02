#include "quest_manager.h"
#include "player.h"
#include "player_manager.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <ctime>

namespace farm {

QuestManager::QuestManager(PlayerManager* player_mgr, const QuestConfig* config)
    : player_mgr_(player_mgr), config_(config) {}

QuestManager::~QuestManager() {
    // EventBus handles cleanup on destruction
    event_handler_ids_.clear();
}

void QuestManager::init() {
    auto& bus = EventBus::instance();

    // Subscribe to all relevant events
    event_handler_ids_.push_back(bus.subscribe(EventType::ItemCollected,
        [this](const GameEvent& e) { on_game_event(e); }));
    event_handler_ids_.push_back(bus.subscribe(EventType::CropPlanted,
        [this](const GameEvent& e) { on_game_event(e); }));
    event_handler_ids_.push_back(bus.subscribe(EventType::CropHarvested,
        [this](const GameEvent& e) { on_game_event(e); }));
    event_handler_ids_.push_back(bus.subscribe(EventType::NPCTalked,
        [this](const GameEvent& e) { on_game_event(e); }));
    event_handler_ids_.push_back(bus.subscribe(EventType::SceneVisited,
        [this](const GameEvent& e) { on_game_event(e); }));
    event_handler_ids_.push_back(bus.subscribe(EventType::ItemCrafted,
        [this](const GameEvent& e) { on_game_event(e); }));
    event_handler_ids_.push_back(bus.subscribe(EventType::ItemUsed,
        [this](const GameEvent& e) { on_game_event(e); }));
    event_handler_ids_.push_back(bus.subscribe(EventType::LevelUp,
        [this](const GameEvent& e) { on_game_event(e); }));
    event_handler_ids_.push_back(bus.subscribe(EventType::GoldChanged,
        [this](const GameEvent& e) { on_game_event(e); }));

    SPDLOG_INFO("[QuestManager]Initialized, subscribed to {} event types", event_handler_ids_.size());
}

void QuestManager::on_player_login(uint64_t player_id) {
    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) return;

    Player* player = player_opt.value();
    const std::string& task_json = player->get_extra_data();

    if (!task_json.empty() && task_json != "{}") {
        try {
            auto json = nlohmann::json::parse(task_json);
            if (json.contains("task_infos")) {
                deserialize_task_infos(player_id, json["task_infos"]);
            }
        } catch (const std::exception& e) {
            SPDLOG_WARN("[QuestManager]Failed to load task infos for player={}: {}",
                       player_id, e.what());
        }
    }

    SPDLOG_INFO("[QuestManager]Player {} login, loaded {} quests", player_id,
                player_quests_[player_id].size());
}

void QuestManager::on_player_logout(uint64_t player_id) {
    // Save quest progress to player data
    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) return;

    Player* player = player_opt.value();

    // Merge task_infos into extra_data
    std::string extra_json_str = player->get_extra_data();
    nlohmann::json extra_json;
    if (!extra_json_str.empty() && extra_json_str != "{}") {
        try {
            extra_json = nlohmann::json::parse(extra_json_str);
        } catch (...) {
            extra_json = nlohmann::json::object();
        }
    }

    extra_json["task_infos"] = serialize_task_infos(player_id);
    player->set_extra_data(extra_json.dump());

    // Clean up in-memory data
    player_quests_.erase(player_id);

    SPDLOG_INFO("[QuestManager]Player {} logout, saved quest progress", player_id);
}

void QuestManager::handle_quest_accept(uint64_t player_id, const std::string& quest_id) {
    auto* quest_def = config_->get_quest(quest_id);
    if (!quest_def) {
        SPDLOG_WARN("[QuestManager]Quest not found: {}", quest_id);
        return;
    }

    auto& quests = get_player_quests(player_id);
    auto it = quests.find(quest_id);

    // Check if quest is already accepted or completed
    if (it != quests.end() && it->second.state != TaskState::NOT_ACCEPTED) {
        SPDLOG_WARN("[QuestManager]Quest {} already in state {} for player {}",
                   quest_id, static_cast<int>(it->second.state), player_id);
        return;
    }

    // Check prerequisites
    if (!are_prerequisites_met(player_id, *quest_def)) {
        SPDLOG_WARN("[QuestManager]Prerequisites not met for quest {} player {}", quest_id, player_id);
        return;
    }

    // Accept the quest
    TaskInfo info;
    info.task_id = quest_id;
    info.state = TaskState::ACCEPTED;
    info.accept_time = std::time(nullptr);

    // Initialize progress for all objectives
    for (const auto& obj : quest_def->objectives) {
        info.progress[obj.id] = 0;
    }

    quests[quest_id] = std::move(info);

    // Emit event
    EventBus::instance().emit(EventType::QuestAccepted, player_id, {{"quest_id", quest_id}});

    SPDLOG_INFO("[QuestManager]Player {} accepted quest {}", player_id, quest_id);
}

void QuestManager::handle_quest_submit(uint64_t player_id, const std::string& quest_id) {
    auto& quests = get_player_quests(player_id);
    auto it = quests.find(quest_id);

    if (it == quests.end() || it->second.state != TaskState::IN_PROGRESS) {
        SPDLOG_WARN("[QuestManager]Cannot submit quest {} for player {}: not in progress", quest_id, player_id);
        return;
    }

    auto* quest_def = config_->get_quest(quest_id);
    if (!quest_def) return;

    // Check if all objectives are complete
    for (const auto& obj : quest_def->objectives) {
        auto prog_it = it->second.progress.find(obj.id);
        if (prog_it == it->second.progress.end() || prog_it->second < obj.count) {
            SPDLOG_WARN("[QuestManager]Quest {} not complete for player {}: objective {} progress {}/{}",
                       quest_id, player_id, obj.id,
                       prog_it != it->second.progress.end() ? prog_it->second : 0,
                       obj.count);
            return;
        }
    }

    complete_quest(player_id, quest_id);
}

void QuestManager::handle_quest_abandon(uint64_t player_id, const std::string& quest_id) {
    auto& quests = get_player_quests(player_id);
    auto it = quests.find(quest_id);

    if (it == quests.end()) return;

    if (it->second.state == TaskState::ACCEPTED || it->second.state == TaskState::IN_PROGRESS) {
        quests.erase(it);
        SPDLOG_INFO("[QuestManager]Player {} abandoned quest {}", player_id, quest_id);
    }
}

std::string QuestManager::get_task_infos_json(uint64_t player_id) const {
    auto it = player_quests_.find(player_id);
    if (it == player_quests_.end()) return "{}";

    nlohmann::json json = nlohmann::json::object();
    for (const auto& [quest_id, info] : it->second) {
        nlohmann::json info_json;
        info_json["task_id"] = info.task_id;
        info_json["state"] = static_cast<int32_t>(info.state);
        info_json["progress"] = info.progress;
        info_json["accept_time"] = info.accept_time;
        info_json["complete_time"] = info.complete_time;
        json[quest_id] = std::move(info_json);
    }
    return json.dump();
}

void QuestManager::load_task_infos(uint64_t player_id, const std::string& json_str) {
    if (json_str.empty() || json_str == "{}") return;

    try {
        auto json = nlohmann::json::parse(json_str);
        deserialize_task_infos(player_id, json);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[QuestManager]Failed to parse task infos for player {}: {}",
                    player_id, e.what());
    }
}

const TaskInfo* QuestManager::get_task_info(uint64_t player_id, const std::string& quest_id) const {
    auto player_it = player_quests_.find(player_id);
    if (player_it == player_quests_.end()) return nullptr;

    auto quest_it = player_it->second.find(quest_id);
    return quest_it != player_it->second.end() ? &quest_it->second : nullptr;
}

std::vector<TaskInfo> QuestManager::get_active_quests(uint64_t player_id) const {
    std::vector<TaskInfo> result;
    auto player_it = player_quests_.find(player_id);
    if (player_it == player_quests_.end()) return result;

    for (const auto& [quest_id, info] : player_it->second) {
        if (info.state == TaskState::ACCEPTED || info.state == TaskState::IN_PROGRESS) {
            result.push_back(info);
        }
    }
    return result;
}

std::vector<std::string> QuestManager::get_available_quests(uint64_t player_id) const {
    std::vector<std::string> result;

    for (const auto& [quest_id, quest_def] : config_->quests()) {
        // Skip if already accepted or completed
        auto* task_info = get_task_info(player_id, quest_id);
        if (task_info && task_info->state != TaskState::NOT_ACCEPTED) {
            continue;
        }

        // Check prerequisites
        if (are_prerequisites_met(player_id, quest_def)) {
            result.push_back(quest_id);
        }
    }

    return result;
}

void QuestManager::on_game_event(const GameEvent& event) {
    auto& quests = get_player_quests(event.player_id);

    for (auto& [quest_id, info] : quests) {
        if (info.state != TaskState::ACCEPTED && info.state != TaskState::IN_PROGRESS) {
            continue;
        }

        auto* quest_def = config_->get_quest(quest_id);
        if (!quest_def) continue;

        for (const auto& obj : quest_def->objectives) {
            // Check if this event matches the objective type
            EventType expected_event = objective_type_to_event_type(obj.type);
            if (event.type != expected_event) continue;

            // Check if target matches
            std::string event_target;
            if (event.data.contains("item_id")) {
                event_target = event.data["item_id"].get<std::string>();
            } else if (event.data.contains("crop_id")) {
                event_target = event.data["crop_id"].get<std::string>();
            } else if (event.data.contains("npc_id")) {
                event_target = event.data["npc_id"].get<std::string>();
            } else if (event.data.contains("scene_id")) {
                event_target = event.data["scene_id"].get<std::string>();
            }

            if (event_target != obj.target) continue;

            // Update progress
            int32_t count = event.data.value("count", 1);
            auto prog_it = info.progress.find(obj.id);
            if (prog_it != info.progress.end()) {
                prog_it->second = std::min(prog_it->second + count, obj.count);

                // Mark as in progress
                if (info.state == TaskState::ACCEPTED) {
                    info.state = TaskState::IN_PROGRESS;
                }

                SPDLOG_DEBUG("[QuestManager]Player {} quest {} objective {} progress: {}/{}",
                            event.player_id, quest_id, obj.id, prog_it->second, obj.count);
            }
        }

        // Check completion
        check_quest_completion(event.player_id, quest_id);
    }
}

void QuestManager::update_quest_progress(uint64_t player_id, ObjectiveType obj_type,
                                          const std::string& target, int32_t count) {
    // This is called directly for non-event-based progress updates
    // (e.g., reach_level, own_gold)
    auto& quests = get_player_quests(player_id);

    for (auto& [quest_id, info] : quests) {
        if (info.state != TaskState::ACCEPTED && info.state != TaskState::IN_PROGRESS) {
            continue;
        }

        auto* quest_def = config_->get_quest(quest_id);
        if (!quest_def) continue;

        for (const auto& obj : quest_def->objectives) {
            if (obj.type != obj_type) continue;
            if (obj.target != target) continue;

            auto prog_it = info.progress.find(obj.id);
            if (prog_it != info.progress.end()) {
                prog_it->second = std::min(prog_it->second + count, obj.count);
                if (info.state == TaskState::ACCEPTED) {
                    info.state = TaskState::IN_PROGRESS;
                }
            }
        }

        check_quest_completion(player_id, quest_id);
    }
}

void QuestManager::check_quest_completion(uint64_t player_id, const std::string& quest_id) {
    auto& quests = get_player_quests(player_id);
    auto it = quests.find(quest_id);
    if (it == quests.end()) return;

    auto* quest_def = config_->get_quest(quest_id);
    if (!quest_def) return;

    // Check if all objectives are complete
    bool all_complete = true;
    for (const auto& obj : quest_def->objectives) {
        auto prog_it = it->second.progress.find(obj.id);
        if (prog_it == it->second.progress.end() || prog_it->second < obj.count) {
            all_complete = false;
            break;
        }
    }

    if (all_complete && it->second.state == TaskState::IN_PROGRESS) {
        // Auto-complete for non-submit quests
        // For submit quests, the player must explicitly submit
        SPDLOG_INFO("[QuestManager]Player {} quest {} objectives complete, ready to submit",
                   player_id, quest_id);
    }
}

void QuestManager::complete_quest(uint64_t player_id, const std::string& quest_id) {
    auto& quests = get_player_quests(player_id);
    auto it = quests.find(quest_id);
    if (it == quests.end()) return;

    it->second.state = TaskState::COMPLETED;
    it->second.complete_time = std::time(nullptr);

    auto* quest_def = config_->get_quest(quest_id);
    if (!quest_def) return;

    // Grant rewards
    grant_rewards(player_id, quest_def->rewards);

    // Unlock subsequent quests
    unlock_quests(player_id, quest_def->rewards.unlock_quests);

    // Emit completion event
    EventBus::instance().emit(EventType::QuestCompleted, player_id, {{"quest_id", quest_id}});

    SPDLOG_INFO("[QuestManager]Player {} completed quest {}", player_id, quest_id);
}

void QuestManager::grant_rewards(uint64_t player_id, const QuestRewardDef& rewards) {
    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) return;

    Player* player = player_opt.value();

    // Grant gold
    if (rewards.gold > 0) {
        player->set_gold(player->get_gold() + rewards.gold);
        SPDLOG_INFO("[QuestManager]Player {} received {} gold", player_id, rewards.gold);
    }

    // Grant experience
    if (rewards.exp > 0) {
        player->set_experience(player->get_experience() + rewards.exp);
        SPDLOG_INFO("[QuestManager]Player {} received {} exp", player_id, rewards.exp);
    }

    // Grant items (emit event for item system to handle)
    if (!rewards.items.empty()) {
        nlohmann::json items_json = nlohmann::json::array();
        for (const auto& item : rewards.items) {
            items_json.push_back({{"id", item.id}, {"count", item.count}});
        }
        EventBus::instance().emit(EventType::RewardGranted, player_id, {
            {"type", "items"},
            {"items", items_json}
        });
        SPDLOG_INFO("[QuestManager]Player {} received {} item types", player_id, rewards.items.size());
    }

    // Emit reward event
    nlohmann::json reward_data;
    reward_data["gold"] = rewards.gold;
    reward_data["exp"] = rewards.exp;
    EventBus::instance().emit(EventType::RewardGranted, player_id, {
        {"type", "quest_reward"},
        {"reward_data", reward_data}
    });
}

void QuestManager::unlock_quests(uint64_t player_id, const std::vector<std::string>& quest_ids) {
    auto& quests = get_player_quests(player_id);

    for (const auto& quest_id : quest_ids) {
        if (!config_->has_quest(quest_id)) {
            SPDLOG_WARN("[QuestManager]Cannot unlock unknown quest: {}", quest_id);
            continue;
        }

        // Only unlock if not already tracked
        if (quests.find(quest_id) == quests.end()) {
            TaskInfo info;
            info.task_id = quest_id;
            info.state = TaskState::NOT_ACCEPTED;
            quests[quest_id] = std::move(info);
            SPDLOG_INFO("[QuestManager]Unlocked quest {} for player {}", quest_id, player_id);
        }
    }
}

bool QuestManager::are_prerequisites_met(uint64_t player_id, const QuestDef& quest) const {
    if (quest.prerequisites.empty()) return true;

    auto player_it = player_quests_.find(player_id);
    if (player_it == player_quests_.end()) return false;

    for (const auto& pre_id : quest.prerequisites) {
        auto quest_it = player_it->second.find(pre_id);
        if (quest_it == player_it->second.end()) return false;
        if (quest_it->second.state != TaskState::COMPLETED) return false;
    }

    return true;
}

std::unordered_map<std::string, TaskInfo>& QuestManager::get_player_quests(uint64_t player_id) {
    return player_quests_[player_id];
}

EventType QuestManager::objective_type_to_event_type(ObjectiveType type) const {
    switch (type) {
        case ObjectiveType::CollectItem: return EventType::ItemCollected;
        case ObjectiveType::PlantCrop: return EventType::CropPlanted;
        case ObjectiveType::HarvestCrop: return EventType::CropHarvested;
        case ObjectiveType::TalkToNPC: return EventType::NPCTalked;
        case ObjectiveType::VisitScene: return EventType::SceneVisited;
        case ObjectiveType::CraftItem: return EventType::ItemCrafted;
        case ObjectiveType::UseItem: return EventType::ItemUsed;
        case ObjectiveType::ReachLevel: return EventType::LevelUp;
        case ObjectiveType::OwnGold: return EventType::GoldChanged;
        case ObjectiveType::Custom: return EventType::CustomConditionMet;
        default: return EventType::CustomConditionMet;
    }
}

nlohmann::json QuestManager::serialize_task_infos(uint64_t player_id) const {
    nlohmann::json json = nlohmann::json::object();
    auto player_it = player_quests_.find(player_id);
    if (player_it == player_quests_.end()) return json;

    for (const auto& [quest_id, info] : player_it->second) {
        nlohmann::json info_json;
        info_json["task_id"] = info.task_id;
        info_json["state"] = static_cast<int32_t>(info.state);
        info_json["progress"] = info.progress;
        info_json["accept_time"] = info.accept_time;
        info_json["complete_time"] = info.complete_time;
        json[quest_id] = std::move(info_json);
    }
    return json;
}

void QuestManager::deserialize_task_infos(uint64_t player_id, const nlohmann::json& json) {
    auto& quests = player_quests_[player_id];
    quests.clear();

    for (auto& [quest_id, info_json] : json.items()) {
        TaskInfo info;
        info.task_id = info_json.value("task_id", quest_id);
        info.state = static_cast<TaskState>(info_json.value("state", 0));
        info.accept_time = info_json.value("accept_time", 0LL);
        info.complete_time = info_json.value("complete_time", 0LL);

        if (info_json.contains("progress")) {
            for (auto& [obj_id, count] : info_json["progress"].items()) {
                info.progress[obj_id] = count.get<int32_t>();
            }
        }

        quests[quest_id] = std::move(info);
    }
}

}  // namespace farm
