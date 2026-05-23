## Why

Phase 1（Gate + Client）已完成，客户端可以连接 Gate 并完成心跳和登录。但 Gate 目前不转发任何游戏逻辑消息——需要实现 Game Server 作为游戏逻辑的承载进程，Gate 将客户端消息转发到 Game 处理，Game 返回结果再由 Gate 转发回客户端。这是进入游戏功能开发的前提。

## What Changes

- 提取 `scripts/server/common/` 共享网络库，包含 `MessageParser` 和 `msvc_compat`，供 Gate 和 Game 共用
- 新增 `scripts/common/proto/internal.proto`，定义 Gate↔Game 内部通信协议（连接握手、心跳、玩家生命周期、消息转发）
- 实现 `scripts/server/game_server/`，包含 Game Server 核心框架：监听 Gate 连接、消息分发、玩家管理
- 修改 `scripts/server/gate_server/`，新增主动连接 Game、消息转发、Game 断开重连逻辑
- 重新生成 protobuf 代码（C++ 和 Python）

## Capabilities

### New Capabilities
- `game-server`: Game 服务器框架，包含 Gate 连接管理、消息分发、玩家实体管理
- `gate-game-connection`: Gate↔Game 内部通信协议，包含连接握手、心跳检测、玩家生命周期通知、客户端消息转发

### Modified Capabilities
- `gate-server`: 新增主动连接 Game、消息转发到 Game、Game 断开检测与重连逻辑；SessionManager 新增 player_to_game 路由表

## Impact

- `scripts/server/gate_server/` — 重构：提取 MessageParser 和 msvc_compat 到 common；gate_server 新增 Game 连接和转发逻辑
- `scripts/server/game_server/` — 新增：整个 Game Server 目录
- `scripts/server/common/` — 新增：共享网络库
- `scripts/common/proto/internal.proto` — 新增：Gate↔Game 内部协议定义
- `scripts/common/proto/generated/` — 变更：重新生成 protobuf 代码
- `tool/build_cpp14.bat` — 变更：支持构建 game_server
