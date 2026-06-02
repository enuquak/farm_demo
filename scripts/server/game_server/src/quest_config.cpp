#include "quest_config.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <stdexcept>

namespace farm {

bool QuestConfig::load_from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        SPDLOG_ERROR("[QuestConfig]Failed to open quest config file: {}", file_path);
        return false;
    }

    try {
        nlohmann::json json;
        file >> json;
        return load_from_json(json.dump());
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[QuestConfig]Failed to parse quest config file: {}", e.what());
        return false;
    }
}

bool QuestConfig::load_from_json(const std::string& json_str) {
    try {
        auto json = nlohmann::json::parse(json_str);

        quests_.clear();
        branch_groups_.clear();

        // Parse quests
        if (json.contains("quests") && json["quests"].is_object()) {
            for (auto& [id, quest_json] : json["quests"].items()) {
                if (!parse_quest(id, quest_json)) {
                    SPDLOG_WARN("[QuestConfig]Skipping invalid quest: {}", id);
                }
            }
        }

        // Parse branch groups
        if (json.contains("branch_groups") && json["branch_groups"].is_object()) {
            for (auto& [id, group_json] : json["branch_groups"].items()) {
                BranchGroupDef group;
                group.name = group_json.value("name", "");
                group.description = group_json.value("description", "");
                group.unlock_condition = group_json.value("unlock_condition", "");

                if (group_json.contains("quest_ids") && group_json["quest_ids"].is_array()) {
                    for (const auto& qid : group_json["quest_ids"]) {
                        group.quest_ids.push_back(qid.get<std::string>());
                    }
                }

                branch_groups_[id] = std::move(group);
            }
        }

        SPDLOG_INFO("[QuestConfig]Loaded {} quests and {} branch groups",
                    quests_.size(), branch_groups_.size());
        return true;

    } catch (const std::exception& e) {
        SPDLOG_ERROR("[QuestConfig]Failed to parse quest JSON: {}", e.what());
        return false;
    }
}

const QuestDef* QuestConfig::get_quest(const std::string& quest_id) const {
    auto it = quests_.find(quest_id);
    return it != quests_.end() ? &it->second : nullptr;
}

const BranchGroupDef* QuestConfig::get_branch_group(const std::string& group_id) const {
    auto it = branch_groups_.find(group_id);
    return it != branch_groups_.end() ? &it->second : nullptr;
}

bool QuestConfig::has_quest(const std::string& quest_id) const {
    return quests_.find(quest_id) != quests_.end();
}

std::vector<std::string> QuestConfig::get_starting_quests() const {
    std::vector<std::string> result;
    for (const auto& [id, quest] : quests_) {
        if (quest.prerequisites.empty()) {
            result.push_back(id);
        }
    }
    return result;
}

bool QuestConfig::parse_quest(const std::string& id, const nlohmann::json& json) {
    QuestDef quest;
    quest.id = id;
    quest.name = json.value("name", "");
    quest.description = json.value("description", "");
    quest.type = json.value("type", "");
    quest.branch_group = json.value("branch_group", "");

    if (json.contains("trigger") && json["trigger"].is_object()) {
        parse_trigger(json["trigger"], quest.trigger);
    }

    if (json.contains("objectives") && json["objectives"].is_array()) {
        parse_objectives(json["objectives"], quest.objectives);
    }

    if (json.contains("rewards") && json["rewards"].is_object()) {
        parse_rewards(json["rewards"], quest.rewards);
    }

    if (json.contains("prerequisites") && json["prerequisites"].is_array()) {
        for (const auto& pre : json["prerequisites"]) {
            quest.prerequisites.push_back(pre.get<std::string>());
        }
    }

    quests_[id] = std::move(quest);
    return true;
}

bool QuestConfig::parse_objectives(const nlohmann::json& json, std::vector<ObjectiveDef>& objectives) {
    for (const auto& obj_json : json) {
        ObjectiveDef obj;
        obj.id = obj_json.value("id", "");
        obj.target = obj_json.value("target", "");
        obj.count = obj_json.value("count", 1);
        obj.description = obj_json.value("description", "");

        std::string type_str = obj_json.value("type", "");
        try {
            obj.type = objective_type_from_string(type_str);
        } catch (const std::invalid_argument&) {
            SPDLOG_WARN("[QuestConfig]Unknown objective type: {}", type_str);
            return false;
        }

        objectives.push_back(std::move(obj));
    }
    return true;
}

bool QuestConfig::parse_rewards(const nlohmann::json& json, QuestRewardDef& rewards) {
    rewards.gold = json.value("gold", 0);
    rewards.exp = json.value("exp", 0);

    if (json.contains("items") && json["items"].is_array()) {
        for (const auto& item_json : json["items"]) {
            QuestRewardDef::ItemReward item;
            item.id = item_json.value("id", "");
            item.count = item_json.value("count", 1);
            rewards.items.push_back(std::move(item));
        }
    }

    if (json.contains("unlock_quests") && json["unlock_quests"].is_array()) {
        for (const auto& qid : json["unlock_quests"]) {
            rewards.unlock_quests.push_back(qid.get<std::string>());
        }
    }

    return true;
}

bool QuestConfig::parse_trigger(const nlohmann::json& json, TriggerDef& trigger) {
    std::string type_str = json.value("type", "auto");
    trigger.type = trigger_type_from_string(type_str);
    trigger.condition = json.value("condition", "");
    trigger.npc_id = json.value("npc_id", "");
    trigger.dialogue = json.value("dialogue", "");
    return true;
}

}  // namespace farm
