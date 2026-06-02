#pragma once

#include "quest_types.h"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace farm {

/**
 * @brief Quest configuration loader.
 *
 * Loads quest definitions and branch group definitions from a JSON file
 * or JSON string. Provides lookup methods for quests and branch groups.
 *
 * JSON format:
 *   {
 *     "quests": { "quest_id": { ... }, ... },
 *     "branch_groups": { "group_id": { ... }, ... }
 *   }
 */
class QuestConfig {
public:
    QuestConfig() = default;
    ~QuestConfig() = default;

    /**
     * @brief Load quest configuration from a JSON file.
     * @param file_path Path to the JSON file
     * @return true if loaded successfully
     */
    bool load_from_file(const std::string& file_path);

    /**
     * @brief Load quest configuration from a JSON string.
     * @param json_str JSON string content
     * @return true if loaded successfully
     */
    bool load_from_json(const std::string& json_str);

    /**
     * @brief Get a quest definition by ID.
     * @param quest_id The quest identifier
     * @return Pointer to QuestDef if found, nullptr otherwise
     */
    const QuestDef* get_quest(const std::string& quest_id) const;

    /**
     * @brief Get all quest definitions.
     */
    const std::unordered_map<std::string, QuestDef>& quests() const { return quests_; }

    /**
     * @brief Get a branch group definition by ID.
     * @param group_id The branch group identifier
     * @return Pointer to BranchGroupDef if found, nullptr otherwise
     */
    const BranchGroupDef* get_branch_group(const std::string& group_id) const;

    /**
     * @brief Get all branch group definitions.
     */
    const std::unordered_map<std::string, BranchGroupDef>& branch_groups() const { return branch_groups_; }

    /**
     * @brief Check if a quest exists in the configuration.
     * @param quest_id The quest identifier
     * @return true if the quest exists
     */
    bool has_quest(const std::string& quest_id) const;

    /**
     * @brief Get IDs of all quests that have no prerequisites (starting quests).
     * @return Vector of quest IDs
     */
    std::vector<std::string> get_starting_quests() const;

private:
    std::unordered_map<std::string, QuestDef> quests_;
    std::unordered_map<std::string, BranchGroupDef> branch_groups_;

    // Parse a single quest from JSON
    bool parse_quest(const std::string& id, const nlohmann::json& json);

    // Parse objectives array from JSON
    bool parse_objectives(const nlohmann::json& json, std::vector<ObjectiveDef>& objectives);

    // Parse rewards object from JSON
    bool parse_rewards(const nlohmann::json& json, QuestRewardDef& rewards);

    // Parse trigger object from JSON
    bool parse_trigger(const nlohmann::json& json, TriggerDef& trigger);
};

}  // namespace farm
