---
name: gm-system
description: GM 系统：命令注册、HTTP API、在线/离线处理、批量模板、前端面板。
metadata:
  type: reference
---

# GM 系统

## 概述

GM（Game Master）系统是内嵌于 game_server 的 HTTP-based 游戏管理框架。它通过 libevent evhttp 在独立端口（默认 7070）提供 REST API，支持单条 GM 指令执行和批量模板执行，并配套一个基于 Vue.js 3 的网页版管理面板。

**技术栈：** C++17、libevent (evhttp)、nlohmann/json、spdlog、Vue.js 3 (CDN)

**设计目标：**
- 在 game_server 中嵌入 HTTP 服务，无需额外部署
- 提供统一的 GM 指令处理框架（GMStub），支持动态注册
- 前端根据 `/api/gm/commands` 动态渲染，新增指令无需修改前端代码
- 仅限内网使用，不做认证

---

## 架构设计

### 核心组件

```
Browser (Vue.js)  ──HTTP (JSON)──>  game_server (evhttp)
                                        │
                                   HttpHandler（解析HTTP请求）
                                        │
                                     GMStub（统一GM指令处理）
                                      /      \
                             PlayerManager   DBMgrConnectionManager
                              (在线玩家)       (离线查询)
```

- **GMStub**：所有 GM 逻辑的统一处理中心。内部维护 `map<string, GmHandler>` 命令注册表和 `vector<GmTemplate>` 模板表。通过 `init()` 注册所有指令，通过 `exec()` 分发执行。
- **GmHttpHandler**：基于 libevent evhttp 的 HTTP 服务器，监听端口 7070，解析 HTTP 请求为 GM 指令，调用 GMStub 执行。
- **gm_templates.h**：批量模板的纯数据定义，包含 `GmTemplateStep` 和 `GmTemplate` 结构体。

### 依赖关系

- HttpHandler 持有 `GMStub*`、`PlayerManager*` 指针
- GMStub 持有 `PlayerManager*`、`DBMgrConnectionManager*` 指针
- 在 `GameServer::start()` 中初始化，复用现有的 spdlog 日志
- CMakeLists 中新增 `gm_stub.cpp`、`gm_http_handler.cpp`，libevent 已链接无需新增依赖

---

## 关键流程

### 添加新 GM 命令的完整步骤

1. 在 `gm_stub.h` 中声明新的 handler 方法（如 `handle_xxx`）
2. 在 `gm_stub.cpp` 的 `init()` 中调用 `register_command()`，传入：
   - 命令名（如 `"SetLevel"`）
   - 显示标签（如 `"设置等级"`）
   - 描述
   - 参数元信息 `vector<GmParamDef>`（name、type、label、required）
   - handler lambda
3. 实现 handler 方法，遵循在线/离线处理模式
4. 前端自动适配，无需修改前端代码

### 在线/离线玩家处理

每个 handler 内部的标准流程：

1. 从 args 提取 `player_id`（使用 `extract_player_id()`）
2. 参数校验（类型、范围检查）
3. 调用 `PlayerManager::get_player(player_id)` 检查是否在线
4. **在线**：直接操作 Player 对象的 setter（如 `set_level()`），自动标记 dirty，数据在下次心跳时持久化
5. **离线**：通过 `DBMgrConnectionManager` 发送数据修改请求到 dbmgr，dbmgr 直接修改 MongoDB
6. **QueryPlayer 特殊处理**：在线返回内存数据，离线从 MongoDB 查询

### 批量模板执行

1. 模板定义在 `gm_templates.h` 中，由 `get_preset_templates()` 返回
2. 每个 `GmTemplate` 包含多个 `GmTemplateStep`（cmd + args）
3. 执行时 `player_id` 由调用者传入，自动合并到每条指令的 args 中
4. 逐条执行，单条失败不中断后续指令，结果记录在 `results` 数组中
5. 日志格式：`[GM-Batch] player_init player_id=10001 3/3 success`

---

## HTTP API

### 端点列表

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/gm` | 返回 gm.html 前端页面 |
| POST | `/api/gm/exec` | 执行单条 GM 指令 |
| POST | `/api/gm/batch_exec` | 执行批量 GM 模板 |
| GET | `/api/gm/online_players?server_id=1` | 获取在线玩家列表 |
| GET | `/api/gm/servers` | 获取可用服务器列表 |
| GET | `/api/gm/templates` | 获取批量模板列表 |
| GET | `/api/gm/commands` | 获取所有可用 GM 指令定义 |

### 已注册的 GM 指令

| 指令名 | 说明 | 参数 |
|--------|------|------|
| SetLevel | 设置玩家等级 | player_id, level |
| SetGold | 设置金币（绝对值） | player_id, gold |
| SetExp | 设置经验值 | player_id, exp |
| SetEnergy | 设置体力（0-100） | player_id, energy |
| GiveItem | 发放物品 | player_id, item_id, count |
| DelItem | 删除物品 | player_id, item_id, count |
| QueryPlayer | 查询玩家信息 | player_id |
| KickPlayer | 踢玩家下线 | player_id |

### 参数类型与前端渲染

`type` 字段决定前端渲染的输入控件：
- `"player"` -- 下拉框（在线玩家列表 + 手动输入）
- `"int"` -- 数字输入框
- `"string"` -- 文本输入框
- `"item"` -- 物品选择下拉框

### 错误码

| code | 场景 |
|------|------|
| -1 | 参数校验失败 |
| -2 | 玩家不存在/不在线 |
| -3 | 数据库操作失败 |
| -100 | 未知指令/模板 |

---

## 关键代码路径

| 文件 | 职责 |
|------|------|
| `scripts/server/game_server/src/gm_stub.h` | GMStub 类定义：命令注册表、exec/dispatch、元信息 |
| `scripts/server/game_server/src/gm_stub.cpp` | GMStub 实现：所有 8 个命令 handler、init()、batch_exec() |
| `scripts/server/game_server/src/gm_http_handler.h` | GmHttpHandler 类定义：evhttp 设置、路由分发 |
| `scripts/server/game_server/src/gm_http_handler.cpp` | GmHttpHandler 实现：所有 API 端点、静态文件服务 |
| `scripts/server/game_server/src/gm_templates.h` | 批量模板数据结构（GmTemplateStep、GmTemplate）和预置定义 |
| `scripts/server/game_server/static/gm.html` | 前端单页应用（Vue.js 3 CDN） |
| `config/gm_server.json` | GM HTTP 服务配置（http_port、static_dir） |

---

## 常见陷阱

1. **HTTP 端口冲突**：GM HTTP 服务器默认监听 7070 端口。如果该端口被占用，`evhttp_bind_socket()` 会失败，日志中会输出 `[GM-HTTP]Failed to bind HTTP server on port 7070`。需修改 `config/gm_server.json` 中的 `http_port` 或释放端口。

2. **离线玩家数据未同步**：当前实现中大部分 handler（SetLevel、SetGold 等）仅处理在线玩家，对离线玩家返回 `code: -2`。只有 QueryPlayer 有离线查询路径。如需支持离线修改，需要通过 `DBMgrConnectionManager` 实现 MongoDB 直接写入。

3. **模板参数类型错误**：批量模板中的 args 是 JSON 对象，执行时自动合并 `player_id`。如果模板中 args 的值类型与 handler 期望的不一致（如字符串 vs 整数），会导致参数校验失败。确保 `gm_templates.h` 中的 JSON 值类型正确。

4. **前端静态文件路径**：`static_dir` 配置是相对于 game_server 可执行文件所在目录的。如果 gm.html 找不到，会返回 404。确保 `static` 目录与可执行文件在同一目录层级。

5. **物品 ID 硬编码**：前端 gm.html 中的物品列表（itemList）是硬编码的，与服务器端 `item_effects.cpp` 中的定义对应。新增物品时需同步更新两边。

---

## 扩展指南

### 添加新 GM 命令

三步完成，前端自动适配：

```cpp
// 1. gm_stub.h 中声明
json handle_set_xxx(const json& args);

// 2. gm_stub.cpp init() 中注册
register_command("SetXxx", "设置XXX", "修改指定玩家的XXX",
    {{"player_id", "player", "玩家ID", true}, {"xxx", "int", "目标值", true}},
    [this](const json& args) { return handle_set_xxx(args); });

// 3. 实现 handler
json GMStub::handle_set_xxx(const json& args) {
    uint64_t player_id = extract_player_id(args);
    if (player_id == 0) return make_error(-1, "参数错误: player_id 无效");
    // ... 参数校验、在线检查、执行逻辑
    return make_success({{"player_id", player_id}, ...});
}
```

### 添加新批量模板

在 `gm_templates.h` 的 `get_preset_templates()` 中添加：

```cpp
GmTemplate my_template;
my_template.name = "my_template";
my_template.label = "我的模板";
my_template.description = "模板描述";
my_template.steps = {
    {"SetLevel", json{{"level", 20}}},
    {"SetGold", json{{"gold", 1000}}}
};
templates.push_back(std::move(my_template));
```

---

## 相关 Skill

- [[server-architecture]] -- game_server 整体架构、启动流程、事件循环
- [[player-persistence]] -- Player 对象的持久化机制、dirty 标记、心跳保存、DBMgr 通信
