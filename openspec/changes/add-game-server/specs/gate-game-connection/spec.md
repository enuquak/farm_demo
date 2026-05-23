## ADDED Requirements

### Requirement: 内部消息协议
Gate↔Game 内部通信 SHALL 使用独立的 Protobuf 消息定义，与客户端↔Gate 的外部协议隔离。

#### Scenario: 内部消息格式
- **WHEN** Gate 或 Game 发送内部消息
- **THEN** 使用 internal.proto 定义的消息类型，Wire 格式与外部一致（4B Length + 4B MsgID + Payload）

#### Scenario: MsgID 空间隔离
- **WHEN** 消息在内部通道传输
- **THEN** MsgID 使用 3000-3299 范围，不与外部 MsgID（1000-2999）冲突

### Requirement: 连接握手
Gate 连接 Game 后 SHALL 完成身份标识握手，建立信任关系。

#### Scenario: Gate 发送身份标识
- **WHEN** Gate 成功连接 Game
- **THEN** Gate 发送 GATE_IDENTIFY（MsgID 3003），包含 gate_id

#### Scenario: Game 确认身份
- **WHEN** Game 收到 GATE_IDENTIFY 并验证通过
- **THEN** Game 回复 GATE_IDENTIFY_RESP（MsgID 3004），code=0

#### Scenario: 握手完成前不处理业务消息
- **WHEN** Gate 尚未收到 GATE_IDENTIFY_RESP
- **THEN** Gate 不发送玩家相关消息（PLAYER_JOIN / CLIENT_MSG）

### Requirement: 内部心跳
Gate↔Game 连接 SHALL 维持应用层心跳，检测连接活性。

#### Scenario: Gate 发送心跳
- **WHEN** 连接建立且握手完成后，每 5 秒
- **THEN** Gate 发送 INTERN_HEARTBEAT（MsgID 3001），携带 timestamp

#### Scenario: Game 回应心跳
- **WHEN** Game 收到 INTERN_HEARTBEAT
- **THEN** Game 回复 INTERN_HEARTBEAT_RESP（MsgID 3002），携带 timestamp

#### Scenario: 心跳超时断开
- **WHEN** Gate 超过 15 秒未收到 INTERN_HEARTBEAT_RESP
- **THEN** Gate 标记 Game 连接为超时，触发断开处理和重连

### Requirement: 玩家生命周期通知
Gate SHALL 在玩家上线和下线时通知 Game。

#### Scenario: 玩家上线通知
- **WHEN** 客户端登录成功（Session 状态变为 LOGGED_IN）
- **THEN** Gate 发送 PLAYER_JOIN（MsgID 3101）给 Game，携带 player_id

#### Scenario: 玩家下线通知
- **WHEN** 客户端断开连接或心跳超时
- **THEN** Gate 发送 PLAYER_LEAVE（MsgID 3103）给 Game，携带 player_id

#### Scenario: Game 确认玩家加入
- **WHEN** Gate 收到 PLAYER_JOIN_RESP
- **THEN** 记录该 player_id 已在 Game 注册成功（或失败）

### Requirement: 客户端消息转发
Gate SHALL 将客户端的游戏逻辑消息转发到 Game，并将 Game 的响应转发回客户端。

#### Scenario: 转发客户端消息到 Game
- **WHEN** Gate 收到客户端消息，MsgID 属于游戏逻辑范围（4000+）
- **THEN** 将 player_id + msg_id + payload 打包为 CLIENT_MSG（MsgID 3201），发送给 Game

#### Scenario: 转发 Game 响应到客户端
- **WHEN** Gate 收到 GAME_MSG（MsgID 3202）
- **THEN** 解包 player_id + msg_id + payload，查找客户端 Session，用原始 msg_id + payload 直接发送

#### Scenario: Game 不可用时丢弃消息
- **WHEN** Gate 未连接到 Game，收到客户端游戏逻辑消息
- **THEN** 静默丢弃消息，记录警告日志

### Requirement: 玩家路由表
Gate SHALL 维护 player_id 到 Game 的映射关系，用于消息路由。

#### Scenario: 注册玩家路由
- **WHEN** 收到 PLAYER_JOIN_RESP code=0
- **THEN** 在 player_to_game 表中记录 player_id → game_conn 映射

#### Scenario: 注销玩家路由
- **WHEN** 玩家下线或 Game 断开
- **THEN** 从 player_to_game 表中移除对应 player_id

#### Scenario: 查找路由
- **WHEN** 需要转发客户端消息
- **THEN** 从 player_to_game 表查找目标 Game 连接
