#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace farm {

using json = nlohmann::json;

struct GmTemplateStep {
    std::string cmd;       // GM command name (e.g. "SetLevel")
    json args;             // Fixed args (不含 player_id，执行时自动合并)
};

struct GmTemplate {
    std::string name;          // Template identifier (e.g. "player_init")
    std::string label;         // Display name (e.g. "玩家初级模板")
    std::string description;   // Description
    std::vector<GmTemplateStep> steps;
};

// Get all preset templates
inline std::vector<GmTemplate> get_preset_templates() {
    std::vector<GmTemplate> templates;

    // 玩家初级模板
    GmTemplate player_init;
    player_init.name = "player_init";
    player_init.label = "玩家初级模板";
    player_init.description = "快速初始化一个新手玩家";
    player_init.steps = {
        {"SetLevel", json{{"level", 10}}},
        {"SetGold", json{{"gold", 500}}},
        {"GiveItem", json{{"item_id", 6}, {"count", 20}}}
    };
    templates.push_back(std::move(player_init));

    return templates;
}

}  // namespace farm
