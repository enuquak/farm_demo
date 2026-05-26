## MODIFIED Requirements

### Requirement: 消息路由
Gate 服务器 SHALL 根据消息类型将消息路由到正确的处理逻辑。心跳和登录消息由 Gate 直接处理；游戏逻辑消息转发到 Game 服务器。

#### Scenario: 处理心跳消息
- **WHEN** 收到 MsgID 为心跳的消息（1001）
- **THEN** 直接回复心跳响应（1002），更新 Session 心跳时间

#### Scenario: 处理登录消息
- **WHEN** 收到 MsgID 为登录请求的消息（2001）
- **THEN** 解析登录信息，验证后返回登录响应（2002），并通知 Game 玩家上线

#### Scenario: 转发游戏消息
- **WHEN** 收到 MsgID 为游戏逻辑的消息（4000+），且 Game 连接已建立
- **THEN** 将 player_id + msg_id + payload 打包为 CLIENT_MSG（3201），转发到 Game

#### Scenario: Game 不可用时处理游戏消息
- **WHEN** 收到 MsgID 为游戏逻辑的消息（4000+），但 Game 未连接
- **THEN** 静默丢弃消息，记录警告日志

## ADDED Requirements

### Requirement: 主动连接 Game
Gate 服务器 SHALL 主动连接 Game 服务器，建立内部 TCP 长连接通道。

#### Scenario: 启动时连接 Game
- **WHEN** Gate 服务器启动后
- **THEN** 尝试连接 Game 服务器（默认 127.0.0.1:9090），发送 GATE_IDENTIFY 完成握手

#### Scenario: 连接 Game 失败
- **WHEN** Game 服务器不可达或连接被拒绝
- **THEN** 启动定时重连（每 5 秒尝试一次），不阻塞 Gate 对客户端的服务

#### Scenario: 收到 Game 转发的响应
- **WHEN** 收到 GAME_MSG（MsgID 3202）
- **THEN** 解包 player_id + msg_id + payload，查找客户端 Session，直接用原始 msg_id + payload 发送给客户端

### Requirement: Game 断开检测与重连
Gate 服务器 SHALL 检测 Game 连接断开，清理路由并定时重连。

#### Scenario: Game 连接断开
- **WHEN** 检测到 Game TCP 连接断开（BEV_EVENT_EOF 或 ERROR）
- **THEN** 清理 player_to_game 路由表，启动定时重连

#### Scenario: 重连成功
- **WHEN** 定时重连触发且连接 Game 成功
- **THEN** 发送 GATE_IDENTIFY 完成握手，恢复正常转发（已登录玩家需要重新通知 Game）

#### Scenario: 重连期间客户端消息处理
- **WHEN** Game 断开期间收到客户端游戏逻辑消息
- **THEN** 静默丢弃，记录警告日志

### Requirement: 玩家路由管理
Gate 服务器 SHALL 维护 player_id 到 Game 连接的映射表，用于消息路由。

#### Scenario: 注册玩家路由
- **WHEN** 玩家登录成功且 Game 确认 PLAYER_JOIN
- **THEN** 在 player_to_game 表中记录 player_id → game_conn

#### Scenario: 注销玩家路由
- **WHEN** 玩家断开连接或 Game 断开
- **THEN** 从 player_to_game 表移除对应记录
