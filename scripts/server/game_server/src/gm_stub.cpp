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

    PlayerInventory inv;
    if (!inv.deserialize(player->get_inventory())) {
        return make_error(-3, "解析背包数据失败");
    }

    int32_t max_stack = ItemEffects::get_max_stack(item_id);
    int32_t remaining = inv.add_item(item_id, count, max_stack);
    int32_t added = count - remaining;

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

    player_mgr_->remove_player_with_save(player_id);

    SPDLOG_INFO("[GM]KickPlayer player_id={}", player_id);
    return make_success({{"player_id", player_id}, {"msg", "玩家已被踢下线"}});
}

}  // namespace farm
