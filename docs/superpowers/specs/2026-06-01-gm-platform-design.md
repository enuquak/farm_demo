# GM 平台设计文档

## 概述

为 farm_demo 游戏服务器增加 GM（Game Master）系统，通过 HTTP API 提供游戏管理操作，并配套一个网页版 GM 管理平台。

### 目标

- 在 game_server 中嵌入 HTTP 服务（libevent evhttp）
- 提供统一的 GM 指令处理框架（GMStub）
- 提供网页版 GM 管理面板（Vue.js CDN + 纯静态 HTML）
- 支持单条 GM 指令执行和批量 GM 模板执行

### 非目标

- 不涉及服务器停服/起服（已有 AdminHandler）
- 不做认证，仅限内网使用
- 不做交易日志、行为日志、数据统计等高级功能

---

## 架构

```
┌─────────────────────────────────────────────────┐
│                  Browser (Vue.js)                │
│            http://localhost:7070/gm              │
└──────────────────────┬──────────────────────────┘
                       │ HTTP (JSON)
                       ▼
┌─────────────────────────────────────────────────┐
│              game_server (evhttp)                │
│  ┌──────────────┐    ┌────────────────────────┐ │
│  │ HttpHandler   │───▶│       GMStub           │ │
│  │ (解析HTTP请求) │    │  (统一GM指令处理)       │ │
│  └──────────────┘    └───────┬────────┬───────┘ │
│                              │        │         │
│                    ┌─────────▼──┐  ┌──▼───────┐ │
│                    │PlayerManager│  │DBMgrConn │ │
│                    │ (在线玩家)  │  │(离线查询) │ │
│                    └────────────┘  └──────────┘ │
└─────────────────────────────────────────────────┘
```

- **HttpHandler**：基于 libevent evhttp，监听独立端口（默认 7070），解析 HTTP 请求为 GM 指令，调用 GMStub
- **GMStub**：所有 GM 逻辑的统一处理中心，根据指令类型分发执行
- 在线玩家 → 直接操作 Player 对象（标记 dirty，立即生效）
- 离线玩家 → 通过 DBMgrConnectionManager 查询/修改 MongoDB

---

## HTTP API

### 静态文件

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/gm` | 返回 gm.html 前端页面 |

### GM 指令执行

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/gm/exec` | 执行单条 GM 指令 |
| POST | `/api/gm/batch_exec` | 执行批量 GM 模板 |

### 查询

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/gm/online_players?server_id=1` | 获取在线玩家列表 |
| GET | `/api/gm/servers` | 获取可用服务器列表 |
| GET | `/api/gm/templates` | 获取批量模板列表 |
| GET | `/api/gm/commands` | 获取所有可用 GM 指令定义 |

### 请求/响应格式

**POST /api/gm/exec**

请求：
```json
{
  "server_id": 1,
  "cmd": "SetLevel",
  "args": {
    "player_id": 10001,
    "level": 10
  }
}
```

响应（成功）：
```json
{
  "code": 0,
  "msg": "ok",
  "data": { "player_id": 10001, "old_level": 5, "new_level": 10 }
}
```

响应（失败）：
```json
{
  "code": -2,
  "msg": "玩家 10099 不存在"
}
```

**POST /api/gm/batch_exec**

请求：
```json
{
  "server_id": 1,
  "template": "player_init",
  "player_id": 10001
}
```

响应：
```json
{
  "code": 0,
  "results": [
    { "cmd": "SetLevel", "code": 0, "msg": "ok", "data": {...} },
    { "cmd": "SetGold", "code": 0, "msg": "ok", "data": {...} },
    { "cmd": "GiveItem", "code": 0, "msg": "ok", "data": {...} }
  ]
}
```

**GET /api/gm/commands**

响应：
```json
{
  "commands": [
    {
      "name": "SetLevel",
      "label": "设置等级",
      "description": "修改指定玩家的等级",
      "params": [
        { "name": "player_id", "type": "player", "label": "玩家ID", "required": true },
        { "name": "level", "type": "int", "label": "目标等级", "required": true }
      ]
    }
  ]
}
```

`type` 字段决定前端渲染的输入控件：
- `"player"` → 下拉框（在线玩家列表 + 手动输入）
- `"int"` → 数字输入框
- `"string"` → 文本输入框
- `"item"` → 物品选择下拉框

---

## GMStub 设计

### 结构

```
GMStub
├── 注册表: map<string, GmHandler>  // cmd_name → 处理函数
├── 模板表: vector<GmTemplate>      // 预配置的批量模板
│
├── init()                    // 注册所有 GM 指令
├── exec(cmd, args) → result  // 单条执行入口
├── batch_exec(template, player_id) → results  // 批量执行入口
├── get_commands() → 列表     // 返回所有已注册指令的元信息
│
├── handle_set_level(args)
├── handle_set_gold(args)
├── handle_set_exp(args)
├── handle_set_energy(args)
├── handle_give_item(args)
├── handle_del_item(args)
├── handle_query_player(args)
└── handle_kick_player(args)
```

### 已注册的 GM 指令

| 指令名 | 说明 | 参数 |
|--------|------|------|
| SetLevel | 设置玩家等级 | player_id, level |
| SetGold | 设置金币（绝对值） | player_id, gold |
| SetExp | 设置经验值 | player_id, exp |
| SetEnergy | 设置体力 | player_id, energy |
| GiveItem | 发放物品 | player_id, item_id, count |
| DelItem | 删除物品 | player_id, item_id, count |
| QueryPlayer | 查询玩家信息 | player_id |
| KickPlayer | 踢玩家下线 | player_id |

### 批量模板

在 `gm_templates.h` 中定义，纯数据结构：

```cpp
struct GmTemplateStep {
    std::string cmd;           // 指令名
    json args;                 // 固定参数（不含 player_id）
};

struct GmTemplate {
    std::string name;          // 模板标识
    std::string label;         // 显示名称
    std::string description;   // 描述
    std::vector<GmTemplateStep> steps;
};
```

预置模板示例：

| 模板名 | 显示名 | 步骤 |
|--------|--------|------|
| player_init | 玩家初级模板 | SetLevel→10, SetGold→500, GiveItem→bread x20 |

模板执行时，`player_id` 由调用者传入，自动合并到每条指令的 args 中。

### 在线/离线处理

每个 handler 内部逻辑：

1. 从 args 提取 player_id
2. 调用 `PlayerManager::get_player(player_id)` 检查是否在线
3. 在线：直接操作 Player 对象的 setter（自动标记 dirty），数据在下次心跳时持久化
4. 离线：通过 `DBMgrConnectionManager` 发送数据修改请求到 dbmgr，dbmgr 直接修改 MongoDB
5. QueryPlayer 特殊处理：在线返回内存数据，离线从 MongoDB 查询

---

## 前端设计

### 页面布局

```
┌──────────────────────────────────────────────────┐
│ 🌾 Farm GM Platform                    server:7070│
├──────────┬───────────────────────────────────────┤
│ [单条GM] [批量GM] │                                │
│──────────│  SetLevel — 设置等级                    │
││ SetLevel │  ┌─────────────────────────────────┐ │
││ SetGold  │  │ 目标服务器  [game_server_1 ▾]    │ │
││ SetExp   │  │ 玩家ID      [10001-小农夫  ▾]   │ │
││ SetEnergy│  │ 目标等级    [________]           │ │
││ GiveItem │  │                                 │ │
││ DelItem  │  │ [执行]                          │ │
││ Query    │  └─────────────────────────────────┘ │
││ Kick     │  ┌─────────────────────────────────┐ │
│          │  │ 执行结果                          │ │
│          │  │ > [GM] SetLevel player_id=10001   │ │
│          │  │   old=5, new=10  ✓ ok             │ │
│          │  └─────────────────────────────────┘ │
└──────────┴───────────────────────────────────────┘
```

- **左侧**：Tab 切换单条/批量，下方是 GM 指令/模板列表
- **右侧**：选中后显示参数输入 + 执行按钮 + 输出区域
- **服务器选择器**：选择服务器后，玩家ID 自动填充为该服在线玩家下拉列表
- **输出区域**：终端风格，显示执行结果

### 技术方案

- 纯静态 HTML 文件（gm.html），通过 CDN 引入 Vue.js 3
- 由 game_server 的 evhttp 直接提供静态文件服务
- 新增 GM 指令时，前端根据 `/api/gm/commands` 动态渲染参数输入框，无需修改前端代码
- 批量模板根据 `/api/gm/templates` 动态加载

---

## 文件结构

```
scripts/server/game_server/src/
├── gm_stub.h              // GMStub 类定义
├── gm_stub.cpp            // GMStub 实现（所有 GM 指令逻辑）
├── gm_http_handler.h      // HttpHandler 类定义（evhttp）
├── gm_http_handler.cpp    // HttpHandler 实现（路由、JSON 解析、静态文件服务）
├── gm_templates.h         // 批量模板定义（纯数据）

scripts/server/game_server/static/
└── gm.html                // 前端单页应用（Vue.js CDN）

config/
└── gm_server.json         // GM HTTP 服务配置
```

### 与现有代码的集成

- `GameServer::start()` 中初始化 GMStub 和 HttpHandler
- HttpHandler 持有 `GMStub*`、`PlayerManager*`、`DBMgrConnectionManager*` 的指针
- GMStub 持有 `PlayerManager*`、`DBMgrConnectionManager*`
- 复用现有的 spdlog 日志
- CMakeLists 新增 `gm_stub.cpp`、`gm_http_handler.cpp`，libevent 已链接无需新增依赖

---

## 配置

`config/gm_server.json`：

```json
{
  "http_port": 7070,
  "static_dir": "static"
}
```

`static_dir` 相对于 game_server 可执行文件所在目录。在 `game_server.json` 中新增 `gm_server` 字段引用此配置。

---

## 错误处理

| 场景 | code | msg |
|------|------|-----|
| 参数校验失败 | -1 | 参数错误: level 必须为正整数 |
| 玩家不存在 | -2 | 玩家 10099 不存在 |
| 数据库操作失败 | -3 | 数据库操作失败: ... |
| 未知指令 | -100 | 未知指令: XxxXxx |
| 批量中单条失败 | 记录在对应 step 的 code 中 | 继续执行后续指令 |

---

## 日志

- 每次 GM 操作记录到 spdlog：`[GM] SetLevel player_id=10001 level=10 result=ok`
- 批量操作记录总览：`[GM-Batch] player_init player_id=10001 3/3 success`

---

## 扩展性

新增 GM 指令只需三步：

1. 在 `gm_stub.h` 中声明新的 handler 方法
2. 在 `gm_stub.cpp` 的 `init()` 中注册指令名、handler、参数元信息
3. 实现 handler 方法

前端自动适配（通过 `/api/gm/commands` 获取指令定义），无需修改前端代码。
