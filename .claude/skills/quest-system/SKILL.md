---
name: quest-system
description: 任务系统架构、任务模型、任务类型、任务目标、任务奖励。
metadata:
  type: reference
---

# 任务系统

## 概述

任务系统支持主线任务、支线任务和日常任务。任务有明确的状态机和目标系统，完成后给予奖励。

## 架构设计

### 任务状态机

```
AVAILABLE → ACTIVE → COMPLETED → SUBMITTED
    ↑          ↓
    └──────────┘（放弃任务）
```

### 任务类型

| 类型 | 说明 | 重置 |
|------|------|------|
| 主线 | 推动剧情发展 | 不可重置 |
| 支线 | 丰富游戏内容 | 不可重置 |
| 日常 | 每日重复 | 每日重置 |

### 任务目标类型

- 收集物品：收集指定数量的物品
- 击杀怪物：击杀指定数量的怪物
- NPC 对话：与指定 NPC 对话
- 探索区域：到达指定区域

## 关键流程

### 任务接取

1. 玩家与 NPC 对话或到达触发区域
2. 检查任务前置条件（等级、前置任务）
3. 创建任务实例，状态设为 ACTIVE
4. 开始追踪任务目标进度

### 任务完成

1. 所有任务目标达成
2. 任务状态变为 COMPLETED
3. 通知客户端显示完成提示
4. 玩家提交任务（与 NPC 对话）
5. 发放奖励，状态变为 SUBMITTED

## 关键代码路径

- Quest Manager：`scripts/server/game_server/src/quest_manager.h/cpp`
- Quest 定义：`config/quests.json`
- 客户端 UI：`scripts/client/ui/quest_panel.py`
- Protobuf：`scripts/common/proto/quest.proto`

## 常见陷阱

### 任务状态持久化

任务进度未保存导致丢失：
- 任务数据存储在 PlayerBizData 中
- 脏标记触发保存

### 目标计数重置

任务目标进度异常重置：
- 避免在 update 中重复初始化进度
- 加载时从持久化数据恢复

### 任务链顺序错误

前置任务未完成就能接取后续任务：
- 接取时严格检查前置任务状态

## 扩展指南

### 添加新任务

1. 在 `config/quests.json` 中定义任务配置
2. 定义任务目标和奖励
3. 如需新目标类型，在 QuestManager 中添加处理逻辑

## 相关 Skill

- [[npc-dialog]] — NPC 对话系统（任务触发）
- [[item-interaction]] — 物品系统（任务奖励）
- [[monster-combat]] — 战斗系统（击杀目标）
