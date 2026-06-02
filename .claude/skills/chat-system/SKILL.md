---
name: chat-system
description: 聊天系统架构、Chat Server、频道类型、消息路由、客户端 UI。
metadata:
  type: reference
---

# 聊天系统

## 概述

聊天系统采用独立 Chat Server 架构，支持世界频道、场景频道、私聊频道和系统频道。GateServer 将聊天消息（msg_id 6000-6999）转发到 ChatServer 处理。

## 架构设计

### 整体架构

```
Client → Gate Server → Chat Server → Redis（历史消息）
                         ↓
                    Channel Manager
                    ├── 世界频道
                    ├── 场景频道
                    ├── 私聊频道
                    └── 系统频道
```

### 频道类型

| 类型 | 说明 | 订阅方式 |
|------|------|----------|
| 世界频道 | 全服广播 | 自动订阅 |
| 场景频道 | 同场景玩家 | 进入场景时订阅 |
| 私聊频道 | 点对点 | 发起私聊时创建 |
| 系统频道 | 系统公告 | 自动订阅 |

## 关键流程

### 消息发送流程

1. 客户端发送 ChatMsgReq（channel_type、content）
2. GateServer 解析 PlayerMsg，提取 player_id
3. 打包为 ClientMessage 发送给 ChatServer
4. ChatServer 验证、存储、广播到频道
5. 返回 ChatMsgResp 给发送者

### 消息接收流程

1. ChatServer 将消息推送到频道所有订阅者
2. 通过 GameMessage 格式发送给 GateServer
3. GateServer 查找玩家 Session，转发消息
4. 客户端渲染消息到 ChatPanel

## 关键代码路径

- Chat Server：`scripts/server/chat_server/src/`
- GateServer 路由：`scripts/server/gate_server/src/gate_server.cpp`（route_message 6000+ 分支）
- 客户端 UI：`scripts/client/ui/chat_panel.py`
- 频道配置：`scripts/client/chat_channel.py`
- Protobuf：`scripts/common/proto/chat.proto`

## 常见陷阱

### 消息顺序

消息到达顺序与发送顺序不一致：
- 使用时间戳排序
- 服务端分配序列号

### 频道订阅遗漏

玩家切换场景后未订阅新频道：
- 场景切换时自动退订旧频道、订阅新频道

### 客户端线程安全

网络线程和渲染线程同时访问消息队列：
- 使用线程安全队列
- 渲染线程从队列取消息时加锁

## 扩展指南

### 添加新频道类型

1. 在 Channel Manager 中添加新频道类型枚举
2. 实现频道的订阅/退订逻辑
3. 定义频道的消息路由规则
4. 更新客户端 UI 支持新频道

## 相关 Skill

- [[server-architecture]] — 服务器架构与消息路由
- [[client-architecture]] — 客户端架构与 UI 系统
