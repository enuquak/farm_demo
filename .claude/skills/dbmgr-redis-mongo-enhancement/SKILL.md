---
name: dbmgr-redis-mongo-enhancement
description: DBMgr 数据层增强、MongoDB 集成、Redis 缓存层、ConnectionManager 状态机。
metadata:
  type: reference
---

# DBMgr 数据层增强

## 概述

在现有 DBMgr JSON 文件存储基础上，引入 MongoDB 作为持久化存储、Redis 作为缓存层。本 skill 覆盖 Redis/Mongo 的详细设计，与 [[dbmgr-data-layer]] 互补。

## 架构设计

### 整体架构

```
Game Server
    │ TCP + Protobuf
    ↓
DBMgr
    ├── Redis（缓存层）
    │   └── 读穿透缓存、写穿透失效
    │
    └── MongoDB（持久化层）
        └── 集合：players, accounts
```

### ConnectionManager 状态机

```
DISCONNECTED → CONNECTING → CONNECTED
                          → FAILED → FAILED_PERMANENT
```

- FAILED 状态：后台线程自动重连（间隔 5 秒）
- FAILED_PERMANENT：重连次数超过阈值，停止重试
- ready 回调：连接就绪时通知上层

### 数据路由

```cpp
// 玩家数据路由
int target_dbmgr = player_id % dbmgr_count;

// 账户数据路由
int target_dbmgr = std::hash<std::string>{}(account_id) % dbmgr_count;
```

## 关键流程

### 读穿透流程

1. Game 请求玩家数据
2. DBMgr 先查 Redis 缓存
3. 缓存命中：直接返回
4. 缓存未命中：查 MongoDB → 写入 Redis（TTL 5 分钟）→ 返回

### 写穿透流程

1. Game 请求写入玩家数据
2. DBMgr 写入 MongoDB
3. 失效 Redis 缓存（DEL key）
4. 返回写入结果

## 关键代码路径

- MongoDB 连接：`scripts/server/dbmgr/src/mongo_connection.h/cpp`
- Redis 连接：`scripts/server/dbmgr/src/redis_connection.h/cpp`
- ConnectionManager：`scripts/server/dbmgr/src/connection_manager.h/cpp`
- 数据路由：`scripts/server/dbmgr/src/data_router.h/cpp`

## 常见陷阱

### 连接池耗尽

MongoDB/Redis 连接数超过限制：
- 监控连接池使用率
- 合理设置最大连接数
- 连接用完后及时释放

### 缓存一致性

Redis 缓存与 MongoDB 数据不一致：
- 写入时必须失效缓存
- 使用 TTL 作为兜底策略
- 避免在缓存中存储未持久化的数据

### 索引缺失

MongoDB 查询性能差：
- 为常用查询字段创建索引
- player_id 和 account_id 必须有索引

## 扩展指南

### 添加新的数据集合

1. 在 MongoDB 中创建新集合
2. 在 DataManager 中添加对应的 CRUD 方法
3. 定义 Protobuf 消息
4. 在 Game Server 中添加使用逻辑

## 相关 Skill

- [[dbmgr-data-layer]] — DBMgr 基础架构（JSON 文件存储）
- [[player-persistence]] — 玩家数据持久化机制
- [[server-architecture]] — 服务器架构与连接管理
