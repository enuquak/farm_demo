## ADDED Requirements

### Requirement: DBMgr 连接管理
Game SHALL 主动连接所有配置的 DBMgr 实例，维护全连接拓扑。

#### Scenario: 启动时连接所有 DBMgr
- **WHEN** Game Server 启动
- **THEN** 读取配置中的 dbmgr_list，为每个 DBMgr 创建 DBMgrConnection 对象，发起 TCP 连接

#### Scenario: 连接成功后身份标识
- **WHEN** 与某个 DBMgr 的 TCP 连接建立成功
- **THEN** 等待接收 DBMgrIdentify 消息，记录该连接的 index，标记状态为 IDENTIFIED

#### Scenario: 收到身份标识
- **WHEN** 收到 DBMgrIdentify 消息（MsgID 4001）
- **THEN** 解析 index 和 address，回复 DBMgrIdentify_RESP（MsgID 4002），标记连接为 IDENTIFIED

#### Scenario: 连接断开
- **WHEN** 与某个 DBMgr 的连接断开（主动或异常）
- **THEN** 标记该连接为 DISCONNECTED，清理所有发往该 DBMgr 的 pending requests（调用 callback 返回失败），启动重连定时器

#### Scenario: 自动重连
- **WHEN** 与某个 DBMgr 的连接断开，且重连定时器触发
- **THEN** 尝试重新建立 TCP 连接，成功后重新走身份标识流程

### Requirement: 心跳检测
Game SHALL 响应 DBMgr 的心跳请求，并检测心跳超时。

#### Scenario: 收到心跳
- **WHEN** 收到 DBMgrHeartbeat 消息（MsgID 4003）
- **THEN** 回复 DBMgrHeartbeat_RESP（MsgID 4004），更新最后心跳时间

#### Scenario: 心跳超时
- **WHEN** 超过 15 秒未收到某 DBMgr 的心跳
- **THEN** 标记该 DBMgr 连接为超时，执行断开清理

### Requirement: 请求路由
Game SHALL 根据 player_id 将数据请求路由到正确的 DBMgr。

#### Scenario: 计算目标 DBMgr
- **WHEN** 需要发送 PlayerDataReq，player_id=N
- **THEN** 计算 dbmgr_index = N % dbmgr_count，找到对应 DBMgrConnection

#### Scenario: 目标 DBMgr 已连接
- **WHEN** 目标 DBMgr 连接状态为 IDENTIFIED
- **THEN** 通过该连接发送 PlayerDataReq

#### Scenario: 目标 DBMgr 未连接
- **WHEN** 目标 DBMgr 连接状态为 DISCONNECTED
- **THEN** 立即返回失败，调用 callback 传入错误码

### Requirement: 异步请求队列
Game SHALL 维护异步请求队列，通过 request_id 匹配请求和响应。

#### Scenario: 发送请求并注册回调
- **WHEN** 发送 PlayerDataReq
- **THEN** 生成唯一 request_id，将 (request_id → callback) 存入 pending_requests 映射表

#### Scenario: 收到响应匹配回调
- **WHEN** 收到 PlayerDataResp，request_id=X
- **THEN** 在 pending_requests 中查找 X，调用对应的 callback，移除该条目

#### Scenario: request_id 生成
- **WHEN** 需要生成新的 request_id
- **THEN** 使用自增计数器，64 位整型，进程生命周期内唯一

#### Scenario: DBMgr 断开时清理 pending requests
- **WHEN** 某 DBMgr 连接断开
- **THEN** 遍历 pending_requests，找到所有发往该 DBMgr 的请求，调用 callback 传入失败码，移除条目

### Requirement: 连接状态查询
Game SHALL 提供 DBMgr 连接状态查询接口。

#### Scenario: 查询单个 DBMgr 状态
- **WHEN** 调用 is_connected(dbmgr_index)
- **THEN** 返回该 DBMgr 的连接状态（IDENTIFIED / DISCONNECTED）

#### Scenario: 查询所有 DBMgr 状态
- **WHEN** 调用 get_all_status()
- **THEN** 返回所有 DBMgr 的连接状态列表
