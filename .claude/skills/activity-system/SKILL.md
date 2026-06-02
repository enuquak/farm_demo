---
name: activity-system
description: 活动系统架构、活动模型、活动类型、活动奖励。
metadata:
  type: reference
---

# 活动系统

## 概述

活动系统支持限时活动、周期活动和节日活动，有明确的时间窗口和状态管理。

## 架构设计

### 活动状态机

```
PENDING → ACTIVE → ENDED
```

### 活动类型

| 类型 | 说明 | 示例 |
|------|------|------|
| 限时 | 固定时间窗口 | 周末双倍经验 |
| 周期 | 重复出现 | 每日签到 |
| 节日 | 特定日期 | 春节活动 |

## 关键流程

### 活动启动

1. 定时器检查活动配置
2. 当前时间进入活动窗口
3. 活动状态变为 ACTIVE
4. 广播活动开始通知

### 活动结束

1. 当前时间超过活动窗口
2. 活动状态变为 ENDED
3. 结算排行榜
4. 发放奖励

## 关键代码路径

- Activity Manager：`scripts/server/game_server/src/activity_manager.h/cpp`
- 活动配置：`config/activities.json`
- Protobuf：`scripts/common/proto/activity.proto`

## 常见陷阱

### 时区处理

活动时间与服务器时区不一致：
- 使用 UTC 时间存储和比较
- 客户端显示时转换为本地时间

### 活动状态持久化

服务器重启后活动状态丢失：
- 从配置文件重新加载
- 根据当前时间重新计算状态

### 跨天活动边界

活动跨越午夜时的行为异常：
- 使用时间戳而非日期字符串
- 明确活动的开始和结束时间戳

## 扩展指南

### 添加新活动类型

1. 在 `config/activities.json` 中定义活动配置
2. 在 ActivityManager 中添加活动逻辑
3. 定义活动奖励规则

## 相关 Skill

- [[game-clock]] — 游戏时钟（活动时间基准）
