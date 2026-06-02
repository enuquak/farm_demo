---
name: client-notification
description: 客户端通知系统架构、NotificationManager、通知类型、优先级。
metadata:
  type: reference
---

# 客户端通知系统

## 概述

通知系统统一管理客户端的各种提示信息，支持 Toast 轻提示、Banner 横幅和 Modal 模态框三种类型。

## 架构设计

### 通知类型

| 类型 | 说明 | 自动消失 | 输入阻塞 |
|------|------|----------|----------|
| Toast | 轻量提示 | 是（3秒） | 否 |
| Banner | 横幅通知 | 是（5秒） | 否 |
| Modal | 模态框 | 否（手动关闭） | 是 |

### 优先级

```
紧急（Modal）> 重要（Banner）> 普通（Toast）
```

## 关键流程

### 显示通知

1. 代码调用 NotificationManager.show(type, message)
2. 加入通知队列
3. 按优先级排序
4. 渲染最高优先级通知
5. 自动消失或手动关闭后显示下一个

### 事件处理

1. PyGame 事件循环先传递给 NotificationManager
2. 如果 Modal 可见，消费事件（阻止游戏输入）
3. Toast/Banner 不消费事件

## 关键代码路径

- NotificationManager：`scripts/client/ui/notification_manager.py`
- Toast 组件：`scripts/client/ui/toast.py`
- Banner 组件：`scripts/client/ui/banner.py`

## 常见陷阱

### 通知堆积

大量通知同时到达导致 UI 混乱：
- 限制同时显示的通知数量
- 使用队列排队显示

### 输入冲突

Modal 显示时游戏输入未屏蔽：
- 在事件处理中检查 Modal 状态
- Modal 可见时阻止游戏输入

### 多通知重叠

多个通知同时显示位置重叠：
- 计算通知位置时考虑已有通知
- 使用堆叠布局

## 扩展指南

### 添加新通知类型

1. 定义新的通知类型枚举
2. 创建对应的 UI 组件
3. 在 NotificationManager 中添加处理逻辑

## 相关 Skill

- [[client-architecture]] — 客户端架构与 UI 系统
