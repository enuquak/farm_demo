#pragma once

#include "gm_templates.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace farm {

using json = nlohmann::json;

class PlayerManager;
class DBMgrConnectionManager;

// Parameter metadata for frontend dynamic rendering
struct GmParamDef {
    std::string name;       // Parameter key (e.g. "player_id")
    std::string type;       // "player", "int", "string", "item"
    std::string label;      // Display label (e.g. "玩家ID")
    bool required = true;
};

// Command metadata returned to frontend
struct GmCommandDef {
    std::string name;       // Command name (e.g. "SetLevel")
    std::string label;      // Display label (e.g. "设置等级")
    std::string description;
    std::vector<GmParamDef> params;
};

// GM command handler signature: takes args JSON, returns result JSON
using GmHandler = std::function<json(const json& args)>;

class GMStub {
public:
    GMStub(PlayerManager* player_mgr, DBMgrConnectionManager* dbmgr_mgr);
    ~GMStub() = default;

    // Initialize: register all commands and load templates
    void init();

    // Execute a single GM command
    json exec(const std::string& cmd, const json& args);

    // Execute a batch template
    json batch_exec(const std::string& template_name, uint64_t player_id, uint32_t server_id);

    // Get all registered command definitions (for frontend)
    json get_commands() const;

    // Get all template definitions (for frontend)
    json get_templates() const;

private:
    // Register a single command
    void register_command(const std::string& name, const std::string& label,
                          const std::string& description,
                          const std::vector<GmParamDef>& params,
                          GmHandler handler);

    // Command handlers
    json handle_set_level(const json& args);
    json handle_set_gold(const json& args);
    json handle_set_exp(const json& args);
    json handle_set_energy(const json& args);
    json handle_give_item(const json& args);
    json handle_del_item(const json& args);
    json handle_query_player(const json& args);
    json handle_kick_player(const json& args);

    // Helper: extract player_id from args, return 0 if missing/invalid
    uint64_t extract_player_id(const json& args);

    // Helper: build error response
    json make_error(int code, const std::string& msg);

    // Helper: build success response
    json make_success(const json& data = json::object());

    PlayerManager* player_mgr_;
    DBMgrConnectionManager* dbmgr_mgr_;

    // Command registry
    struct CommandEntry {
        GmCommandDef def;
        GmHandler handler;
    };
    std::unordered_map<std::string, CommandEntry> commands_;

    // Template registry
    std::vector<GmTemplate> templates_;
};

}  // namespace farm
