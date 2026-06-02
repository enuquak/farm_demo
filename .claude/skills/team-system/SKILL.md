---
name: team-system
description: 组队系统架构、Team Manager、组队流程、队伍状态管理。
metadata:
  type: reference
---

# 组队系统

## 概述

组队系统支持玩家创建队伍、邀请成员、管理队伍状态。Team Manager 管理所有队伍，Team 实体持有成员列表和队伍属性。

## 架构设计

### 组件结构

```
TeamManager
├── Team 1
│   ├── Leader (Player A)
│   ├── Member (Player B)
│   └── Member (Player C)
└── Team 2
    └── ...
```

### 队伍属性

- 队伍上限：5 人
- 队长转移：队长离开时自动转移给最早加入的成员
- 队伍解散：所有成员离开后自动解散

## 关键流程

### 创建队伍

1. 玩家发送 TeamCreateReq
2. TeamManager 创建 Team 实体
3. 创建者成为队长
4. 返回 TeamCreateResp（team_id）

### 邀请成员

1. 队长发送 TeamInviteReq（target_player_id）
2. 验证队伍未满、目标玩家不在其他队伍
3. 发送邀请通知给目标玩家
4. 目标玩家接受：加入队伍
5. 目标玩家拒绝：删除邀请

### 离开队伍

1. 成员发送 TeamLeaveReq
2. 如果是队长：转移队长权限
3. 如果是最后成员：解散队伍
4. 广播队伍状态更新

## 关键代码路径

- Team Manager：`scripts/server/game_server/src/team_manager.h/cpp`
- Team 实体：`scripts/server/game_server/src/team.h`
- Protobuf：`scripts/common/proto/team.proto`

## 常见陷阱

### 队长离线处理

队长掉线但队伍未解散：
- 实现队长超时转移机制
- 或允许其他成员申请队长

### 成员状态同步

成员状态变更未及时通知其他成员：
- 所有队伍操作都广播状态更新
- 使用统一的 TeamSync 消息

### 队伍数据持久化

服务器重启后队伍数据丢失：
- 当前设计队伍数据仅在内存中
- 服务器重启后队伍自动解散
- 如需持久化，需添加 DBMgr 存储

## 扩展指南

### 集成副本匹配

1. 在 Team 中添加副本类型字段
2. TeamManager 提供按副本类型查询队伍的接口
3. 副本系统调用 TeamManager 获取队伍信息

## 相关 Skill

- [[server-architecture]] — 服务器架构
- [[friend-system]] — 好友系统（组队邀请来源）
- [[monster-combat]] — 战斗系统（组队战斗）
