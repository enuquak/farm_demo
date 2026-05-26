## ADDED Requirements

### Requirement: DBMgr 连接集成
Game Server SHALL 在启动时连接所有配置的 DBMgr 实例，并在运行时维护连接状态。

#### Scenario: 启动时初始化 DBMgr 连接
- **WHEN** Game Server 启动
- **THEN** 读取配置中的 dbmgr_list，为每个 DBMgr 创建 DBMgrConnection，发起连接

#### Scenario: DBMgr 连接断开处理
- **WHEN** 某个 DBMgr 连接断开
- **THEN** 标记该 DBMgr 为不可用，清理相关 pending requests，持续尝试重连

### Requirement: 玩家数据加载
Game Server SHALL 在玩家加入时从 DBMgr 加载玩家数据。

#### Scenario: 玩家加入时加载数据
- **WHEN** 收到 PLAYER_JOIN 消息，player_id=N
- **THEN** 向 DBMgr 发送 GET_ALL 请求（player_id=N），等待响应后创建 Player 对象并填充数据

#### Scenario: 新玩家首次加入
- **WHEN** DBMgr 返回的 player data 为空（文件不存在）
- **THEN** 构造初始玩家数据（默认等级、空背包等），通过 SET_ALL 写入 DBMgr，然后创建 Player 对象

#### Scenario: DBMgr 不可用时玩家加入
- **WHEN** 目标 DBMgr 连接状态为 DISCONNECTED
- **THEN** 回复 PLAYER_JOIN_RESP code=失败，记录错误日志

### Requirement: 玩家数据保存
Game Server SHALL 在玩家离开时将数据保存到 DBMgr。

#### Scenario: 玩家离开时保存数据
- **WHEN** 收到 PLAYER_LEAVE 消息或 Gate 连接断开导致玩家清理
- **THEN** 将 Player 对象中的数据通过 SET_ALL 写入 DBMgr

#### Scenario: 保存时 DBMgr 不可用
- **WHEN** 目标 DBMgr 连接状态为 DISCONNECTED
- **THEN** 记录错误日志，数据可能丢失（下次登录使用旧数据）

### Requirement: Player 数据层
Game Server SHALL 在 Player 对象中维护玩家业务数据，提供数据读写接口。

#### Scenario: Player 对象持有数据
- **WHEN** Player 对象创建并加载数据后
- **THEN** Player 对象内存储玩家业务数据（如 level、gold、inventory 等），业务逻辑可直接读写

#### Scenario: 获取 Player 数据
- **WHEN** 业务代码调用 player->get_data(key)
- **THEN** 返回对应字段的数据

#### Scenario: 设置 Player 数据
- **WHEN** 业务代码调用 player->set_data(key, value)
- **THEN** 更新 Player 对象中的数据，标记为脏（后续可按需保存）
