#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace farm {

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum class TaskState : int32_t {
    NOT_ACCEPTED = 0,
    ACCEPTED     = 1,
    IN_PROGRESS  = 2,
    COMPLETED    = 3,
    FAILED       = 4,
};

enum class ObjectiveType {
    CollectItem,
    PlantCrop,
    HarvestCrop,
    TalkToNPC,
    VisitScene,
    CraftItem,
    UseItem,
    ReachLevel,
    OwnGold,
    Custom,
};

enum class TriggerType {
    Auto,
    NPC,
};

// ---------------------------------------------------------------------------
// String conversion declarations (implemented in quest_types.cpp)
// ---------------------------------------------------------------------------

const char* objective_type_to_string(ObjectiveType type);
ObjectiveType objective_type_from_string(const std::string& str);
TriggerType trigger_type_from_string(const std::string& str);

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

struct ObjectiveDef {
    std::string id;
    ObjectiveType type = ObjectiveType::Custom;
    std::string target;
    int32_t count = 0;
    std::string description;
};

struct QuestRewardDef {
    struct ItemReward {
        std::string id;
        int32_t count = 0;
    };

    std::vector<ItemReward> items;
    int32_t gold = 0;
    int32_t exp = 0;
    std::vector<std::string> unlock_quests;
};

struct TriggerDef {
    TriggerType type = TriggerType::Auto;
    std::string condition;
    std::string npc_id;
    std::string dialogue;
};

struct QuestDef {
    std::string id;
    std::string name;
    std::string description;
    std::string type;
    TriggerDef trigger;
    std::vector<ObjectiveDef> objectives;
    QuestRewardDef rewards;
    std::vector<std::string> prerequisites;
    std::string branch_group;
};

struct BranchGroupDef {
    std::string name;
    std::string description;
    std::string unlock_condition;
    std::vector<std::string> quest_ids;
};

struct TaskInfo {
    std::string task_id;
    TaskState state = TaskState::NOT_ACCEPTED;
    std::unordered_map<std::string, int32_t> progress;
    int64_t accept_time = 0;
    int64_t complete_time = 0;
};

} // namespace farm
