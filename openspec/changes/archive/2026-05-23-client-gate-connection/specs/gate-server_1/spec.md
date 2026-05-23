## ADDED Requirements

### Requirement: TCP 连接管理
Gate 服务器 SHALL 使用 libevent 监听指定端口，接受客户端 TCP 连接，并维护连接生命周期。

#### Scenario: 服务器启动监听
- **WHEN** Gate 服务器启动
- **THEN** 服务器绑定指定 IP 和端口，开始监听 TCP 连接

#### Scenario: 接受新连接
- **WHEN** 客户端发起 TCP 连接请求
- **THEN** 服务器接受连接，创建 Session 对象，注册到事件循环

#### Scenario: 连接断开清理
- **WHEN** 客户端主动断开或连接异常断开
- **THEN** 服务器检测到断开事件，清理对应 Session 资源

### Requirement: 消息解析
Gate 服务器 SHALL 解析客户端发送的 Protobuf 消息，使用长度前缀解决 TCP 粘包问题。

#### Scenario: 解析完整消息
- **WHEN** 收到完整的消息包（4字节长度 + 4字节MsgID + Payload）
- **THEN** 解析出 MsgID 和 Payload，封装为内部消息结构

#### Scenario: 处理粘包
- **WHEN** 一次收到多个消息包的数据
- **THEN** 按长度前缀逐个拆分，分别解析每个消息

#### Scenario: 处理拆包
- **WHEN** 收到不完整的消息包数据
- **THEN** 缓存数据，等待后续数据到达后继续解析

### Requirement: 会话管理
Gate 服务器 SHALL 为每个客户端连接维护 Session 对象，存储连接状态和玩家信息。

#### Scenario: 创建会话
- **WHEN** 新的客户端连接建立
- **THEN** 创建 Session 对象，记录连接 fd、连接时间、初始状态

#### Scenario: 更新会话状态
- **WHEN** 玩家登录成功或状态变化
- **THEN** 更新 Session 中的玩家 ID、登录状态等信息

#### Scenario: 查找会话
- **WHEN** 需要根据玩家 ID 或连接 fd 查找会话
- **THEN** 从会话管理器中快速定位对应 Session

### Requirement: 心跳检测
Gate 服务器 SHALL 实现心跳检测机制，定期检查客户端连接活性，清理超时连接。

#### Scenario: 收到心跳包
- **WHEN** 收到客户端的心跳消息
- **THEN** 更新该 Session 的最后心跳时间

#### Scenario: 检测超时连接
- **WHEN** 定时检测任务执行，发现某 Session 超过 15 秒未收到心跳
- **THEN** 标记该连接为超时，执行断开清理

#### Scenario: 心跳定时器
- **WHEN** 服务器启动
- **THEN** 创建定时器，每 5 秒执行一次心跳检测

### Requirement: 消息路由
Gate 服务器 SHALL 根据消息类型将消息路由到正确的处理逻辑。

#### Scenario: 处理心跳消息
- **WHEN** 收到 MsgID 为心跳的消息
- **THEN** 直接回复心跳响应，更新 Session 心跳时间

#### Scenario: 处理登录消息
- **WHEN** 收到 MsgID 为登录请求的消息
- **THEN** 解析登录信息，验证后返回登录响应

#### Scenario: 转发游戏消息
- **WHEN** 收到 MsgID 为游戏逻辑的消息（后续实现）
- **THEN** 根据 Session 所在 Game，转发到对应 Game 服务器

### Requirement: 跨平台支持
Gate 服务器 SHALL 使用 libevent 实现跨平台网络 IO，支持 Linux、Windows、macOS。

#### Scenario: Linux 环境运行
- **WHEN** 在 Linux 系统编译运行
- **THEN** 使用 epoll 作为 IO 多路复用后端

#### Scenario: Windows 环境运行
- **WHEN** 在 Windows 系统编译运行
- **THEN** 使用 IOCP 作为 IO 多路复用后端

#### Scenario: macOS 环境运行
- **WHEN** 在 macOS 系统编译运行
- **THEN** 使用 kqueue 作为 IO 多路复用后端

### Requirement: Protobuf 消息定义
Gate 服务器 SHALL 定义并使用标准的 Protobuf 消息格式。

#### Scenario: 心跳消息
- **WHEN** 客户端或服务器发送心跳
- **THEN** 使用 Heartbeat 消息，包含 timestamp 字段

#### Scenario: 登录请求消息
- **WHEN** 客户端发起登录
- **THEN** 使用 LoginReq 消息，包含 token 字段

#### Scenario: 登录响应消息
- **WHEN** 服务器返回登录结果
- **THEN** 使用 LoginResp 消息，包含 code 和 msg 字段

#### Scenario: 通用消息封装
- **WHEN** 传输游戏逻辑消息
- **THEN** 使用 Packet 消息封装，包含 msg_id 和 payload 字段
