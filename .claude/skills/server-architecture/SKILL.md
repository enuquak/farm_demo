---
name: server-architecture
description: 服务器三进程架构、消息协议、服务生命周期、配置系统、服务发现、共享常量、构建系统。
metadata:
  type: reference
---

# 服务器架构

## 概述

项目采用三进程架构，各进程职责单一，通过 TCP 长连接通信：

| 进程 | 默认端口 | 职责 |
|------|----------|------|
| **Gate Server** | 8080 | 客户端入口网关，处理客户端连接、消息路由、心跳、登录 |
| **Game Server** | 9090 | 游戏逻辑处理，玩家生命周期管理，消息分发 |
| **DBMgr** | 5000 | 纯数据存取代理，玩家数据 CRUD（初期为 JSON 文件） |

通信链路：`客户端 → Gate(8080) → Game(9090) → DBMgr(5000)`

## 架构设计

### 三进程架构图

```
┌─────────────┐       TCP:8080       ┌─────────────┐       TCP:9090       ┌─────────────┐
│   客户端    │ ──────────────────→ │ Gate Server │ ──────────────────→ │ Game Server │
│  (Python)   │ ←────────────────── │             │ ←────────────────── │             │
└─────────────┘    心跳/登录/游戏    └─────────────┘    内部消息转发      └─────────────┘
                                                                 │
                                                                 │ TCP:5000
                                                                 ↓
                                                           ┌─────────────┐
                                                           │    DBMgr    │
                                                           │             │
                                                           └─────────────┘
                                                                  │
                                                                  ↓
                                                           data-dir/players/*.json
```

### 消息协议

#### 线格式

所有消息使用统一的二进制线格式：

```
┌──────────────┬──────────────┬─────────────────────┐
│  Length (4B) │  MsgID (4B)  │  Protobuf Payload   │
│  大端序      │  大端序      │  变长               │
└──────────────┴──────────────┴─────────────────────┘
```

- **Length**: 整个消息的字节长度（含 Length 自身 4 字节 + MsgID 4 字节 + Payload）
- **MsgID**: 消息类型标识，决定 Payload 的 Protobuf schema
- **Payload**: Protobuf 序列化的消息体

#### MsgID 分区表

MsgID 按功能分区分配，避免冲突：

| 范围 | 用途 | 说明 |
|------|------|------|
| 1-999 | 客户端基础消息 | 心跳(1,2)、登录(3,4) |
| 1001-1999 | 角色相关 | 查询角色(1001,1002)、创建角色(1003,1004)、进入游戏(1005,1006) |
| 2001-2999 | 场景/位置/时钟 | 地图数据(2001)、位置更新(2101)、场景切换(2201)、时钟同步(2301) |
| 3001-3099 | Gate-Game 连接管理 | 内部心跳(3001,3002)、身份识别(3003,3004) |
| 3100-3199 | 玩家生命周期 | 加入(3101,3102)、离开(3103) |
| 3200-3299 | 消息转发 | 客户端消息(3201)、游戏消息(3202) |
| 3300-3399 | 账号消息转发 | 账号消息(3301,3302) |
| 4000-4099 | Game-DBMgr 连接管理 | 身份标识(4001,4002)、心跳(4003,4004) |
| 4100-4199 | 数据操作 | 玩家数据请求/响应(4101,4102) |
| 4200-4299 | 账号数据操作 | 账号数据(4201-4204)、玩家ID分配(4205,4206) |
| 5000-5999 | 管理消息 | 停服(5001,5002) |

客户端消息 ID 定义在 `shared/message_ids.json`，自动生成：
- C++: `scripts/server/common/include/message_ids.h`
- Python: `scripts/client/message_ids.py`

内部消息 ID 定义在：
- `scripts/server/common/include/internal_msg_ids.h`（Gate-Game、Game-DBMgr）
- `scripts/server/common/include/admin_msg_ids.h`（管理消息）

### Protobuf 定义

Proto 文件位于 `scripts/common/proto/`：

| 文件 | 用途 |
|------|------|
| `base.proto` | 基础消息类型 |
| `account.proto` | 账号相关消息 |
| `player.proto` | 玩家数据消息 |
| `internal.proto` | 内部通信消息（Gate-Game、Game-DBMgr） |
| `dbmgr.proto` | DBMgr 数据操作消息 |

### 消息处理框架

Game Server 提供消息 handler 注册机制：

```cpp
// 注册消息处理函数
server.register_handler(MSG_ID_ENTER_GAME_REQ, [](Player* player, const std::string& payload) {
    // 处理进入游戏请求
});

// 收到未注册 MsgID 时记录 "Unhandled msg_id" 日志并丢弃
```

Gate Server 消息路由规则：
- 心跳(1,2)、登录(3,4): Gate 直接处理
- 游戏逻辑(1001+): 打包为 CLIENT_MSG(3201) 转发到 Game
- Game 不可用时: 静默丢弃，记录警告日志

## 关键流程

### 服务启动顺序

必须按依赖顺序启动，间隔 2 秒：

```
1. 启动 dbmgr          (bin/dbmgr.exe --config config/dbmgr.json)
2. 等待 2 秒
3. 启动 game_server    (bin/game_server.exe --config config/game_server.json)
4. 等待 2 秒
5. 启动 gate_server    (bin/gate_server.exe --config config/gate_server.json)
```

一键启动脚本: `tools/start_all.bat`

### PID 文件管理

各服务启动后将 PID 写入配置文件指定的 `pid_file` 路径：

| 进程 | PID 文件路径 |
|------|-------------|
| dbmgr | `runtimeData/dbmgr.pid` |
| game_server | `runtimeData/game_server.pid` |
| gate_server | `runtimeData/gate_server.pid` |

- PID 文件内容仅为数字，无其他字符
- 启动时覆盖写入
- 自动创建 `runtimeData/` 目录

### 优雅关闭

通过发送管理消息实现级联停服：

```
stop_graceful.bat
    │
    ↓ 连接 gate_server，发送 MSG_ID_SHUTDOWN(5001)
    │
    ↓ gate_server: 停止接受新连接，向所有 game_server 发送 MSG_ID_SHUTDOWN
    │
    ↓ game_server: 停止接受新连接，向所有 dbmgr 发送 MSG_ID_SHUTDOWN
    │
    ↓ dbmgr: 保存所有数据，发送 MSG_ID_SHUTDOWN_RESP(5002)，退出
    │
    ↓ game_server: 收到响应后退出
    │
    ↓ gate_server: 收到响应后退出
```

超时处理：默认 60 秒，超时后强制终止（`taskkill /PID <pid> /F`）。

强制停服脚本: `tools/stop_force.bat`（直接读取 PID 文件强制终止）

### 服务连接建立

#### Gate → Game 连接

1. Gate 启动后主动连接 Game（默认 127.0.0.1:9090）
2. 发送 GATE_IDENTIFY(3003) 完成握手
3. Game 回复 GATE_IDENTIFY_RESP(3004)
4. 连接失败时每 5 秒重试，不阻塞客户端服务
5. 断开后清理 player_to_game 路由表，启动重连

#### Game → DBMgr 连接

1. Game 启动时读取配置中的 `dbmgr_list`，为每个 DBMgr 创建连接
2. DBMgr 连接建立后主动发送 DBMGR_IDENTIFY(4001)
3. Game 回复 DBMGR_IDENTIFY_RESP(4002)
4. DBMgr 每 5 秒发送 DBMGR_HEARTBEAT(4003)
5. 连接断开时标记 DBMgr 为不可用，持续重连

### 玩家数据流程

```
玩家登录 → Gate 转发到 Game → Game 向 DBMgr 发送 GET_ALL
    → DBMgr 读取 data-dir/players/N.json → 返回数据
    → Game 创建 Player 对象，填充数据

玩家离开 → Game 将 Player 数据通过 SET_ALL 写入 DBMgr
    → DBMgr 写入 data-dir/players/N.json
```

新玩家首次加入时，DBMgr 返回空数据，Game 构造初始数据并写入。

## 关键代码路径

### 服务器进程

| 路径 | 说明 |
|------|------|
| `scripts/server/gate_server/src/main.cpp` | Gate Server 入口 |
| `scripts/server/gate_server/src/gate_server.h` | Gate Server 核心类 |
| `scripts/server/gate_server/src/gate_server.cpp` | Gate Server 实现 |
| `scripts/server/game_server/src/main.cpp` | Game Server 入口 |
| `scripts/server/game_server/src/game_server.h` | Game Server 核心类 |
| `scripts/server/game_server/src/game_server.cpp` | Game Server 实现 |
| `scripts/server/dbmgr/src/main.cpp` | DBMgr 入口 |
| `scripts/server/dbmgr/src/dbmgr.h` | DBMgr 核心类 |
| `scripts/server/dbmgr/src/dbmgr.cpp` | DBMgr 实现 |

### 公共模块

| 路径 | 说明 |
|------|------|
| `scripts/server/common/include/internal_msg_ids.h` | 内部消息 ID 常量 |
| `scripts/server/common/include/admin_msg_ids.h` | 管理消息 ID 常量 |
| `scripts/server/common/include/message_ids.h` | 客户端消息 ID（自动生成） |
| `scripts/server/common/include/error_codes.h` | 错误码（自动生成） |
| `scripts/server/common/include/message_parser.h` | 消息解析器 |
| `scripts/server/common/include/log_macros.h` | 日志宏 |
| `scripts/server/common/include/log_modules.h` | 日志模块常量 |
| `scripts/server/common/include/log_config.h` | 日志配置 |
| `scripts/server/common/include/log_init.h` | 日志初始化 |
| `scripts/server/common/include/server_main_helper.h` | 服务启动辅助 |
| `scripts/server/common/include/game_constants.h` | 游戏常量 |
| `scripts/server/common/include/etcd_manager.h` | etcd 服务发现管理器 |

### Protobuf 定义

| 路径 | 说明 |
|------|------|
| `scripts/common/proto/base.proto` | 基础消息类型 |
| `scripts/common/proto/account.proto` | 账号消息 |
| `scripts/common/proto/player.proto` | 玩家数据消息 |
| `scripts/common/proto/internal.proto` | 内部通信消息 |
| `scripts/common/proto/dbmgr.proto` | DBMgr 数据操作消息 |

### 配置文件

| 路径 | 说明 |
|------|------|
| `config/gate_server.json` | Gate Server 配置 |
| `config/game_server.json` | Game Server 配置 |
| `config/dbmgr.json` | DBMgr 配置 |

### 共享常量

| 路径 | 说明 |
|------|------|
| `shared/message_ids.json` | 消息 ID 源定义 |
| `shared/error_codes.json` | 错误码源定义 |
| `shared/item_ids.json` | 物品 ID 源定义 |
| `scripts/tools/generate_constants.py` | 常量代码生成脚本 |
| `scripts/tools/file_watcher.py` | 文件监控服务 |

### 工具脚本

| 路径 | 说明 |
|------|------|
| `tools/build_all.bat` | 编译所有服务 |
| `tools/start_all.bat` | 按顺序启动所有服务 |
| `tools/stop_graceful.bat` | 优雅停服 |
| `tools/stop_force.bat` | 强制停服 |

### 第三方库

| 路径 | 说明 |
|------|------|
| `scripts/common/include/nlohmann/json.hpp` | JSON 解析库（header-only） |
| `scripts/common/third_party/spdlog/` | 日志库（header-only） |

## 配置系统

### 配置文件结构

所有配置文件位于 `config/` 目录，使用 JSON 格式，通过 nlohmann/json 库解析。

#### gate_server.json

```json
{
  "server": {
    "ip": "0.0.0.0",
    "port": 8080
  },
  "game_servers": [
    { "server_id": 1, "ip": "127.0.0.1", "port": 9090 }
  ],
  "shutdown": { "timeout_ms": 60000 },
  "pid_file": "./runtimeData/gate_server.pid"
}
```

#### game_server.json

```json
{
  "server": {
    "ip": "0.0.0.0",
    "port": 9090
  },
  "dbmgrs": [
    { "host": "127.0.0.1", "port": 5000 }
  ],
  "shutdown": { "timeout_ms": 60000 },
  "pid_file": "./runtimeData/game_server.pid"
}
```

#### dbmgr.json

```json
{
  "server": {
    "index": 0,
    "ip": "0.0.0.0",
    "port": 5000,
    "data_dir": "./data"
  },
  "database": { "uri": "", "type": "file" },
  "shutdown": { "timeout_ms": 60000 },
  "pid_file": "./runtimeData/dbmgr.pid"
}
```

### 配置加载机制

- 服务启动时从 JSON 配置文件读取参数
- 配置文件不存在或格式错误时输出错误信息并退出
- 统一编译输出到 `bin/` 目录

## 日志系统

### 日志格式

统一格式：`[时间][级别][进程名:PID][模块][内容]`

示例：
```
[2026-05-26 14:30:05.123][INFO ][gate_server:12345][Gate      ] 新玩家连接: 192.168.1.100
[2026-05-26 14:30:06.456][ERROR][dbmgr:12346     ][DBMgr     ] 查询超时: player_data
```

### 日志级别

支持 4 个级别：DEBUG、INFO、ERROR、CRITICAL

通过配置文件 `logging.level` 设置最低输出级别。

### 日志轮转

- 按日轮转：每天 00:00 自动创建新文件
- 按大小轮转：单文件超过 20MB 时创建新文件
- 保留最近 7 个历史文件

### 日志目录

- 服务器：`runtimeData/logs/server/`
- 客户端：`runtimeData/logs/client/`

## 服务发现（etcd）

### 设计目标

引入 etcd 作为服务注册中心和配置中心，实现进程动态发现与配置统一管理。

### etcd Key 结构

```
/farm/
├── services/                    # 服务注册（绑定 lease，自动过期）
│   ├── gate/{instance_id}       → {"ip":"0.0.0.0","port":8080,"status":"online"}
│   ├── game/{server_id}         → {"ip":"0.0.0.0","port":9090,"server_id":1}
│   └── dbmgr/{index}            → {"ip":"0.0.0.0","port":5000,"index":0}
│
└── config/                      # 配置中心（持久化，不绑定 lease）
    └── servers/
        ├── gate/{instance_id}
        ├── game/{server_id}
        └── dbmgr/{index}
```

### 关键行为

- Lease TTL: 15 秒，进程崩溃后自动清理注册信息
- 进程启动时：先从 `config/` 读取配置，再向 `services/` 注册自己
- 服务发现：watch `services/` 下对应目录，实时感知节点变更

### EtcdManager 接口

```cpp
class EtcdManager {
    bool connect();
    void shutdown();
    bool register_service(service_type, instance_id, value_json);
    void deregister_service(service_type, instance_id);
    std::vector<ServiceInstance> discover_services(service_type);
    void watch_services(service_type, callback);
    bool put_config(key, value_json);
    std::optional<std::string> get_config(key);
    void watch_config(key_prefix, callback);
};
```

### 集成方式

本地配置文件简化为只保留 etcd 地址，原有的 `game_servers[]`、`dbmgrs[]` 由 etcd 动态提供。

## 共享常量系统

### 设计原则

- **单一数据源**：所有共享常量只在 `shared/` 目录下的 JSON 文件中定义
- **自动生成**：文件变化时自动触发代码生成
- **向后兼容**：保持现有 `#include` 和 `import` 路径不变

### 数据流

```
shared/*.json → file_watcher.py → generate_constants.py → scripts/client/*.py
                                                           scripts/server/common/include/*.h
```

### 配置文件格式

```json
{
  "message_ids": [
    { "code": 1, "name": "MSG_ID_HEARTBEAT", "description": "心跳消息" }
  ]
}
```

### 生成产物

- Python: `scripts/client/{config_name}.py`（class 常量）
- C++: `scripts/server/common/include/{config_name}.h`（enum class）

### 开发流程

1. 启动文件监控：`python scripts/tools/file_watcher.py`
2. 修改 `shared/` 目录下的 JSON 文件
3. 自动生成代码文件

构建时在 `build_all.bat` 中调用 `python scripts/tools/generate_constants.py`。

## 常见陷阱

### DLL 缺失

Windows 下编译后运行时报 DLL 缺失错误：
- 检查 `bin/` 目录下是否有所有依赖的 DLL（libevent、protobuf 等）
- 使用 `tools/build_all.bat` 确保 DLL 复制到输出目录

### 端口冲突

服务启动失败，报端口被占用：
- 检查 `config/*.json` 中的端口配置
- 使用 `netstat -ano | findstr :8080` 查找占用进程
- 确认没有残留的旧进程（检查 PID 文件）

### 启动顺序错误

Game Server 启动后无法连接 DBMgr：
- 必须先启动 DBMgr，等待 2 秒后再启动 Game Server
- 使用 `tools/start_all.bat` 自动处理顺序

### MsgID 不匹配

客户端消息无响应或服务器报 "Unhandled msg_id"：
- 确认客户端和服务器使用相同的 `shared/message_ids.json` 生成代码
- 修改 MsgID 后需要重新运行 `generate_constants.py`
- 检查 `internal_msg_ids.h` 中的内部 MsgID 是否与客户端消息 ID 冲突

### 配置文件格式错误

服务启动时报 JSON 解析错误：
- 使用 JSON 验证工具检查配置文件
- 注意逗号、引号等语法细节
- 错误信息会包含具体的解析错误详情

### etcd 连接失败

服务启动时报 etcd 连接失败：
- 确认 etcd 服务已启动
- 检查配置中的 `etcd.endpoints` 地址是否正确
- etcd 为硬依赖，连接失败时进程会退出

### 数据目录不存在

DBMgr 启动时报数据目录错误：
- DBMgr 启动时会自动创建 `data-dir/players/` 目录
- 检查 `config/dbmgr.json` 中的 `data_dir` 配置

## 扩展指南

### 添加新服务器进程

1. 创建进程目录：`scripts/server/new_server/src/`
2. 实现 `main.cpp`，使用 `server_main_helper.h` 中的辅助函数
3. 创建 `CMakeLists.txt`，链接公共模块和第三方库
4. 添加配置文件：`config/new_server.json`
5. 更新启动脚本 `tools/start_all.bat`，在合适的位置插入新进程
6. 更新停服脚本，确保级联关闭包含新进程
7. 如需 etcd 集成，在 `EtcdManager` 中注册新服务类型

### 添加新 MsgID

1. 在 `shared/message_ids.json` 中添加新条目（客户端消息）
2. 运行 `python scripts/tools/generate_constants.py` 生成代码
3. 在 `scripts/common/proto/` 中定义对应的 Protobuf 消息
4. 在 Game Server 中注册消息 handler：`server.register_handler(MSG_ID, callback)`
5. 如为内部消息，在 `internal_msg_ids.h` 中添加常量

### 添加新日志模块

1. 在 `scripts/server/common/include/log_modules.h` 中添加：
   ```cpp
   constexpr std::string_view NewModule = "NewModule";
   ```
2. 在 `scripts/client/log_modules.py` 中添加：
   ```python
   NewModule = "NewModule"
   ```
3. 使用 `LOG_INFO(LogModule::NewModule, "消息内容")` 输出日志

## 相关 Skill

- [[dbmgr-data-layer]] — DBMgr 数据层详细设计
- [[player-persistence]] — 玩家数据持久化机制
- [[gm-system]] — GM 管理系统
