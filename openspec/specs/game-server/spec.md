# game-server

## Purpose

实现 Game Server 核心框架，接受 Gate 的 TCP 连接并处理转发的游戏逻辑消息。Game Server 使用单线程 libevent 事件循环，支持 Gate 身份识别、内部心跳、玩家生命周期管理和消息分发机制。

## Requirements

### Requirement: TCP 监听
Game 服务器 SHALL 监听指定端口，接受 Gate 的 TCP 长连接。

#### Scenario: 服务器启动监听
- **WHEN** Game 服务器启动
- **THEN** 绑定指定 IP 和端口（默认 127.0.0.1:9090），开始监听 TCP 连接

#### Scenario: 接受 Gate 连接
- **WHEN** Gate 发起 TCP 连接请求
- **THEN** 接受连接，创建 GateSession 对象，注册到事件循环

#### Scenario: Gate 连接断开清理
- **WHEN** Gate 主动断开或连接异常断开
- **THEN** 检测到断开事件，清理该 Gate 关联的所有玩家数据，销毁 GateSession

### Requirement: Gate 身份识别
Game 服务器 SHALL 在 Gate 连入时验证其身份，建立内部通信会话。

#### Scenario: 收到 Gate 身份标识
- **WHEN** 收到 GATE_IDENTIFY 消息（MsgID 3003）
- **THEN** 记录 gate_id，回复 GATE_IDENTIFY_RESP（MsgID 3004），code=0 表示成功

#### Scenario: 未收到身份标识超时
- **WHEN** Gate 连入后 10 秒内未发送 GATE_IDENTIFY
- **THEN** 主动断开该连接

### Requirement: 内部心跳检测
Game 服务器 SHALL 响应 Gate 的心跳请求，维持内部连接活性。

#### Scenario: 收到心跳
- **WHEN** 收到 INTERN_HEARTBEAT 消息（MsgID 3001）
- **THEN** 回复 INTERN_HEARTBEAT_RESP（MsgID 3002），携带当前时间戳

#### Scenario: 心跳超时
- **WHEN** 超过 15 秒未收到 Gate 的心跳
- **THEN** 标记该 Gate 连接为超时，执行断开清理

### Requirement: 玩家生命周期管理
Game 服务器 SHALL 管理玩家的加入和离开，维护在线玩家状态。

#### Scenario: 玩家加入
- **WHEN** 收到 PLAYER_JOIN 消息（MsgID 3101）
- **THEN** 创建 Player 对象，注册到 PlayerManager，回复 PLAYER_JOIN_RESP（MsgID 3102）code=0

#### Scenario: 玩家重复加入
- **WHEN** 收到 PLAYER_JOIN 消息，但该 player_id 已存在
- **THEN** 回复 PLAYER_JOIN_RESP code=1，不重复创建

#### Scenario: 玩家离开
- **WHEN** 收到 PLAYER_LEAVE 消息（MsgID 3103）
- **THEN** 销毁 Player 对象，从 PlayerManager 移除

### Requirement: 客户端消息分发
Game 服务器 SHALL 接收 Gate 转发的客户端消息，根据 MsgID 分发到对应的处理逻辑。

#### Scenario: 收到转发的客户端消息
- **WHEN** 收到 CLIENT_MSG 消息（MsgID 3201）
- **THEN** 解包 player_id、msg_id、payload，查找对应 Player，调用注册的消息 handler

#### Scenario: 消息处理后返回结果
- **WHEN** 消息 handler 处理完成，产生响应
- **THEN** 将响应打包为 GAME_MSG（MsgID 3202），携带 player_id，发送给 Gate

#### Scenario: Player 不存在
- **WHEN** 收到 CLIENT_MSG，但 player_id 对应的 Player 不存在
- **THEN** 记录警告日志，丢弃该消息

### Requirement: 消息处理框架
Game 服务器 SHALL 提供消息 handler 注册机制，支持按 MsgID 注册处理函数。

#### Scenario: 注册消息 handler
- **WHEN** 服务器初始化阶段
- **THEN** 支持 register_handler(msg_id, callback) 注册消息处理函数

#### Scenario: 未注册的消息
- **WHEN** 收到未注册 MsgID 的消息
- **THEN** 记录 "Unhandled msg_id" 日志，丢弃消息

### Requirement: 玩家管理
Game 服务器 SHALL 提供在线玩家的增删查功能。

#### Scenario: 添加玩家
- **WHEN** 调用 add_player(player_id)
- **THEN** 创建 Player 对象并存入管理容器

#### Scenario: 移除玩家
- **WHEN** 调用 remove_player(player_id)
- **THEN** 从管理容器中移除对应 Player

#### Scenario: 查找玩家
- **WHEN** 调用 get_player(player_id)
- **THEN** 返回对应的 Player 对象指针，不存在时返回 nullptr
