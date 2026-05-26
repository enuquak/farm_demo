## Why

Game Server 目前没有数据持久化能力，玩家数据（等级、背包、位置等）无法跨会话保存。需要新增一个独立的 DBMgr 进程，作为纯数据存取代理，负责 Game 与数据库之间的连接和读写操作。先用本地 JSON 文件存储快速验证，后续平滑切换到 MongoDB。

## What Changes

- 新增 DBMgr 进程：纯数据存取代理，不做业务逻辑，每玩家一个 JSON 文件（嵌套结构）
- Game Server 与所有 DBMgr 建立全连接（Mesh 拓扑），按 `player_id % dbmgr_count` 路由请求
- 采用异步请求模型：Game 发出请求后不阻塞，通过 `request_id` 匹配回调处理响应
- DBMgr 连接后发送 `DBMgrIdentify` 进行身份标识，随后走心跳保活（与 Gate→Game 模式一致）
- 新增内部消息协议（MsgID 4000-4299）用于 Game ↔ DBMgr 通信
- DBMgr 挂机时 Game 直接返回失败，由上层决定如何处理；新玩家数据不存在时 DBMgr 返回空，由 Game 构造初始数据后 SET 回去
- 配置写死：DBMgr 通过启动参数传 index 和 port，Game 配置文件写死 DBMgr 地址列表

## Capabilities

### New Capabilities

- `dbmgr`: DBMgr 进程本身——TCP 监听、连接管理、身份标识、心跳、JSON 文件读写、PlayerDataReq/Resp 处理
- `game-dbmgr-connection`: Game 侧的 DBMgr 连接管理——主动连接、身份标识、心跳、重连、异步请求队列（request_id + callback）

### Modified Capabilities

- `game-server`: Game Server 需要集成 DBMgr 连接，玩家登录/登出时触发数据加载/保存，新增 Player 数据层抽象

## Impact

- **新增代码**: `scripts/server/dbmgr/` 目录（C++ 项目，libevent + protobuf）
- **修改代码**: `scripts/server/game_server/` 新增 DBMgr 连接管理和数据请求逻辑
- **新增 Proto**: `scripts/common/proto/dbmgr.proto`（DBMgrIdentify、PlayerDataReq/Resp 等消息定义）
- **修改 Proto**: `scripts/common/proto/internal.proto` 或 `internal_msg_ids.h` 新增 4000-4299 消息 ID 范围
- **构建系统**: 新增 dbmgr 的 CMakeLists.txt，构建脚本需编译和启动 dbmgr
- **配置**: Game 和 DBMgr 的启动配置文件
