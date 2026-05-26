## Purpose

账号系统：支持账号数据的存储与读取、角色列表查询、新角色添加，以及基于 account_id 的数据路由。

## Requirements

### Requirement: 账号数据存储
DBMgr SHALL 支持账号数据的存储和读取，每账号一个 JSON 文件。

#### Scenario: 账号数据文件结构
- **WHEN** 账号数据被存储
- **THEN** 数据文件路径为 `data/accounts/{hash(account_id) % dbmgr_count}.json`，包含 account_id 和 roles 列表

#### Scenario: 账号数据内容
- **WHEN** 账号数据被读取
- **THEN** 返回的数据包含 account_id 和 roles 数组，每个 role 包含 server_id、player_id、role_name

### Requirement: 查询账号角色列表
Game Server SHALL 支持查询指定账号下的所有角色。

#### Scenario: 查询角色列表
- **WHEN** 收到 AccountDataReq，account_id="user_abc"
- **THEN** DBMgr 读取对应的账号文件，返回 AccountDataResp，包含 roles 列表

#### Scenario: 账号不存在
- **WHEN** 收到 AccountDataReq，但账号文件不存在
- **THEN** 返回 AccountDataResp，code=0，roles 为空列表（表示新账号，非错误）

### Requirement: 添加账号角色
Game Server SHALL 支持向账号添加新角色。

#### Scenario: 添加新角色
- **WHEN** 收到 AccountSetReq，account_id="user_abc"，new_role={server_id=1, player_id=1000001, role_name="farmer_1"}
- **THEN** DBMgr 读取账号文件，添加新角色到 roles 列表，写回文件，返回 AccountSetResp code=0

#### Scenario: 账号文件不存在
- **WHEN** 收到 AccountSetReq，但账号文件不存在
- **THEN** 创建新账号文件，添加新角色，返回 AccountSetResp code=0

#### Scenario: 角色已存在
- **WHEN** 收到 AccountSetReq，但该 server_id 下已有角色
- **THEN** 返回 AccountSetResp code=1，msg="角色已存在"

### Requirement: 账号数据路由
DBMgr SHALL 根据 account_id 的 hash 值路由账号数据请求。

#### Scenario: 计算目标 DBMgr
- **WHEN** 需要发送 AccountDataReq，account_id="user_abc"
- **THEN** 计算 dbmgr_index = hash("user_abc") % dbmgr_count，找到对应 DBMgrConnection

#### Scenario: 目标 DBMgr 已连接
- **WHEN** 目标 DBMgr 连接状态为 IDENTIFIED
- **THEN** 通过该连接发送 AccountDataReq

#### Scenario: 目标 DBMgr 未连接
- **WHEN** 目标 DBMgr 连接状态为 DISCONNECTED
- **THEN** 立即返回失败，调用 callback 传入错误码
