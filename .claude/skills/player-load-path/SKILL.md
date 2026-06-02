---
name: player-load-path
description: 玩家加载路径修复、load_from_json() 方法、字段兼容策略。
metadata:
  type: reference
---

# 玩家加载路径修复

## 概述

修复现有玩家数据加载路径的缺陷，提供统一的 `load_from_json()` 方法，支持字段兼容和数据迁移。

## 架构设计

### 问题描述

现有加载路径的问题：
- 字段丢失时崩溃
- 格式不一致导致解析失败
- 缺少版本管理

### 解决方案

统一的 `load_from_json()` 方法：
- 缺失字段使用默认值
- 多余字段忽略
- 支持版本号字段实现数据迁移

## 关键流程

### 加载流程

1. 从 DBMgr 获取 JSON 数据
2. 调用 `load_from_json(json)`
3. 解析各字段，缺失则使用默认值
4. 检查版本号，必要时执行迁移
5. 填充 Player 对象

### 数据迁移

1. 检查 JSON 中的 version 字段
2. 如果版本低于当前版本，执行迁移函数
3. 迁移后更新版本号
4. 标记为脏以便保存新格式

## 关键代码路径

- Player 加载：`scripts/server/game_server/src/player.cpp`（load_from_json）
- Player 序列化：`scripts/server/game_server/src/player.cpp`（get_all_data_json）
- 测试：`scripts/server/game_server/tests/`

## 常见陷阱

### JSON 格式兼容性

旧格式数据无法解析：
- 使用 try-catch 包裹解析逻辑
- 解析失败时使用默认数据

### 字段缺失导致崩溃

直接访问不存在的 JSON 字段：
- 使用 `json.value("key", default_value)` 安全访问
- 检查字段存在性再使用

### 默认值不合理

默认值导致游戏逻辑异常：
- 默认值应符合游戏设计（如 level=1, gold=100）
- 测试覆盖默认值场景

## 扩展指南

### 添加新字段

1. 在 PlayerBizData 中添加新字段
2. 在 load_from_json 中添加解析逻辑（带默认值）
3. 在 get_all_data_json 中添加序列化逻辑
4. 更新版本号（如需要）

## 相关 Skill

- [[player-persistence]] — 玩家数据持久化机制
- [[dbmgr-data-layer]] — DBMgr 数据层
