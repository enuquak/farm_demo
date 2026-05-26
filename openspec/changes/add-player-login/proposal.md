## Why

Game Server 目前没有完整的账号和角色系统，玩家登录时使用临时的 fd 作为 player_id，无法支持多角色、多服务器的游戏场景。需要实现完整的账号+角色登录流程，支持一个账号在多个服务器拥有不同角色，并通过 DBMgr 进行数据持久化。

## What Changes

- 新增账号+角色系统：一个账号可以拥有多个角色（不同服务器），支持查询角色列表和创建新角色
- 实现完整登录流程：客户端用用户名登录 → 查询角色列表 → 选择角色进入游戏 / 创建新角色
- 扩展消息协议：新增 AccountMsg 和 PlayerMsg 消息类型，替代原有的 Packet
- 新增账号相关消息：QueryRolesReq/Resp、CreateRoleReq/Resp
- 新增玩家相关消息：EnterGameReq/Resp
- 修改 Gate 路由逻辑：支持根据 server_id 路由消息到对应的 Game Server
- 扩展 Player 实体：添加等级、用户名、坐标、场景ID等业务数据字段
- 新增 Game ↔ DBMgr 账号数据消息：AccountDataReq/Resp、AccountSetReq/Resp
- Gate 配置：维护 server_id 到 game_server 的静态配置映射

## Capabilities

### New Capabilities

- `account-system`: 账号系统——账号数据存储、角色列表查询、新角色创建
- `player-entity`: 玩家实体——玩家业务数据（等级、用户名、坐标、场景ID）、数据读写接口
- `login-flow`: 登录流程——账号验证、角色选择、进入游戏

### Modified Capabilities

- `gate-server`: Gate Server 需要支持 AccountMsg 和 PlayerMsg 的路由，维护 server_id 到 Game Server 的配置映射
- `game-server`: Game Server 需要处理账号查询和角色创建逻辑，集成 DBMgr 进行账号和角色数据存取
- `dbmgr`: DBMgr 需要支持账号数据的读写（AccountDataReq/Resp、AccountSetReq/Resp）

## Impact

- **新增代码**: 无新增进程，主要在现有代码基础上扩展
- **修改代码**:
  - `scripts/server/gate_server/`: 消息路由逻辑、配置文件解析
  - `scripts/server/game_server/`: 账号处理、角色管理、Player 实体扩展
  - `scripts/server/dbmgr/`: 账号数据读写支持
- **新增 Proto**: `scripts/common/proto/account.proto`（QueryRolesReq/Resp、CreateRoleReq/Resp）、`scripts/common/proto/player.proto`（EnterGameReq/Resp、PlayerData）
- **修改 Proto**: `scripts/common/proto/base.proto`（Packet 替换为 AccountMsg 和 PlayerMsg）、`scripts/common/proto/internal.proto`（新增账号数据消息）
- **配置**: Gate Server 新增 game_servers 配置文件，DBMgr 新增 accounts 数据目录
- **消息 ID 范围**:
  - 账号相关: 1000-1999
  - 玩家相关: 2000-2999
  - DBMgr 账号数据: 4100-4199
