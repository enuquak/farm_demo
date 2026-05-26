## ADDED Requirements

### Requirement: Player 数据层
Game Server SHALL 在 Player 对象中维护玩家业务数据，提供数据读写接口。

#### Scenario: Player 对象持有数据
- **WHEN** Player 对象创建并加载数据后
- **THEN** Player 对象内存储玩家业务数据（player_id、role_name、level、position、scene_id）

#### Scenario: 获取 Player 数据
- **WHEN** 业务代码调用 player->get_data()
- **THEN** 返回 PlayerData 结构体，包含所有业务数据字段

#### Scenario: 设置 Player 数据
- **WHEN** 业务代码调用 player->set_data(data)
- **THEN** 更新 Player 对象中的数据，标记为脏（后续可按需保存）

### Requirement: Player 数据字段
Player 对象 SHALL 包含以下业务数据字段。

#### Scenario: 基础数据字段
- **WHEN** Player 对象被创建
- **THEN** 包含以下字段：
  - player_id: uint64（唯一标识）
  - role_name: string（角色名）
  - level: uint32（等级）
  - position: {x: float, y: float, z: float}（坐标）
  - scene_id: string（场景ID）

### Requirement: 玩家数据加载
Game Server SHALL 在玩家进入游戏时从 DBMgr 加载玩家数据。

#### Scenario: 玩家进入游戏时加载数据
- **WHEN** 收到 EnterGameReq，player_id=1000001
- **THEN** 向 DBMgr 发送 PlayerDataReq（player_id=1000001），等待响应后创建 Player 对象并填充数据

#### Scenario: 新玩家首次进入
- **WHEN** DBMgr 返回的 player data 为空（文件不存在）
- **THEN** 构造初始玩家数据（默认等级1、默认坐标、默认场景），通过 PlayerSetReq 写入 DBMgr，然后创建 Player 对象

#### Scenario: DBMgr 不可用时玩家进入
- **WHEN** 目标 DBMgr 连接状态为 DISCONNECTED
- **THEN** 回复 EnterGameResp code=失败，记录错误日志

### Requirement: 玩家数据保存
Game Server SHALL 在玩家离开时将数据保存到 DBMgr。

#### Scenario: 玩家离开时保存数据
- **WHEN** 收到 PlayerLeave 消息或 Gate 连接断开导致玩家清理
- **THEN** 将 Player 对象中的数据通过 PlayerSetReq 写入 DBMgr

#### Scenario: 保存时 DBMgr 不可用
- **WHEN** 目标 DBMgr 连接状态为 DISCONNECTED
- **THEN** 记录错误日志，数据可能丢失（下次登录使用旧数据）

### Requirement: 角色数据路由
DBMgr SHALL 根据 player_id 路由角色数据请求。

#### Scenario: 计算目标 DBMgr
- **WHEN** 需要发送 PlayerDataReq，player_id=1000001
- **THEN** 计算 dbmgr_index = 1000001 % dbmgr_count，找到对应 DBMgrConnection

#### Scenario: 目标 DBMgr 已连接
- **WHEN** 目标 DBMgr 连接状态为 IDENTIFIED
- **THEN** 通过该连接发送 PlayerDataReq

#### Scenario: 目标 DBMgr 未连接
- **WHEN** 目标 DBMgr 连接状态为 DISCONNECTED
- **THEN** 立即返回失败，调用 callback 传入错误码
