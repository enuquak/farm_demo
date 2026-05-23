# client-connection_2 Specification

## Purpose
TBD - created by archiving change client-gate-connection. Update Purpose after archive.
## Requirements
### Requirement: TCP 连接建立
客户端连接模块 SHALL 支持与 Gate 服务器建立 TCP 长连接。

#### Scenario: 连接到服务器
- **WHEN** 客户端启动并指定服务器地址和端口
- **THEN** 创建 TCP socket，发起连接，连接成功后进入就绪状态

#### Scenario: 连接失败处理
- **WHEN** 服务器不可达或连接被拒绝
- **THEN** 抛出连接异常，包含失败原因

#### Scenario: 连接超时处理
- **WHEN** 连接尝试超过指定时间未成功
- **THEN** 抛出超时异常，可配置超时时间

### Requirement: 消息收发
客户端连接模块 SHALL 支持 Protobuf 消息的发送和接收，使用长度前缀解决粘包问题。

#### Scenario: 发送消息
- **WHEN** 调用发送接口传入 MsgID 和 Protobuf 消息
- **THEN** 序列化消息，添加长度前缀，通过 TCP 发送

#### Scenario: 接收消息
- **WHEN** TCP 缓冲区有数据到达
- **THEN** 按长度前缀读取完整消息，反序列化为 Protobuf 对象

#### Scenario: 处理粘包
- **WHEN** 一次接收到多个消息的数据
- **THEN** 按长度前缀逐个拆分，分别处理每个消息

#### Scenario: 处理拆包
- **WHEN** 接收到不完整的消息数据
- **THEN** 缓存数据，等待后续数据到达后继续处理

### Requirement: 心跳维持
客户端连接模块 SHALL 实现心跳机制，定期向服务器发送心跳包保持连接活性。

#### Scenario: 启动心跳定时器
- **WHEN** 连接建立成功
- **THEN** 启动心跳定时器，每 5 秒发送一次心跳

#### Scenario: 发送心跳包
- **WHEN** 心跳定时器触发
- **THEN** 构造 Heartbeat 消息（包含当前时间戳），发送到服务器

#### Scenario: 收到心跳响应
- **WHEN** 收到服务器的心跳响应
- **THEN** 更新最后心跳响应时间

#### Scenario: 心跳超时检测
- **WHEN** 超过 15 秒未收到心跳响应
- **THEN** 标记连接为断开，触发重连或通知上层

### Requirement: 网络线程
客户端连接模块 SHALL 使用独立线程处理网络 IO，避免阻塞 PyGame 主循环。

#### Scenario: 启动网络线程
- **WHEN** 连接建立成功
- **THEN** 启动独立的网络线程，负责消息收发和心跳

#### Scenario: 线程安全消息队列
- **WHEN** 网络线程接收到消息
- **THEN** 将消息放入线程安全队列，供主循环读取

#### Scenario: 主循环读取消息
- **WHEN** PyGame 主循环每帧执行
- **THEN** 从消息队列中取出所有待处理消息，分发给游戏逻辑

#### Scenario: 主循环发送消息
- **WHEN** 游戏逻辑需要发送消息
- **THEN** 将消息放入发送队列，由网络线程异步发送

#### Scenario: 网络线程退出
- **WHEN** 连接断开或客户端关闭
- **THEN** 网络线程优雅退出，清理资源

### Requirement: Protobuf 消息定义
客户端连接模块 SHALL 使用与服务器一致的 Protobuf 消息格式。

#### Scenario: 心跳消息
- **WHEN** 发送或接收心跳
- **THEN** 使用 Heartbeat 消息，包含 timestamp 字段

#### Scenario: 登录请求消息
- **WHEN** 发起登录请求
- **THEN** 使用 LoginReq 消息，包含 token 字段

#### Scenario: 登录响应消息
- **WHEN** 收到登录响应
- **THEN** 使用 LoginResp 消息，包含 code 和 msg 字段

#### Scenario: 通用消息封装
- **WHEN** 发送或接收游戏逻辑消息
- **THEN** 使用 Packet 消息封装，包含 msg_id 和 payload 字段

### Requirement: 连接状态管理
客户端连接模块 SHALL 管理连接状态，提供状态查询接口。

#### Scenario: 查询连接状态
- **WHEN** 上层逻辑需要了解连接状态
- **THEN** 返回当前状态（连接中、已连接、已断开）

#### Scenario: 断开连接
- **WHEN** 客户端主动关闭或收到断开指令
- **THEN** 关闭 socket，停止心跳，清理网络线程

#### Scenario: 连接异常回调
- **WHEN** 网络异常导致连接断开
- **THEN** 通知上层逻辑，携带断开原因

