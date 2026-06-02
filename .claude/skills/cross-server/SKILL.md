---
name: cross-server
description: 跨服架构、CrossServerService、GameConnection、RouteCache。
metadata:
  type: reference
---

# 跨服系统

## 概述

跨服系统支持玩家在不同服务器之间交互和迁移。CrossServerService 管理跨服连接，RouteCache 缓存路由信息。

## 架构设计

### 整体架构

```
Game Server A ←→ CrossServerService ←→ Game Server B
                    │
                    ├── GameConnection（连接池）
                    ├── GameSession（会话管理）
                    └── RouteCache（路由缓存）
```

### 组件职责

- CrossServerService：管理所有跨服连接和消息路由
- GameConnection：与远程 Game Server 的连接
- GameSession：跨服玩家会话状态
- RouteCache：玩家 → 目标服务器映射缓存

## 关键流程

### 跨服消息转发

1. 玩家 A 向跨服玩家 B 发送消息
2. 查询 RouteCache 获取 B 所在服务器
3. 通过 GameConnection 转发到目标服务器
4. 目标服务器投递给玩家 B

### 玩家迁移

1. 玩家请求迁移到目标服务器
2. 打包玩家数据
3. 发送到目标服务器
4. 目标服务器创建玩家实例
5. 更新 RouteCache

## 关键代码路径

- CrossServerService：`scripts/server/game_server/src/cross_server_service.h/cpp`
- GameConnection：`scripts/server/game_server/src/game_connection.h/cpp`
- GameSession：`scripts/server/game_server/src/game_session.h/cpp`
- RouteCache：`scripts/server/game_server/src/route_cache.h/cpp`

## 常见陷阱

### 路由缓存失效

玩家迁移后缓存未更新：
- 迁移成功后更新所有相关服务器的 RouteCache
- 缓存设置 TTL 定期刷新

### 会话状态丢失

跨服会话中断后状态丢失：
- 会话数据持久化到 DBMgr
- 重连后恢复会话

### 消息乱序

跨服消息到达顺序与发送顺序不一致：
- 使用消息序列号
- 接收端按序列号排序

## 扩展指南

### 添加新的跨服功能

1. 定义新的跨服消息类型
2. 在 CrossServerService 中添加路由逻辑
3. 在目标服务器中添加处理逻辑

## 相关 Skill

- [[server-architecture]] — 服务器架构
- [[player-persistence]] — 玩家数据持久化（迁移数据）
