## ADDED Requirements

### Requirement: 查询角色列表
Gate Server SHALL 支持客户端查询账号下的角色列表。

#### Scenario: 客户端查询角色列表
- **WHEN** 客户端发送 AccountMsg(QueryRolesReq)，account_id="user_abc"
- **THEN** Gate 转发给任意一个 Game Server，Game 查询 DBMgr 并返回角色列表

#### Scenario: 有角色
- **WHEN** 账号下有角色
- **THEN** 返回 QueryRolesResp，code=0，roles 包含角色列表（server_id、player_id、role_name）

#### Scenario: 无角色
- **WHEN** 账号下无角色
- **THEN** 返回 QueryRolesResp，code=0，roles 为空列表，客户端应展示服务器列表

### Requirement: 创建新角色
Gate Server SHALL 支持客户端在指定服务器创建新角色。

#### Scenario: 客户端创建角色
- **WHEN** 客户端发送 AccountMsg(CreateRoleReq)，account_id="user_abc"，server_id=1，role_name="farmer_1"
- **THEN** Gate 根据 server_id 路由到对应 Game Server，Game 生成 player_id 并创建角色

#### Scenario: 创建成功
- **WHEN** 角色创建成功
- **THEN** 返回 CreateRoleResp，code=0，player_id=1000001

#### Scenario: 角色已存在
- **WHEN** 该服务器下已有角色
- **THEN** 返回 CreateRoleResp，code=1，msg="角色已存在"

#### Scenario: 服务器不存在
- **WHEN** server_id 无效
- **THEN** 返回 CreateRoleResp，code=2，msg="服务器不存在"

### Requirement: 登录进入游戏
Gate Server SHALL 支持客户端选择角色进入游戏。

#### Scenario: 客户端进入游戏
- **WHEN** 客户端发送 PlayerMsg(EnterGameReq)，player_id=1000001，server_id=1
- **THEN** Gate 根据 server_id 路由到对应 Game Server，Game 加载角色数据并返回

#### Scenario: 进入成功
- **WHEN** 角色数据加载成功
- **THEN** 返回 EnterGameResp，code=0，player_data 包含完整角色数据

#### Scenario: 角色不存在
- **WHEN** player_id 无效或角色数据不存在
- **THEN** 返回 EnterGameResp，code=1，msg="角色不存在"

#### Scenario: 服务器不可用
- **WHEN** 目标 Game Server 未连接
- **THEN** 返回 EnterGameResp，code=2，msg="服务器不可用"

### Requirement: Gate 消息路由
Gate Server SHALL 根据消息类型和 server_id 路由消息。

#### Scenario: 账号消息路由
- **WHEN** 收到 AccountMsg
- **THEN** 转发给任意一个 Game Server（轮询或随机选择）

#### Scenario: 玩家消息路由
- **WHEN** 收到 PlayerMsg，且 session 中记录了 server_id
- **THEN** 根据 server_id 路由到对应的 Game Server

#### Scenario: 未知消息
- **WHEN** 收到未知类型的消息
- **THEN** 记录警告日志，丢弃消息

### Requirement: Gate 配置管理
Gate Server SHALL 维护 server_id 到 Game Server 的配置映射。

#### Scenario: 配置文件结构
- **WHEN** Gate Server 启动
- **THEN** 读取配置文件，解析 game_servers 列表，建立 server_id 到 (ip, port) 的映射

#### Scenario: 配置文件内容
- **WHEN** 配置文件被读取
- **THEN** 包含以下内容：
  ```json
  {
    "game_servers": [
      { "server_id": 1, "ip": "127.0.0.1", "port": 9091 },
      { "server_id": 2, "ip": "127.0.0.1", "port": 9092 },
      { "server_id": 3, "ip": "127.0.0.1", "port": 9093 }
    ]
  }
  ```

#### Scenario: 服务器不存在
- **WHEN** 收到的 server_id 在配置中不存在
- **THEN** 返回错误响应，记录警告日志
