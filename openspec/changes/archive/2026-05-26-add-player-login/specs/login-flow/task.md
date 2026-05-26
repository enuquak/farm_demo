# Login Flow 开发任务

## 任务概览

基于 spec.md 需求，实现玩家登录流程，包括 Proto 定义、Gate Server 消息路由、Game Server 业务处理。

---

## 1. Proto 定义

### 1.1 创建 player.proto [已完成 ✅]
- **文件**: `scripts/common/proto/player.proto`
- **内容**:
  - `PlayerData` message: player_id(uint64), server_id(uint32), role_name(string), level(uint32), exp(uint64), pos_x(float), pos_y(float), created_at(uint64)
  - `EnterGameReq` message: player_id(uint64), server_id(uint32)
  - `EnterGameResp` message: code(int32), msg(string), player_data(PlayerData)
- **说明**: 进入游戏时返回的完整玩家数据

### 1.2 更新 account.proto [已完成 ✅]
- **文件**: `scripts/common/proto/account.proto`
- **内容**:
  - 添加 `QueryRolesResp` 中的 roles 字段使用 RoleInfo（已有）
  - 确保 QueryRolesReq/Resp, CreateRoleReq/Resp 定义完整
- **说明**: 确保账号相关消息定义完整

### 1.3 更新 base.proto [已完成 ✅]
- **文件**: `scripts/common/proto/base.proto`
- **内容**:
  - 删除 `Packet` message（不再使用）
  - 添加 `AccountMsg` message: account_id(string), msg_id(uint32), payload(bytes)
  - 添加 `PlayerMsg` message: player_id(uint64), server_id(uint32), msg_id(uint32), payload(bytes)
- **说明**: 客户端发送的账号/玩家消息封装

### 1.4 生成 Protobuf 代码 [已完成 ✅]
- **命令**: 使用 protoc 生成 C++ 和 Python 代码
- **输出**: `scripts/common/proto/generated/` 目录下的 .pb.h/.pb.cc/_pb2.py 文件

---

## 2. 消息 ID 定义

### 2.1 创建 msg_ids.h（公共消息 ID） [已完成 ✅]
- **文件**: `scripts/common/proto/msg_ids.h`
- **内容**:
  - 账号相关 (1000-1999): MSG_ID_QUERY_ROLES_REQ=1001, MSG_ID_QUERY_ROLES_RESP=1002, MSG_ID_CREATE_ROLE_REQ=1003, MSG_ID_CREATE_ROLE_RESP=1004
  - 玩家相关 (2000-2999): MSG_ID_ENTER_GAME_REQ=2001, MSG_ID_ENTER_GAME_RESP=2002
- **说明**: 客户端与服务器之间的业务消息 ID

---

## 3. Gate Server 扩展

### 3.1 添加 game_servers 配置解析 [已完成 ✅]
- **文件**: `scripts/server/gate_server/src/gate_server.h`, `scripts/server/gate_server/src/gate_server.cpp`
- **内容**:
  - 添加 `GameServerConfig` 结构体: server_id(uint32), ip(string), port(uint16)
  - 添加 `game_servers_` 成员: `std::unordered_map<uint32_t, GameServerConfig>`
  - 添加 `load_game_servers_config(const std::string& config_file)` 方法
  - 在 `start()` 中调用配置加载
- **说明**: 支持多个 Game Server 的配置管理

### 3.2 添加多 Game 连接管理 [已完成 ✅]
- **文件**: `scripts/server/gate_server/src/gate_server.h`, `scripts/server/gate_server/src/gate_server.cpp`
- **内容**:
  - 修改 `game_conn_` 为 `std::unordered_map<uint32_t, std::unique_ptr<GameConnection>> game_conns_`
  - 添加 `get_game_connection(uint32_t server_id)` 方法
  - 添加 `get_any_game_connection()` 方法（轮询选择）
  - 修改 `start()` 中的连接逻辑，连接所有配置的 Game Server
- **说明**: 支持连接多个 Game Server

### 3.3 添加 AccountMsg 路由 [已完成 ✅]
- **文件**: `scripts/server/gate_server/src/gate_server.cpp`
- **内容**:
  - 在 `route_message()` 中添加 AccountMsg 处理（msg_id 在 1000-1999 范围）
  - 将消息封装为 `AccountMessage`（internal.proto）
  - 调用 `get_any_game_connection()` 转发给任意 Game Server
  - 处理 Game 返回的 `AccountMessageResp`，转发给客户端
- **说明**: 账号消息路由到任意 Game Server

### 3.4 添加 PlayerMsg 路由 [已完成 ✅]
- **文件**: `scripts/server/gate_server/src/gate_server.cpp`
- **内容**:
  - 在 `route_message()` 中添加 PlayerMsg 处理（msg_id 在 2000-2999 范围）
  - 解析 `PlayerMsg` 获取 server_id
  - 调用 `get_game_connection(server_id)` 路由到对应 Game Server
  - 处理 Game 返回的响应，转发给客户端
- **说明**: 玩家消息根据 server_id 路由

### 3.5 添加 Session 的 server_id 存储 [已完成 ✅]
- **文件**: `scripts/server/gate_server/src/session.h`, `scripts/server/gate_server/src/session.cpp`
- **内容**:
  - 添加 `server_id_` 成员变量
  - 添加 `set_server_id()` / `server_id()` 方法
- **说明**: 记录玩家选择的服务器 ID

### 3.6 更新 internal_msg_ids.h（Gate） [已完成 ✅]
- **文件**: `scripts/server/gate_server/src/internal_msg_ids.h`
- **内容**:
  - 添加 MSG_ID_ACCOUNT_MSG=3301, MSG_ID_ACCOUNT_MSG_RESP=3302（已有）
  - 确认定义完整
- **说明**: 保持消息 ID 一致性

---

## 4. Game Server 扩展

### 4.1 添加 QueryRolesReq 处理 [已完成 ✅]
- **文件**: `scripts/server/game_server/src/game_server.cpp`
- **内容**:
  - 在 `handle_account_msg()` 中添加 MSG_ID_QUERY_ROLES_REQ 处理
  - 解析 QueryRolesReq，获取 account_id
  - 调用 DBMgr 查询账号角色列表
  - 构造 QueryRolesResp 返回给 Gate
- **说明**: 处理客户端查询角色列表请求

### 4.2 添加 CreateRoleReq 处理 [已完成 ✅]
- **文件**: `scripts/server/game_server/src/game_server.cpp`
- **内容**:
  - 在 `handle_account_msg()` 中添加 MSG_ID_CREATE_ROLE_REQ 处理
  - 解析 CreateRoleReq，获取 account_id, server_id, role_name
  - 生成 player_id（使用雪花算法或简单递增）
  - 调用 DBMgr 创建角色
  - 构造 CreateRoleResp 返回给 Gate
- **说明**: 处理客户端创建角色请求

### 4.3 添加 EnterGameReq 处理 [已完成 ✅]
- **文件**: `scripts/server/game_server/src/game_server.cpp`
- **内容**:
  - 添加 `handle_enter_game_req()` 方法
  - 在 `handle_client_msg()` 中添加 MSG_ID_ENTER_GAME_REQ 分发
  - 解析 EnterGameReq，获取 player_id
  - 从 DBMgr 加载玩家数据
  - 构造 EnterGameResp（包含 PlayerData）返回给 Gate
- **说明**: 处理客户端进入游戏请求

### 4.4 添加 player_id 生成逻辑 [已完成 ✅]
- **文件**: `scripts/server/game_server/src/player_id_generator.h`, `scripts/server/game_server/src/player_id_generator.cpp`
- **内容**:
  - 创建 `PlayerIdGenerator` 类
  - 使用 server_id 作为前缀，递增序列号
  - 格式: player_id = (server_id << 20) | sequence
  - 提供 `generate(uint32_t server_id)` 方法
- **说明**: 生成唯一的 player_id

### 4.5 更新 internal_msg_ids.h（Game） [已完成 ✅]
- **文件**: `scripts/server/game_server/src/internal_msg_ids.h`
- **内容**:
  - 添加 MSG_ID_ACCOUNT_MSG=3301, MSG_ID_ACCOUNT_MSG_RESP=3302（已有）
  - 确认定义完整
- **说明**: 保持消息 ID 一致性

---

## 5. 编译验证

### 5.1 Gate Server 编译 [已完成 ✅]
- **命令**: `cmd /c "D:\mb_workspace\farm_demo\tool\build_cpp14.bat" "D:\mb_workspace\farm_demo\scripts\server\gate_server"`
- **验证**: 编译成功，无错误

### 5.2 Game Server 编译 [已完成 ✅]
- **命令**: `cmd /c "D:\mb_workspace\farm_demo\tool\build_cpp14.bat" "D:\mb_workspace\farm_demo\scripts\server\game_server"`
- **验证**: 编译成功，无错误

---

## 6. 缺陷修复

### 6.1 MSG_ID 常量重复定义 [已完成 ✅]
- **问题**: `message_parser.h` 和 `msg_ids.h` 都定义了相同的消息 ID 常量
- **修复**: 删除 `message_parser.h` 中的重复定义，改为 `#include "msg_ids.h"`
- **文件**: `scripts/server/gate_server/src/message_parser.h`

### 6.2 game_conn_ 成员变量名错误 [已完成 ✅]
- **问题**: `handle_login()` 中使用了 `game_conn_`，但成员变量名是 `game_conns_`
- **修复**: 将 `game_conn_` 改为 `get_any_game_connection()`
- **文件**: `scripts/server/gate_server/src/gate_server.cpp`

### 6.3 SessionManager 缺少 find_by_account_id [已完成 ✅]
- **问题**: `handle_account_msg_resp()` 调用了不存在的 `find_by_account_id()` 方法
- **修复**: 在 `SessionManager` 类中添加 `find_by_account_id()` 方法
- **文件**: `scripts/server/gate_server/src/session_manager.h`, `scripts/server/gate_server/src/session_manager.cpp`

### 6.4 send_player_data_req 函数签名不匹配 [已完成 ✅]
- **问题**: `handle_enter_game_req()` 中调用 `send_player_data_req` 时参数数量和回调签名不正确
- **修复**: 使用正确的参数 `(player_id, GET_ALL, "", "", callback)` 和回调签名 `(int32_t code, const uint8_t* value_data, size_t value_len)`
- **文件**: `scripts/server/game_server/src/game_server.cpp`

---

## 任务依赖关系

```
1.1 player.proto
1.2 account.proto
1.3 base.proto
    ↓
1.4 生成 Protobuf 代码
    ↓
2.1 消息 ID 定义
    ↓
3.1 game_servers 配置解析 ──┐
3.2 多 Game 连接管理 ───────┤
3.5 Session server_id ──────┤
3.6 更新 internal_msg_ids ──┤
                            ↓
3.3 AccountMsg 路由 ────────┤
3.4 PlayerMsg 路由 ─────────┤
                            ↓
4.4 player_id 生成 ─────────┤
4.5 更新 internal_msg_ids ──┤
                            ↓
4.1 QueryRolesReq 处理 ─────┤
4.2 CreateRoleReq 处理 ─────┤
4.3 EnterGameReq 处理 ──────┤
                            ↓
5.1 Gate Server 编译 ───────┤
5.2 Game Server 编译 ───────┘
```

---

## 完成标准

- [ ] 所有 Proto 文件定义正确
- [ ] 消息 ID 定义完整
- [ ] Gate Server 支持 AccountMsg/PlayerMsg 路由
- [ ] Game Server 支持 QueryRoles/CreateRole/EnterGame 处理
- [ ] 编译通过，无错误
- [ ] 代码符合项目规范
