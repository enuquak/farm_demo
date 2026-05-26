# gate-game-connection 开发任务

## 任务概述
实现 Gate↔Game 内部通信协议，包括消息定义、连接握手、心跳检测、玩家生命周期通知和消息转发。

## 开发任务列表

### 1. 定义内部消息协议 (internal.proto) [已完成 ✅]
- [x] 创建 `scripts/common/proto/internal.proto` 文件
- [x] 定义内部消息类型（MsgID 3000-3299）
- [x] 定义消息格式：INTERN_HEARTBEAT, INTERN_HEARTBEAT_RESP, GATE_IDENTIFY, GATE_IDENTIFY_RESP, PLAYER_JOIN, PLAYER_JOIN_RESP, PLAYER_LEAVE, CLIENT_MSG, GAME_MSG
- [x] 生成 Protobuf C++ 和 Python 代码
- [x] 验证生成的代码可编译

### 2. 协议规范已制定：连接握手功能
- 协议规范已定义在 internal.proto 中
- 具体实现将在 game-server 和 gate-server 任务中完成

### 3. 协议规范已制定：内部心跳检测
- 协议规范已定义在 internal.proto 中
- 具体实现将在 game-server 和 gate-server 任务中完成

### 4. 协议规范已制定：玩家生命周期通知
- 协议规范已定义在 internal.proto 中
- 具体实现将在 game-server 和 gate-server 任务中完成

### 5. 协议规范已制定：客户端消息转发
- 协议规范已定义在 internal.proto 中
- 具体实现将在 game-server 和 gate-server 任务中完成

### 6. 协议规范已制定：玩家路由表管理
- 协议规范已定义在 internal.proto 中
- 具体实现将在 gate-server 任务中完成

## 依赖关系
- 任务 1 是基础，必须首先完成
- 任务 2 依赖任务 1（需要 internal.proto）
- 任务 3 依赖任务 2（需要连接握手）
- 任务 4 依赖任务 2（需要连接握手）
- 任务 5 依赖任务 4（需要玩家生命周期）
- 任务 6 依赖任务 5（需要消息转发）

## 验收标准
- 所有 Protobuf 消息定义正确
- Gate 和 Game 能够完成握手
- 心跳检测正常工作
- 玩家上下线通知正常
- 消息转发功能正常
- 路由表管理正确

## 开发顺序
按照任务编号 1-6 顺序开发，每个任务完成后标记为 [已完成 ✅]
