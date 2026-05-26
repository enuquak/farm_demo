# Tasks

## 1. 消息协议扩展

### 1.1 修改 base.proto
- [ ] 删除 Packet 消息定义
- [ ] 新增 AccountMsg 消息定义
- [ ] 新增 PlayerMsg 消息定义

### 1.2 新增 account.proto
- [ ] 定义 QueryRolesReq 消息
- [ ] 定义 RoleInfo 消息
- [ ] 定义 QueryRolesResp 消息
- [ ] 定义 CreateRoleReq 消息
- [ ] 定义 CreateRoleResp 消息

### 1.3 新增 player.proto
- [ ] 定义 EnterGameReq 消息
- [ ] 定义 PlayerData 消息
- [ ] 定义 EnterGameResp 消息

### 1.4 修改 internal.proto
- [ ] 新增 DBMgrIdentify/Resp 消息
- [ ] 新增 DBMgrHeartbeat/Resp 消息
- [ ] 新增 AccountDataReq/Resp 消息
- [ ] 新增 AccountSetReq/Resp 消息
- [ ] 新增 AccountRoleInfo 消息

### 1.5 更新 internal_msg_ids.h
- [ ] 新增账号相关消息 ID (1000-1999)
- [ ] 新增玩家相关消息 ID (2000-2999)
- [ ] 新增 DBMgr 账号数据消息 ID (4100-4199)

## 2. DBMgr 扩展

### 2.1 账号数据存储
- [ ] 实现 accounts 目录管理
- [ ] 实现账号数据文件读写
- [ ] 实现 hash(account_id) 路由逻辑

### 2.2 账号数据消息处理
- [ ] 处理 AccountDataReq 消息
- [ ] 处理 AccountSetReq 消息
- [ ] 实现角色重复检查逻辑

### 2.3 测试
- [ ] 测试账号数据读写
- [ ] 测试角色添加和查询
- [ ] 测试并发写入场景

## 3. Game Server 扩展

### 3.1 Player 实体扩展
- [ ] 扩展 Player 类，添加业务数据字段
- [ ] 实现 PlayerData 结构体
- [ ] 实现 get_data/set_data 接口

### 3.2 账号处理逻辑
- [ ] 实现 QueryRolesReq 处理
- [ ] 实现 CreateRoleReq 处理
- [ ] 实现 player_id 生成逻辑

### 3.3 登录流程
- [ ] 实现 EnterGameReq 处理
- [ ] 实现玩家数据加载逻辑
- [ ] 实现新玩家初始化逻辑

### 3.4 DBMgr 集成
- [ ] 实现账号数据路由（hash(account_id)）
- [ ] 实现角色数据路由（player_id % count）
- [ ] 实现异步请求队列

### 3.5 测试
- [ ] 测试角色查询
- [ ] 测试角色创建
- [ ] 测试登录进入游戏
- [ ] 测试数据持久化

## 4. Gate Server 扩展

### 4.1 消息路由
- [ ] 实现 AccountMsg 路由（转发给任意 Game）
- [ ] 实现 PlayerMsg 路由（根据 server_id）
- [ ] 实现 server_id 记录到 session

### 4.2 配置管理
- [ ] 实现 game_servers 配置文件解析
- [ ] 实现 server_id 到 (ip, port) 映射
- [ ] 实现配置文件加载逻辑

### 4.3 测试
- [ ] 测试消息路由
- [ ] 测试配置加载
- [ ] 测试错误处理

## 5. 集成测试

### 5.1 完整登录流程
- [ ] 测试查询角色列表（无角色）
- [ ] 测试创建新角色
- [ ] 测试查询角色列表（有角色）
- [ ] 测试登录进入游戏

### 5.2 多服务器场景
- [ ] 测试跨服务器角色创建
- [ ] 测试不同服务器登录

### 5.3 异常场景
- [ ] 测试 DBMgr 不可用
- [ ] 测试角色已存在
- [ ] 测试服务器不存在

## 6. 文档和配置

### 6.1 配置文件
- [ ] 创建 Gate 配置文件模板
- [ ] 创建 DBMgr 数据目录结构

### 6.2 文档
- [ ] 更新 API 文档
- [ ] 更新部署文档
