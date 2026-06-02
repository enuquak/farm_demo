#pragma once

#include "quest_types.h"
#include "quest_config.h"
#include "event_bus.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace farm {

class Player;
class PlayerManager;

class QuestManager {
public:
    QuestManager(PlayerManager* player_mgr, const QuestConfig* config);
    ~QuestManager();

    // Initialize: subscribe to events
    void init();

    // Player login: load quest progress from player data
    void on_player_login(uint64_t player_id);

    // Player logout: quest progress is saved with player data
    void on_player_logout(uint64_t player_id);

    // Handle quest accept request
    void handle_quest_accept(uint64_t player_id, const std::string& quest_id);

    // Handle quest submit request
    void handle_quest_submit(uint64_t player_id, const std::string& quest_id);

    // Handle quest abandon request
    void handle_quest_abandon(uint64_t player_id, const std::string& quest_id);

    // Get player's task info JSON for persistence
    std::string get_task_infos_json(uint64_t player_id) const;

    // Load player's task info from JSON
    void load_task_infos(uint64_t player_id, const std::string& json_str);

    // Get player's quest progress for a specific quest
    const TaskInfo* get_task_info(uint64_t player_id, const std::string& quest_id) const;

    // Get all active quests for a player
    std::vector<TaskInfo> get_active_quests(uint64_t player_id) const;

    // Get all available (not accepted) quests for a player
    std::vector<std::string> get_available_quests(uint64_t player_id) const;

private:
    PlayerManager* player_mgr_;
    const QuestConfig* config_;
    std::vector<EventHandlerId> event_handler_ids_;

    // Per-player quest data: player_id -> (quest_id -> TaskInfo)
    std::unordered_map<uint64_t, std::unordered_map<std::string, TaskInfo>> player_quests_;

    // Event handlers
    void on_game_event(const GameEvent& event);

    // Quest state management
    void update_quest_progress(uint64_t player_id, ObjectiveType obj_type,
                               const std::string& target, int32_t count = 1);
    void check_quest_completion(uint64_t player_id, const std::string& quest_id);
    void complete_quest(uint64_t player_id, const std::string& quest_id);
    void grant_rewards(uint64_t player_id, const QuestRewardDef& rewards);
    void unlock_quests(uint64_t player_id, const std::vector<std::string>& quest_ids);

    // Helper: check if all prerequisites are completed
    bool are_prerequisites_met(uint64_t player_id, const QuestDef& quest) const;

    // Helper: get or create player quest data
    std::unordered_map<std::string, TaskInfo>& get_player_quests(uint64_t player_id);

    // Helper: map ObjectiveType to EventType
    EventType objective_type_to_event_type(ObjectiveType type) const;

    // Helper: serialize/deserialize task infos
    nlohmann::json serialize_task_infos(uint64_t player_id) const;
    void deserialize_task_infos(uint64_t player_id, const nlohmann::json& json);
};

}  // namespace farm
