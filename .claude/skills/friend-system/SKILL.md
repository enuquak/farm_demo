---
name: friend-system
description: 好友系统架构、好友关系模型、在线状态同步、好友请求流程。
metadata:
  type: reference
---

# 好友系统

## 概述

好友系统支持双向好友确认、好友列表管理、黑名单和在线状态同步。采用独立 Friend Server 或 Game Server 内置 Stub 模式实现。

## 架构设计

### 好友关系模型

```
Player A ←→ Player B（双向好友）
Player A → Player C（单向黑名单）
```

- 好友列表上限：50 人
- 黑名单上限：20 人
- 好友关系需要双方确认

### 在线状态同步

使用 Redis pub/sub 跨进程广播在线状态：
- 玩家上线：PUBLISH player:online {player_id}
- 玩家下线：PUBLISH player:offline {player_id}
- Friend Server 订阅并更新好友列表的在线状态

## 关键流程

### 好友请求流程

1. 玩家 A 发送 FriendReq（target_player_id）
2. 服务端验证目标玩家存在且不在黑名单
3. 创建待确认请求，通知目标玩家
4. 目标玩家接受：双向添加好友关系
5. 目标玩家拒绝：删除请求

### 好友数据持久化

好友数据存储在 DBMgr 的 player_data 中：
- friend_list: [player_id, ...]
- black_list: [player_id, ...]

## 关键代码路径

- Friend Server：`scripts/server/friend_server/src/`
- FriendManager：`scripts/server/friend_server/src/friend_manager.h/cpp`
- Protobuf：`scripts/common/proto/friend.proto`
- 消息 ID：`shared/message_ids.json`（friend 相关条目）

## 常见陷阱

### 并发请求处理

同时收到多个好友请求：
- 使用请求队列串行处理
- 验证当前状态再执行操作

### 在线状态延迟

好友上线后状态未及时更新：
- Redis pub/sub 有网络延迟
- 客户端定期拉取作为兜底

### 好友列表上限溢出

好友数超过 50 人限制：
- 添加前检查列表长度
- 返回 FRIEND_LIST_FULL 错误码

## 扩展指南

### 添加好友功能（如最近组队）

1. 在 player_data 中添加新字段
2. 在 FriendManager 中添加对应的查询/更新方法
3. 定义新的 Protobuf 消息
4. 更新客户端 UI

## 相关 Skill

- [[server-architecture]] — 服务器架构
- [[player-persistence]] — 玩家数据持久化
- [[team-system]] — 组队系统
