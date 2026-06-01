# GM Platform Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an HTTP-based GM system to game_server with a web management panel for player and item management.

**Architecture:** An evhttp-based HTTP handler in game_server exposes REST APIs. All GM logic lives in a centralized GMStub that dispatches commands. The frontend is a single static HTML file using Vue.js CDN.

**Tech Stack:** C++17, libevent (evhttp), nlohmann/json, spdlog, Vue.js 3 (CDN)

---

## File Structure

| Action | File | Responsibility |
|--------|------|----------------|
| Create | `scripts/server/game_server/src/gm_templates.h` | Batch template data structures and preset definitions |
| Create | `scripts/server/game_server/src/gm_stub.h` | GMStub class: command registry, exec/dispatch, metadata |
| Create | `scripts/server/game_server/src/gm_stub.cpp` | GMStub implementation: all 8 command handlers |
| Create | `scripts/server/game_server/src/gm_http_handler.h` | HttpHandler class: evhttp setup, route dispatch |
| Create | `scripts/server/game_server/src/gm_http_handler.cpp` | HttpHandler implementation: all API endpoints |
| Create | `scripts/server/game_server/static/gm.html` | Frontend SPA: Vue.js 3, single-page GM panel |
| Create | `config/gm_server.json` | GM HTTP service config (port, static_dir) |
| Modify | `scripts/server/game_server/src/game_server.h` | Add GMStub and HttpHandler members |
| Modify | `scripts/server/game_server/src/game_server.cpp` | Initialize GMStub and HttpHandler in start() |
| Modify | `scripts/server/game_server/src/main.cpp` | Load gm_server config, pass to GameServer |
| Modify | `scripts/server/game_server/CMakeLists.txt` | Add gm_stub.cpp and gm_http_handler.cpp to SOURCES |

---

### Task 1: GM Templates

**Files:**
- Create: `scripts/server/game_server/src/gm_templates.h`

- [ ] **Step 1: Create gm_templates.h**

```cpp
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
```

- [ ] **Step 2: Verify compilation**

Run: The file is header-only and will be verified when included in Task 2.

- [ ] **Step 3: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/gm_templates.h
git commit -m "feat(gm): add batch template data structures and preset definitions"
```

---

### Task 2: GMStub — Command Registry and Dispatch

**Files:**
- Create: `scripts/server/game_server/src/gm_stub.h`
- Create: `scripts/server/game_server/src/gm_stub.cpp`

- [ ] **Step 1: Create gm_stub.h**

```cpp
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
```

- [ ] **Step 2: Create gm_stub.cpp — constructor, init, exec, batch_exec, metadata**

```cpp
#include "gm_stub.h"
#include "player_manager.h"
#include "player.h"
#include "item_interaction_handler.h"
#include "item_effects.h"
#include "log_macros.h"

#include <spdlog/spdlog.h>

namespace farm {

GMStub::GMStub(PlayerManager* player_mgr, DBMgrConnectionManager* dbmgr_mgr)
    : player_mgr_(player_mgr)
    , dbmgr_mgr_(dbmgr_mgr)
{
}

void GMStub::init() {
    // Register all commands
    register_command("SetLevel", "设置等级", "修改指定玩家的等级",
        {{"player_id", "player", "玩家ID", true}, {"level", "int", "目标等级", true}},
        [this](const json& args) { return handle_set_level(args); });

    register_command("SetGold", "设置金币", "设置指定玩家的金币（绝对值）",
        {{"player_id", "player", "玩家ID", true}, {"gold", "int", "金币数量", true}},
        [this](const json& args) { return handle_set_gold(args); });

    register_command("SetExp", "设置经验", "设置指定玩家的经验值",
        {{"player_id", "player", "玩家ID", true}, {"exp", "int", "经验值", true}},
        [this](const json& args) { return handle_set_exp(args); });

    register_command("SetEnergy", "设置体力", "设置指定玩家的体力",
        {{"player_id", "player", "玩家ID", true}, {"energy", "int", "体力值", true}},
        [this](const json& args) { return handle_set_energy(args); });

    register_command("GiveItem", "发放物品", "给指定玩家发放物品",
        {{"player_id", "player", "玩家ID", true}, {"item_id", "item", "物品", true}, {"count", "int", "数量", true}},
        [this](const json& args) { return handle_give_item(args); });

    register_command("DelItem", "删除物品", "删除指定玩家的物品",
        {{"player_id", "player", "玩家ID", true}, {"item_id", "item", "物品", true}, {"count", "int", "数量", true}},
        [this](const json& args) { return handle_del_item(args); });

    register_command("QueryPlayer", "查询玩家", "查询指定玩家的详细信息",
        {{"player_id", "player", "玩家ID", true}},
        [this](const json& args) { return handle_query_player(args); });

    register_command("KickPlayer", "踢下线", "将指定玩家踢下线",
        {{"player_id", "player", "玩家ID", true}},
        [this](const json& args) { return handle_kick_player(args); });

    // Load preset templates
    templates_ = get_preset_templates();

    SPDLOG_INFO("[GM]GMStub initialized: {} commands, {} templates",
                commands_.size(), templates_.size());
}

void GMStub::register_command(const std::string& name, const std::string& label,
                               const std::string& description,
                               const std::vector<GmParamDef>& params,
                               GmHandler handler) {
    CommandEntry entry;
    entry.def.name = name;
    entry.def.label = label;
    entry.def.description = description;
    entry.def.params = params;
    entry.handler = std::move(handler);
    commands_[name] = std::move(entry);
}

json GMStub::exec(const std::string& cmd, const json& args) {
    auto it = commands_.find(cmd);
    if (it == commands_.end()) {
        SPDLOG_WARN("[GM]Unknown command: {}", cmd);
        return make_error(-100, "未知指令: " + cmd);
    }

    SPDLOG_INFO("[GM]Executing command: {}", cmd);
    try {
        json result = it->second.handler(args);
        SPDLOG_INFO("[GM]Command {} result: code={}", cmd, result.value("code", -999));
        return result;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("[GM]Command {} exception: {}", cmd, e.what());
        return make_error(-3, std::string("执行异常: ") + e.what());
    }
}

json GMStub::batch_exec(const std::string& template_name, uint64_t player_id, uint32_t server_id) {
    // Find template
    const GmTemplate* tmpl = nullptr;
    for (const auto& t : templates_) {
        if (t.name == template_name) {
            tmpl = &t;
            break;
        }
    }
    if (!tmpl) {
        return make_error(-100, "未知模板: " + template_name);
    }

    SPDLOG_INFO("[GM-Batch]Executing template: {} for player_id={}", template_name, player_id);

    json results = json::array();
    int success_count = 0;

    for (const auto& step : tmpl->steps) {
        // Merge player_id into step args
        json args = step.args;
        args["player_id"] = player_id;

        json result = exec(step.cmd, args);
        result["cmd"] = step.cmd;
        results.push_back(result);

        if (result.value("code", -1) == 0) {
            success_count++;
        }
    }

    SPDLOG_INFO("[GM-Batch]Template {} completed: {}/{} success for player_id={}",
                template_name, success_count, tmpl->steps.size(), player_id);

    json response;
    response["code"] = 0;
    response["msg"] = "ok";
    response["results"] = results;
    return response;
}

json GMStub::get_commands() const {
    json commands_json = json::array();
    for (const auto& [name, entry] : commands_) {
        json cmd;
        cmd["name"] = entry.def.name;
        cmd["label"] = entry.def.label;
        cmd["description"] = entry.def.description;
        json params = json::array();
        for (const auto& p : entry.def.params) {
            params.push_back({
                {"name", p.name},
                {"type", p.type},
                {"label", p.label},
                {"required", p.required}
            });
        }
        cmd["params"] = params;
        commands_json.push_back(cmd);
    }
    json response;
    response["commands"] = commands_json;
    return response;
}

json GMStub::get_templates() const {
    json templates_json = json::array();
    for (const auto& tmpl : templates_) {
        json t;
        t["name"] = tmpl.name;
        t["label"] = tmpl.label;
        t["description"] = tmpl.description;
        json steps = json::array();
        for (const auto& step : tmpl.steps) {
            steps.push_back({
                {"cmd", step.cmd},
                {"args", step.args}
            });
        }
        t["steps"] = steps;
        templates_json.push_back(t);
    }
    json response;
    response["templates"] = templates_json;
    return response;
}

uint64_t GMStub::extract_player_id(const json& args) {
    if (!args.contains("player_id") || !args["player_id"].is_number_unsigned()) {
        return 0;
    }
    return args["player_id"].get<uint64_t>();
}

json GMStub::make_error(int code, const std::string& msg) {
    return {{"code", code}, {"msg", msg}};
}

json GMStub::make_success(const json& data) {
    return {{"code", 0}, {"msg", "ok"}, {"data", data}};
}

}  // namespace farm
```

- [ ] **Step 3: Add command handlers to gm_stub.cpp**

Append the following handlers to `gm_stub.cpp` (before the closing `}  // namespace farm`):

```cpp
// ========== Command Handlers ==========

json GMStub::handle_set_level(const json& args) {
    uint64_t player_id = extract_player_id(args);
    if (player_id == 0) return make_error(-1, "参数错误: player_id 无效");

    if (!args.contains("level") || !args["level"].is_number_integer()) {
        return make_error(-1, "参数错误: level 必须为整数");
    }
    int32_t level = args["level"].get<int32_t>();
    if (level < 1) return make_error(-1, "参数错误: level 必须大于0");

    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) {
        return make_error(-2, "玩家 " + std::to_string(player_id) + " 不在线");
    }

    Player* player = player_opt.value();
    int32_t old_level = player->get_level();
    player->set_level(level);

    SPDLOG_INFO("[GM]SetLevel player_id={} old={} new={}", player_id, old_level, level);
    return make_success({{"player_id", player_id}, {"old_level", old_level}, {"new_level", level}});
}

json GMStub::handle_set_gold(const json& args) {
    uint64_t player_id = extract_player_id(args);
    if (player_id == 0) return make_error(-1, "参数错误: player_id 无效");

    if (!args.contains("gold") || !args["gold"].is_number_integer()) {
        return make_error(-1, "参数错误: gold 必须为整数");
    }
    int64_t gold = args["gold"].get<int64_t>();
    if (gold < 0) return make_error(-1, "参数错误: gold 不能为负数");

    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) {
        return make_error(-2, "玩家 " + std::to_string(player_id) + " 不在线");
    }

    Player* player = player_opt.value();
    int64_t old_gold = player->get_gold();
    player->set_gold(gold);

    SPDLOG_INFO("[GM]SetGold player_id={} old={} new={}", player_id, old_gold, gold);
    return make_success({{"player_id", player_id}, {"old_gold", old_gold}, {"new_gold", gold}});
}

json GMStub::handle_set_exp(const json& args) {
    uint64_t player_id = extract_player_id(args);
    if (player_id == 0) return make_error(-1, "参数错误: player_id 无效");

    if (!args.contains("exp") || !args["exp"].is_number_integer()) {
        return make_error(-1, "参数错误: exp 必须为整数");
    }
    int64_t exp = args["exp"].get<int64_t>();
    if (exp < 0) return make_error(-1, "参数错误: exp 不能为负数");

    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) {
        return make_error(-2, "玩家 " + std::to_string(player_id) + " 不在线");
    }

    Player* player = player_opt.value();
    int64_t old_exp = player->get_experience();
    player->set_experience(exp);

    SPDLOG_INFO("[GM]SetExp player_id={} old={} new={}", player_id, old_exp, exp);
    return make_success({{"player_id", player_id}, {"old_exp", old_exp}, {"new_exp", exp}});
}

json GMStub::handle_set_energy(const json& args) {
    uint64_t player_id = extract_player_id(args);
    if (player_id == 0) return make_error(-1, "参数错误: player_id 无效");

    if (!args.contains("energy") || !args["energy"].is_number_integer()) {
        return make_error(-1, "参数错误: energy 必须为整数");
    }
    int32_t energy = args["energy"].get<int32_t>();
    if (energy < 0 || energy > 100) return make_error(-1, "参数错误: energy 必须在 0-100 之间");

    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) {
        return make_error(-2, "玩家 " + std::to_string(player_id) + " 不在线");
    }

    Player* player = player_opt.value();
    int32_t old_energy = player->get_energy();
    player->set_energy(energy);

    SPDLOG_INFO("[GM]SetEnergy player_id={} old={} new={}", player_id, old_energy, energy);
    return make_success({{"player_id", player_id}, {"old_energy", old_energy}, {"new_energy", energy}});
}

json GMStub::handle_give_item(const json& args) {
    uint64_t player_id = extract_player_id(args);
    if (player_id == 0) return make_error(-1, "参数错误: player_id 无效");

    if (!args.contains("item_id") || !args["item_id"].is_number_integer()) {
        return make_error(-1, "参数错误: item_id 必须为整数");
    }
    if (!args.contains("count") || !args["count"].is_number_integer()) {
        return make_error(-1, "参数错误: count 必须为整数");
    }
    int32_t item_id = args["item_id"].get<int32_t>();
    int32_t count = args["count"].get<int32_t>();
    if (item_id <= 0) return make_error(-1, "参数错误: item_id 无效");
    if (count <= 0) return make_error(-1, "参数错误: count 必须大于0");

    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) {
        return make_error(-2, "玩家 " + std::to_string(player_id) + " 不在线");
    }

    Player* player = player_opt.value();

    // Parse inventory
    PlayerInventory inv;
    if (!inv.deserialize(player->get_inventory())) {
        return make_error(-3, "解析背包数据失败");
    }

    int32_t max_stack = ItemEffects::get_max_stack(item_id);
    int32_t remaining = inv.add_item(item_id, count, max_stack);
    int32_t added = count - remaining;

    // Save inventory back
    player->set_inventory(inv.serialize());

    const char* item_name = ItemEffects::get_item_name(item_id);
    SPDLOG_INFO("[GM]GiveItem player_id={} item={}({}) x{} added={}",
                player_id, item_name, item_id, count, added);

    if (remaining > 0) {
        return make_success({
            {"player_id", player_id}, {"item_id", item_id},
            {"requested", count}, {"added", added}, {"remaining", remaining},
            {"msg", "背包空间不足，部分物品未添加"}
        });
    }
    return make_success({
        {"player_id", player_id}, {"item_id", item_id},
        {"requested", count}, {"added", added}
    });
}

json GMStub::handle_del_item(const json& args) {
    uint64_t player_id = extract_player_id(args);
    if (player_id == 0) return make_error(-1, "参数错误: player_id 无效");

    if (!args.contains("item_id") || !args["item_id"].is_number_integer()) {
        return make_error(-1, "参数错误: item_id 必须为整数");
    }
    if (!args.contains("count") || !args["count"].is_number_integer()) {
        return make_error(-1, "参数错误: count 必须为整数");
    }
    int32_t item_id = args["item_id"].get<int32_t>();
    int32_t count = args["count"].get<int32_t>();
    if (item_id <= 0) return make_error(-1, "参数错误: item_id 无效");
    if (count <= 0) return make_error(-1, "参数错误: count 必须大于0");

    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) {
        return make_error(-2, "玩家 " + std::to_string(player_id) + " 不在线");
    }

    Player* player = player_opt.value();

    PlayerInventory inv;
    if (!inv.deserialize(player->get_inventory())) {
        return make_error(-3, "解析背包数据失败");
    }

    int32_t has_count = inv.get_count(item_id);
    if (has_count < count) {
        return make_error(-1, "物品不足: 需要 " + std::to_string(count) +
                        " 个，当前只有 " + std::to_string(has_count) + " 个");
    }

    inv.remove_item(item_id, count);
    player->set_inventory(inv.serialize());

    const char* item_name = ItemEffects::get_item_name(item_id);
    SPDLOG_INFO("[GM]DelItem player_id={} item={}({}) x{}",
                player_id, item_name, item_id, count);

    return make_success({
        {"player_id", player_id}, {"item_id", item_id},
        {"removed", count}, {"remaining", has_count - count}
    });
}

json GMStub::handle_query_player(const json& args) {
    uint64_t player_id = extract_player_id(args);
    if (player_id == 0) return make_error(-1, "参数错误: player_id 无效");

    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) {
        return make_error(-2, "玩家 " + std::to_string(player_id) + " 不在线");
    }

    Player* player = player_opt.value();
    const auto& data = player->player_data();

    json result;
    result["player_id"] = player_id;
    result["role_name"] = data.role_name;
    result["level"] = data.level;
    result["gold"] = data.gold;
    result["experience"] = data.experience;
    result["energy"] = data.energy;
    result["scene_id"] = data.scene_id;
    result["pos_x"] = data.pos_x;
    result["pos_y"] = data.pos_y;
    result["pos_z"] = data.pos_z;
    result["online"] = true;

    SPDLOG_INFO("[GM]QueryPlayer player_id={} name={} level={}", player_id, data.role_name, data.level);
    return make_success(result);
}

json GMStub::handle_kick_player(const json& args) {
    uint64_t player_id = extract_player_id(args);
    if (player_id == 0) return make_error(-1, "参数错误: player_id 无效");

    auto player_opt = player_mgr_->get_player(player_id);
    if (!player_opt.has_value()) {
        return make_error(-2, "玩家 " + std::to_string(player_id) + " 不在线");
    }

    // Save and remove player
    player_mgr_->remove_player_with_save(player_id);

    SPDLOG_INFO("[GM]KickPlayer player_id={}", player_id);
    return make_success({{"player_id", player_id}, {"msg", "玩家已被踢下线"}});
}
```

- [ ] **Step 4: Verify compilation of gm_stub**

Run: The file will be compiled as part of game_server in Task 5. Verify syntax is correct by inspection.

- [ ] **Step 5: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/gm_stub.h scripts/server/game_server/src/gm_stub.cpp
git commit -m "feat(gm): add GMStub with 8 command handlers and batch execution"
```

---

### Task 3: HTTP Handler (evhttp)

**Files:**
- Create: `scripts/server/game_server/src/gm_http_handler.h`
- Create: `scripts/server/game_server/src/gm_http_handler.cpp`

- [ ] **Step 1: Create gm_http_handler.h**

```cpp
#pragma once

#include <event2/http.h>
#include <string>
#include <cstdint>

namespace farm {

class GMStub;
class PlayerManager;

class GmHttpHandler {
public:
    GmHttpHandler(GMStub* gm_stub, PlayerManager* player_mgr);
    ~GmHttpHandler();

    // Start HTTP server on given port, serving static files from static_dir
    bool start(struct event_base* base, uint16_t port, const std::string& static_dir);

    // Stop HTTP server
    void stop();

private:
    // Static callback dispatched by evhttp
    static void on_request(struct evhttp_request* req, void* ctx);

    // Route dispatch
    void handle_request(struct evhttp_request* req);

    // API handlers
    void handle_api_exec(struct evhttp_request* req);
    void handle_api_batch_exec(struct evhttp_request* req);
    void handle_api_commands(struct evhttp_request* req);
    void handle_api_templates(struct evhttp_request* req);
    void handle_api_online_players(struct evhttp_request* req);
    void handle_api_servers(struct evhttp_request* req);

    // Static file serving
    void handle_static_file(struct evhttp_request* req);

    // Helpers
    std::string read_request_body(struct evhttp_request* req);
    void send_json_response(struct evhttp_request* req, int code, const std::string& json_str);
    void send_error(struct evhttp_request* req, int http_code, int app_code, const std::string& msg);

    GMStub* gm_stub_;
    PlayerManager* player_mgr_;
    struct evhttp* http_server_;
    uint16_t port_;
    std::string static_dir_;
};

}  // namespace farm
```

- [ ] **Step 2: Create gm_http_handler.cpp**

```cpp
#include "gm_http_handler.h"
#include "gm_stub.h"
#include "player_manager.h"
#include "player.h"
#include "log_macros.h"

#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>

namespace farm {

using json = nlohmann::json;

GmHttpHandler::GmHttpHandler(GMStub* gm_stub, PlayerManager* player_mgr)
    : gm_stub_(gm_stub)
    , player_mgr_(player_mgr)
    , http_server_(nullptr)
    , port_(0)
{
}

GmHttpHandler::~GmHttpHandler() {
    stop();
}

bool GmHttpHandler::start(struct event_base* base, uint16_t port, const std::string& static_dir) {
    port_ = port;
    static_dir_ = static_dir;

    http_server_ = evhttp_new(base);
    if (!http_server_) {
        SPDLOG_ERROR("[GM-HTTP]Failed to create evhttp server");
        return false;
    }

    evhttp_set_gencb(http_server_, on_request, this);

    if (evhttp_bind_socket(http_server_, "0.0.0.0", port) != 0) {
        SPDLOG_ERROR("[GM-HTTP]Failed to bind HTTP server on port {}", port);
        evhttp_free(http_server_);
        http_server_ = nullptr;
        return false;
    }

    SPDLOG_INFO("[GM-HTTP]GM HTTP server listening on port {}", port);
    return true;
}

void GmHttpHandler::stop() {
    if (http_server_) {
        evhttp_free(http_server_);
        http_server_ = nullptr;
    }
}

void GmHttpHandler::on_request(struct evhttp_request* req, void* ctx) {
    auto* handler = static_cast<GmHttpHandler*>(ctx);
    handler->handle_request(req);
}

void GmHttpHandler::handle_request(struct evhttp_request* req) {
    const char* uri = evhttp_request_get_uri(req);
    std::string uri_str(uri ? uri : "");

    // Route: /gm → static file
    if (uri_str == "/gm" || uri_str == "/gm/") {
        handle_static_file(req);
        return;
    }

    // Route: API endpoints
    if (uri_str == "/api/gm/exec" && evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
        handle_api_exec(req);
        return;
    }
    if (uri_str == "/api/gm/batch_exec" && evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
        handle_api_batch_exec(req);
        return;
    }
    if (uri_str == "/api/gm/commands") {
        handle_api_commands(req);
        return;
    }
    if (uri_str == "/api/gm/templates") {
        handle_api_templates(req);
        return;
    }
    if (uri_str.find("/api/gm/online_players") == 0) {
        handle_api_online_players(req);
        return;
    }
    if (uri_str == "/api/gm/servers") {
        handle_api_servers(req);
        return;
    }

    // Fallback: try static file
    handle_static_file(req);
}

void GmHttpHandler::handle_api_exec(struct evhttp_request* req) {
    std::string body = read_request_body(req);
    if (body.empty()) {
        send_error(req, HTTP_BADREQUEST, -1, "请求体为空");
        return;
    }

    json request;
    try {
        request = json::parse(body);
    } catch (const json::parse_error& e) {
        send_error(req, HTTP_BADREQUEST, -1, std::string("JSON 解析失败: ") + e.what());
        return;
    }

    if (!request.contains("cmd") || !request["cmd"].is_string()) {
        send_error(req, HTTP_BADREQUEST, -1, "缺少 cmd 字段");
        return;
    }

    std::string cmd = request["cmd"].get<std::string>();
    json args = request.value("args", json::object());

    json result = gm_stub_->exec(cmd, args);
    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_batch_exec(struct evhttp_request* req) {
    std::string body = read_request_body(req);
    if (body.empty()) {
        send_error(req, HTTP_BADREQUEST, -1, "请求体为空");
        return;
    }

    json request;
    try {
        request = json::parse(body);
    } catch (const json::parse_error& e) {
        send_error(req, HTTP_BADREQUEST, -1, std::string("JSON 解析失败: ") + e.what());
        return;
    }

    if (!request.contains("template") || !request["template"].is_string()) {
        send_error(req, HTTP_BADREQUEST, -1, "缺少 template 字段");
        return;
    }
    if (!request.contains("player_id") || !request["player_id"].is_number_unsigned()) {
        send_error(req, HTTP_BADREQUEST, -1, "缺少 player_id 字段");
        return;
    }

    std::string template_name = request["template"].get<std::string>();
    uint64_t player_id = request["player_id"].get<uint64_t>();
    uint32_t server_id = request.value("server_id", 1u);

    json result = gm_stub_->batch_exec(template_name, player_id, server_id);
    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_commands(struct evhttp_request* req) {
    json result = gm_stub_->get_commands();
    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_templates(struct evhttp_request* req) {
    json result = gm_stub_->get_templates();
    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_online_players(struct evhttp_request* req) {
    auto players = player_mgr_->get_all_players();

    json players_json = json::array();
    for (const auto* player : players) {
        json p;
        p["player_id"] = player->player_id();
        p["role_name"] = player->get_role_name();
        p["level"] = player->get_level();
        players_json.push_back(p);
    }

    json result;
    result["code"] = 0;
    result["msg"] = "ok";
    result["players"] = players_json;
    result["count"] = players_json.size();

    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_api_servers(struct evhttp_request* req) {
    // Single server for now (server_id from config)
    json result;
    result["code"] = 0;
    result["servers"] = json::array({
        {{"server_id", 1}, {"name", "game_server_1"}, {"online", player_mgr_->player_count()}}
    });

    send_json_response(req, HTTP_OK, result.dump());
}

void GmHttpHandler::handle_static_file(struct evhttp_request* req) {
    // Serve gm.html
    std::string file_path = static_dir_ + "/gm.html";

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        send_error(req, HTTP_NOTFOUND, -1, "文件不存在: " + file_path);
        return;
    }

    std::ostringstream content;
    content << file.rdbuf();
    std::string body = content.str();

    struct evbuffer* evb = evbuffer_new();
    evbuffer_add(evb, body.c_str(), body.size());

    struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(headers, "Content-Type", "text/html; charset=utf-8");
    evhttp_add_header(headers, "Cache-Control", "no-cache");

    evhttp_send_reply(req, HTTP_OK, "OK", evb);
    evbuffer_free(evb);
}

std::string GmHttpHandler::read_request_body(struct evhttp_request* req) {
    struct evbuffer* input = evhttp_request_get_input_buffer(req);
    if (!input) return "";

    size_t len = evbuffer_get_length(input);
    if (len == 0) return "";

    std::string body(len, '\0');
    evbuffer_copyout(input, &body[0], len);
    return body;
}

void GmHttpHandler::send_json_response(struct evhttp_request* req, int code, const std::string& json_str) {
    struct evbuffer* evb = evbuffer_new();
    evbuffer_add(evb, json_str.c_str(), json_str.size());

    struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(headers, "Content-Type", "application/json; charset=utf-8");
    evhttp_add_header(headers, "Access-Control-Allow-Origin", "*");

    evhttp_send_reply(req, code, "OK", evb);
    evbuffer_free(evb);
}

void GmHttpHandler::send_error(struct evhttp_request* req, int http_code, int app_code, const std::string& msg) {
    json result;
    result["code"] = app_code;
    result["msg"] = msg;
    send_json_response(req, http_code, result.dump());
}

}  // namespace farm
```

- [ ] **Step 3: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/gm_http_handler.h scripts/server/game_server/src/gm_http_handler.cpp
git commit -m "feat(gm): add evhttp-based GM HTTP handler with REST API endpoints"
```

---

### Task 4: Config File

**Files:**
- Create: `config/gm_server.json`

- [ ] **Step 1: Create config/gm_server.json**

```json
{
    "http_port": 7070,
    "static_dir": "static"
}
```

- [ ] **Step 2: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add config/gm_server.json
git commit -m "feat(gm): add GM HTTP server config"
```

---

### Task 5: Integration with GameServer

**Files:**
- Modify: `scripts/server/game_server/src/game_server.h`
- Modify: `scripts/server/game_server/src/game_server.cpp`
- Modify: `scripts/server/game_server/src/main.cpp`
- Modify: `scripts/server/game_server/CMakeLists.txt`

- [ ] **Step 1: Add GM includes and members to game_server.h**

Add after the `#include "online_stub.h"` line:

```cpp
#include "gm_stub.h"
#include "gm_http_handler.h"
```

Add forward declaration after `class AdminHandler;`:

```cpp
class GMStub;
class GmHttpHandler;
```

Add members after `std::unique_ptr<AdminHandler> admin_handler_;`:

```cpp
    // GM system
    std::unique_ptr<GMStub> gm_stub_;
    std::unique_ptr<GmHttpHandler> gm_http_handler_;
    uint16_t gm_http_port_ = 7070;
    std::string gm_static_dir_ = "static";
```

- [ ] **Step 2: Add GM config loading to main.cpp**

Add after the `uint32_t server_id = ...` line (around line 67):

```cpp
    // Parse GM config
    uint16_t gm_http_port = 7070;
    std::string gm_static_dir = "static";
    if (config.contains("gm_server")) {
        gm_http_port = static_cast<uint16_t>(config["gm_server"].value("http_port", 7070));
        gm_static_dir = config["gm_server"].value("static_dir", "static");
    }
```

Modify the GameServer constructor call to pass GM config. Change:

```cpp
    farm::GameServer server(ip, port, dbmgr_configs, redis_uri, server_id);
```

To:

```cpp
    farm::GameServer server(ip, port, dbmgr_configs, redis_uri, server_id, gm_http_port, gm_static_dir);
```

- [ ] **Step 3: Update GameServer constructor signature in game_server.h**

Change the constructor declaration to:

```cpp
    GameServer(const std::string& ip, uint16_t port,
               const std::vector<DBMgrConfig>& dbmgr_configs = {},
               const std::string& redis_uri = "",
               uint32_t server_id = 1,
               uint16_t gm_http_port = 7070,
               const std::string& gm_static_dir = "static");
```

- [ ] **Step 4: Update GameServer constructor implementation in game_server.cpp**

Add `gm_http_port` and `gm_static_dir` parameters to the constructor definition and initializer list. Change the constructor signature at the top of game_server.cpp to match the header, and add:

```cpp
    , gm_http_port_(gm_http_port)
    , gm_static_dir_(gm_static_dir)
```

to the initializer list.

- [ ] **Step 5: Add GMStub and HttpHandler initialization in game_server.cpp start()**

Add after the `admin_handler_` initialization block (after line ~157, before the item handler registration):

```cpp
    // Initialize GM system
    gm_stub_ = std::make_unique<GMStub>(&player_mgr_, &dbmgr_mgr_);
    gm_stub_->init();

    gm_http_handler_ = std::make_unique<GmHttpHandler>(gm_stub_.get(), &player_mgr_);
    if (!gm_http_handler_->start(base_, gm_http_port_, gm_static_dir_)) {
        SPDLOG_ERROR("[Game]Failed to start GM HTTP server on port {}", gm_http_port_);
    } else {
        SPDLOG_INFO("[Game]GM HTTP server started on port {}", gm_http_port_);
    }
```

- [ ] **Step 6: Add GM cleanup in game_server.cpp stop()**

Add before the `event_base_loopexit` call in `stop()`:

```cpp
    // Stop GM HTTP server
    if (gm_http_handler_) {
        gm_http_handler_->stop();
    }
```

- [ ] **Step 7: Update CMakeLists.txt**

Add to the SOURCES list (after `src/admin_handler.cpp`):

```cmake
    src/gm_stub.cpp
    src/gm_http_handler.cpp
```

Add to the TEST_SOURCES list (after `src/admin_handler.cpp`):

```cmake
    src/gm_stub.cpp
    src/gm_http_handler.cpp
```

- [ ] **Step 8: Update game_server.json to include gm_server config**

Add the `gm_server` section to `config/game_server.json`:

```json
{
    "server": {
        "id": 1,
        "ip": "0.0.0.0",
        "port": 6000
    },
    "redis": {
        "uri": "redis://localhost:6379"
    },
    "dbmgrs": [
        {
            "host": "127.0.0.1",
            "port": 5000
        }
    ],
    "gm_server": {
        "http_port": 7070,
        "static_dir": "static"
    },
    "pid_file": "./runtimeData/game_server.pid",
    "logging": {
        "dir": "./runtimeData/logs/server",
        "level": "debug",
        "max_file_size_mb": 20,
        "max_files": 7
    }
}
```

- [ ] **Step 9: Build and verify compilation**

Run:
```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Expected: Build succeeds with no errors.

- [ ] **Step 10: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/src/game_server.h scripts/server/game_server/src/game_server.cpp scripts/server/game_server/src/main.cpp scripts/server/game_server/CMakeLists.txt config/game_server.json
git commit -m "feat(gm): integrate GMStub and GmHttpHandler into GameServer"
```

---

### Task 6: Frontend (Vue.js SPA)

**Files:**
- Create: `scripts/server/game_server/static/gm.html`

- [ ] **Step 1: Create static directory**

```bash
mkdir -p D:/mb_workspace/farm_demo/scripts/server/game_server/static
```

- [ ] **Step 2: Create gm.html**

Create `scripts/server/game_server/static/gm.html` with the complete Vue.js SPA. The file structure:

- Vue 3 CDN import
- Reactive state: currentTab, selectedCommand, serverId, playerId, onlinePlayers, commands, templates, params, outputLog
- Lifecycle: onMounted fetches `/api/gm/commands`, `/api/gm/templates`, `/api/gm/servers`, `/api/gm/online_players`
- Methods: executeCommand(), executeBatch(), loadOnlinePlayers()
- Template: left panel (tabs + command/template list), right panel (params form + execute button + output log)

The full HTML content:

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Farm GM Platform</title>
<script src="https://unpkg.com/vue@3/dist/vue.global.prod.js"></script>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; font-size: 13px; background: #f0f2f5; color: #333; height: 100vh; display: flex; flex-direction: column; }
.topbar { background: #1e1e2e; color: #cdd6f4; padding: 8px 16px; display: flex; justify-content: space-between; align-items: center; flex-shrink: 0; }
.topbar .title { font-weight: bold; font-size: 15px; }
.topbar .info { color: #888; font-size: 11px; }
.main { display: flex; flex: 1; overflow: hidden; }
.left-panel { width: 240px; background: #181825; color: #cdd6f4; display: flex; flex-direction: column; flex-shrink: 0; }
.tabs { display: flex; border-bottom: 1px solid #313244; }
.tab { flex: 1; padding: 10px; text-align: center; cursor: pointer; font-size: 12px; user-select: none; }
.tab.active { background: #313244; color: #89b4fa; font-weight: bold; }
.tab:not(.active) { color: #666; }
.cmd-list { flex: 1; overflow-y: auto; padding: 8px 0; }
.cmd-item { padding: 8px 16px; cursor: pointer; font-size: 12px; border-left: 3px solid transparent; }
.cmd-item:hover { background: #252536; }
.cmd-item.active { background: #313244; color: #89b4fa; border-left-color: #89b4fa; }
.cmd-item:not(.active) { color: #a6adc8; }
.right-panel { flex: 1; background: #f5f5f5; display: flex; flex-direction: column; overflow-y: auto; }
.cmd-header { padding: 12px 20px; background: #fff; border-bottom: 1px solid #e0e0e0; }
.cmd-header .name { font-weight: bold; font-size: 15px; }
.cmd-header .desc { color: #888; font-size: 12px; margin-left: 10px; }
.params { padding: 20px; flex: 1; }
.param-group { margin-bottom: 14px; }
.param-group label { display: block; font-weight: bold; margin-bottom: 5px; color: #555; }
.param-group input, .param-group select { width: 100%; padding: 9px 12px; border: 1px solid #ccc; border-radius: 6px; font-size: 13px; background: #fff; }
.btn { padding: 10px 30px; background: #89b4fa; color: #1e1e2e; border: none; border-radius: 6px; cursor: pointer; font-size: 14px; font-weight: bold; }
.btn:hover { background: #7aa8f0; }
.btn:disabled { background: #ccc; cursor: not-allowed; }
.template-info { padding: 12px 20px; background: #fff3e0; border-bottom: 1px solid #e0e0e0; font-size: 12px; }
.template-info .title { font-weight: bold; color: #e65100; margin-bottom: 6px; }
.template-info .steps { color: #555; padding-left: 10px; }
.output-area { padding: 0 20px 20px; }
.output-area label { display: block; font-weight: bold; margin-bottom: 5px; color: #555; }
.output-box { background: #1e1e2e; color: #a6e3a1; padding: 12px; border-radius: 6px; font-family: 'Consolas', 'Courier New', monospace; font-size: 13px; min-height: 100px; max-height: 250px; overflow-y: auto; white-space: pre-wrap; word-break: break-all; }
.output-box .wait { color: #888; }
.output-box .error { color: #f38ba8; }
.output-box .success { color: #a6e3a1; }
.output-box .info { color: #89b4fa; }
</style>
</head>
<body>
<div id="app">
  <div class="topbar">
    <span class="title">🌾 Farm GM Platform</span>
    <span class="info">game_server:{{ serverPort }}</span>
  </div>
  <div class="main">
    <div class="left-panel">
      <div class="tabs">
        <div class="tab" :class="{active: currentTab==='single'}" @click="currentTab='single'">单条GM</div>
        <div class="tab" :class="{active: currentTab==='batch'}" @click="currentTab='batch'">批量GM</div>
      </div>
      <div class="cmd-list">
        <template v-if="currentTab==='single'">
          <div v-for="cmd in commands" :key="cmd.name"
               class="cmd-item" :class="{active: selectedCommand && selectedCommand.name===cmd.name}"
               @click="selectCommand(cmd)">
            {{ cmd.label }} ({{ cmd.name }})
          </div>
        </template>
        <template v-else>
          <div v-for="tmpl in templates" :key="tmpl.name"
               class="cmd-item" :class="{active: selectedTemplate && selectedTemplate.name===tmpl.name}"
               @click="selectTemplate(tmpl)">
            {{ tmpl.label }}
          </div>
        </template>
      </div>
    </div>
    <div class="right-panel">
      <template v-if="currentTab==='single' && selectedCommand">
        <div class="cmd-header">
          <span class="name">{{ selectedCommand.label }} — {{ selectedCommand.name }}</span>
          <span class="desc">{{ selectedCommand.description }}</span>
        </div>
        <div class="params">
          <div v-for="param in selectedCommand.params" :key="param.name" class="param-group">
            <label>{{ param.label }}<span v-if="param.required" style="color:red">*</span></label>
            <select v-if="param.type==='player'" v-model="paramValues[param.name]" @change="onPlayerChange(param.name)">
              <option value="">-- 选择玩家 --</option>
              <option v-for="p in onlinePlayers" :key="p.player_id" :value="p.player_id">
                {{ p.player_id }} — {{ p.role_name }} (Lv.{{ p.level }})
              </option>
              <option value="__manual__">✏️ 手动输入...</option>
            </select>
            <select v-else-if="param.type==='item'" v-model="paramValues[param.name]">
              <option value="">-- 选择物品 --</option>
              <option v-for="item in itemList" :key="item.id" :value="item.id">
                {{ item.name }} (id:{{ item.id }})
              </option>
            </select>
            <input v-else-if="param.type==='int'" type="number" v-model.number="paramValues[param.name]" :placeholder="'输入'+param.label">
            <input v-else type="text" v-model="paramValues[param.name]" :placeholder="'输入'+param.label">
            <input v-if="param.type==='player' && paramValues[param.name]==='__manual__'"
                   type="number" v-model.number="manualPlayerId" placeholder="手动输入玩家ID"
                   style="margin-top:6px">
          </div>
          <button class="btn" @click="executeSingle" :disabled="executing">
            {{ executing ? '执行中...' : '执行' }}
          </button>
        </div>
      </template>
      <template v-else-if="currentTab==='batch' && selectedTemplate">
        <div class="cmd-header">
          <span class="name">{{ selectedTemplate.label }}</span>
          <span class="desc">{{ selectedTemplate.description }}</span>
        </div>
        <div class="template-info">
          <div class="title">📋 模板内容（共{{ selectedTemplate.steps.length }}条指令）：</div>
          <div class="steps">
            <div v-for="(step, i) in selectedTemplate.steps" :key="i">
              {{ i+1 }}. {{ step.cmd }} → {{ formatArgs(step.args) }}
            </div>
          </div>
        </div>
        <div class="params">
          <div class="param-group">
            <label>目标服务器</label>
            <select v-model="batchServerId">
              <option v-for="s in servers" :key="s.server_id" :value="s.server_id">
                {{ s.name }} — 在线: {{ s.online }}人
              </option>
            </select>
          </div>
          <div class="param-group">
            <label>玩家ID</label>
            <select v-model="batchPlayerId" @change="onBatchPlayerChange">
              <option value="">-- 选择玩家 --</option>
              <option v-for="p in onlinePlayers" :key="p.player_id" :value="p.player_id">
                {{ p.player_id }} — {{ p.role_name }} (Lv.{{ p.level }})
              </option>
              <option value="__manual__">✏️ 手动输入...</option>
            </select>
            <input v-if="batchPlayerId==='__manual__'" type="number" v-model.number="batchManualPlayerId"
                   placeholder="手动输入玩家ID" style="margin-top:6px">
          </div>
          <button class="btn" @click="executeBatch" :disabled="executing">
            {{ executing ? '执行中...' : '执行全部' }}
          </button>
        </div>
      </template>
      <template v-else>
        <div style="display:flex;align-items:center;justify-content:center;height:100%;color:#aaa;">
          请从左侧选择一个GM指令或模板
        </div>
      </template>
      <div class="output-area">
        <label>执行结果</label>
        <div class="output-box" ref="outputBox">
          <template v-if="outputLog.length===0">
            <span class="wait">// 等待执行...</span>
          </template>
          <template v-else>
            <div v-for="(log, i) in outputLog" :key="i" :class="log.type">{{ log.text }}</div>
          </template>
        </div>
      </div>
    </div>
  </div>
</div>
<script>
const { createApp, ref, reactive, onMounted, nextTick } = Vue;

createApp({
  setup() {
    const serverPort = ref(7070);
    const currentTab = ref('single');
    const commands = ref([]);
    const templates = ref([]);
    const servers = ref([]);
    const onlinePlayers = ref([]);
    const selectedCommand = ref(null);
    const selectedTemplate = ref(null);
    const paramValues = reactive({});
    const manualPlayerId = ref(null);
    const batchServerId = ref(1);
    const batchPlayerId = ref('');
    const batchManualPlayerId = ref(null);
    const executing = ref(false);
    const outputLog = ref([]);
    const outputBox = ref(null);

    // Item list (hardcoded, matching server item_effects.cpp)
    const itemList = [
      { id: 1, name: '木材 (wood)' },
      { id: 2, name: '石头 (stone)' },
      { id: 3, name: '斧头 (axe)' },
      { id: 4, name: '锄头 (hoe)' },
      { id: 5, name: '种子 (seed)' },
      { id: 6, name: '面包 (bread)' },
      { id: 7, name: '农作物 (crop)' }
    ];

    async function fetchJSON(url) {
      const resp = await fetch(url);
      return resp.json();
    }

    async function postJSON(url, body) {
      const resp = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      });
      return resp.json();
    }

    async function loadAll() {
      try {
        const [cmdResp, tmplResp, srvResp, plResp] = await Promise.all([
          fetchJSON('/api/gm/commands'),
          fetchJSON('/api/gm/templates'),
          fetchJSON('/api/gm/servers'),
          fetchJSON('/api/gm/online_players')
        ]);
        commands.value = cmdResp.commands || [];
        templates.value = tmplResp.templates || [];
        servers.value = srvResp.servers || [];
        onlinePlayers.value = plResp.players || [];
        if (servers.value.length > 0) batchServerId.value = servers.value[0].server_id;
      } catch (e) {
        appendLog('error', '加载数据失败: ' + e.message);
      }
    }

    async function loadOnlinePlayers() {
      try {
        const resp = await fetchJSON('/api/gm/online_players');
        onlinePlayers.value = resp.players || [];
      } catch (e) {
        // ignore
      }
    }

    function selectCommand(cmd) {
      selectedCommand.value = cmd;
      selectedTemplate.value = null;
      // Reset param values
      Object.keys(paramValues).forEach(k => delete paramValues[k]);
      cmd.params.forEach(p => {
        paramValues[p.name] = p.type === 'int' ? null : '';
      });
      loadOnlinePlayers();
    }

    function selectTemplate(tmpl) {
      selectedTemplate.value = tmpl;
      selectedCommand.value = null;
      batchPlayerId.value = '';
      batchManualPlayerId.value = null;
      loadOnlinePlayers();
    }

    function onPlayerChange(paramName) {
      if (paramValues[paramName] === '__manual__') {
        paramValues[paramName] = '__manual__';
      }
    }

    function onBatchPlayerChange() {
      // just reactive update
    }

    function getEffectivePlayerId() {
      // Find the player_id param
      for (const [k, v] of Object.entries(paramValues)) {
        if (k === 'player_id') {
          if (v === '__manual__') return manualPlayerId.value;
          return v;
        }
      }
      return null;
    }

    function buildArgs() {
      const args = {};
      for (const [k, v] of Object.entries(paramValues)) {
        if (k === 'player_id') {
          const pid = getEffectivePlayerId();
          if (pid) args.player_id = Number(pid);
        } else {
          args[k] = v;
        }
      }
      return args;
    }

    function formatArgs(args) {
      return Object.entries(args).map(([k,v]) => `${k}=${v}`).join(', ');
    }

    function appendLog(type, text) {
      outputLog.value.push({ type, text });
      nextTick(() => {
        if (outputBox.value) {
          outputBox.value.scrollTop = outputBox.value.scrollHeight;
        }
      });
    }

    async function executeSingle() {
      if (!selectedCommand.value) return;
      const args = buildArgs();
      if (!args.player_id) {
        appendLog('error', '错误: 请选择或输入玩家ID');
        return;
      }

      executing.value = true;
      appendLog('info', `> 执行 ${selectedCommand.value.name} ...`);

      try {
        const result = await postJSON('/api/gm/exec', {
          server_id: batchServerId.value,
          cmd: selectedCommand.value.name,
          args: args
        });

        if (result.code === 0) {
          appendLog('success', `✓ ${selectedCommand.value.name}: ${JSON.stringify(result.data || {})}`);
        } else {
          appendLog('error', `✗ ${selectedCommand.value.name}: [${result.code}] ${result.msg}`);
        }
      } catch (e) {
        appendLog('error', '请求失败: ' + e.message);
      } finally {
        executing.value = false;
      }
    }

    async function executeBatch() {
      if (!selectedTemplate.value) return;

      let playerId = batchPlayerId.value;
      if (playerId === '__manual__') playerId = batchManualPlayerId.value;
      if (!playerId) {
        appendLog('error', '错误: 请选择或输入玩家ID');
        return;
      }

      executing.value = true;
      appendLog('info', `> 执行批量模板: ${selectedTemplate.value.label} ...`);

      try {
        const result = await postJSON('/api/gm/batch_exec', {
          server_id: batchServerId.value,
          template: selectedTemplate.value.name,
          player_id: Number(playerId)
        });

        if (result.code === 0 && result.results) {
          for (const r of result.results) {
            if (r.code === 0) {
              appendLog('success', `✓ ${r.cmd}: ${JSON.stringify(r.data || {})}`);
            } else {
              appendLog('error', `✗ ${r.cmd}: [${r.code}] ${r.msg}`);
            }
          }
          const successCount = result.results.filter(r => r.code === 0).length;
          appendLog('info', `批量执行完成: ${successCount}/${result.results.length} 成功`);
        } else {
          appendLog('error', `批量执行失败: [${result.code}] ${result.msg}`);
        }
      } catch (e) {
        appendLog('error', '请求失败: ' + e.message);
      } finally {
        executing.value = false;
      }
    }

    onMounted(loadAll);

    return {
      serverPort, currentTab, commands, templates, servers, onlinePlayers,
      selectedCommand, selectedTemplate, paramValues, manualPlayerId,
      batchServerId, batchPlayerId, batchManualPlayerId,
      executing, outputLog, outputBox, itemList,
      selectCommand, selectTemplate, onPlayerChange, onBatchPlayerChange,
      executeSingle, executeBatch, formatArgs
    };
  }
}).mount('#app');
</script>
</body>
</html>
```

- [ ] **Step 3: Commit**

```bash
cd D:/mb_workspace/farm_demo
git add scripts/server/game_server/static/gm.html
git commit -m "feat(gm): add Vue.js web-based GM management panel"
```

---

### Task 7: Build, Test, and Verify

- [ ] **Step 1: Build the project**

```bash
cd D:/mb_workspace/farm_demo/scripts/server/game_server
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Expected: Build succeeds. `game_server.exe` is produced in `bin/`.

- [ ] **Step 2: Start the server and verify GM HTTP**

Start game_server, then verify:

```bash
# Check GM HTTP is listening
curl http://localhost:7070/api/gm/commands
```

Expected response: JSON with `commands` array containing 8 commands.

```bash
curl http://localhost:7070/api/gm/templates
```

Expected response: JSON with `templates` array containing `player_init`.

```bash
curl http://localhost:7070/api/gm/servers
```

Expected response: JSON with `servers` array.

```bash
curl http://localhost:7070/api/gm/online_players
```

Expected response: JSON with `players` array (empty if no players online).

- [ ] **Step 3: Open browser and verify GM panel**

Open `http://localhost:7070/gm` in browser. Verify:
- Left panel shows command list
- Clicking a command shows its parameters on the right
- Server and player dropdowns load correctly
- Output area shows "等待执行..."

- [ ] **Step 4: Test with an online player (requires game client)**

1. Login with game client to create an online player
2. In GM panel, select SetLevel, pick the online player, set level to 10
3. Click "执行"
4. Output should show: `✓ SetLevel: {"player_id":...,"old_level":1,"new_level":10}`

- [ ] **Step 5: Final commit**

```bash
cd D:/mb_workspace/farm_demo
git add -A
git commit -m "feat(gm): complete GM platform with HTTP API and web panel"
```
