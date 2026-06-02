---
name: config-editor
description: 配置编辑器架构、Web UI、JSON Schema 验证、热更新机制。
metadata:
  type: reference
---

# 配置编辑器

## 概述

配置编辑器提供 Web UI 界面管理游戏配置文件，支持 JSON Schema 验证和热更新。

## 架构设计

### 整体架构

```
Browser → Web UI (Vue.js) → REST API (Python) → config/*.json
                                                      ↓
                                                  File Watcher
                                                      ↓
                                                  Server Reload
```

### 功能模块

- 配置浏览：树形展示配置文件
- 在线编辑：表单化编辑配置项
- Schema 验证：编辑时实时校验
- 热更新：保存后自动通知服务重载

## 关键流程

### 编辑流程

1. 用户在 Web UI 选择配置文件
2. 加载配置内容和 Schema
3. 表单化展示可编辑字段
4. 用户修改并保存
5. API 验证 Schema
6. 写入配置文件
7. 通知相关服务重载

### 热更新流程

1. 配置文件被修改
2. File Watcher 检测到变更
3. 通知相关服务
4. 服务重新加载配置

## 关键代码路径

- Web UI：`tools/config-editor/` 目录
- API 服务：`tools/config-editor/server.py`
- 配置 Schema：`config/schema/` 目录

## 常见陷阱

### 配置格式校验遗漏

保存了格式错误的配置：
- 严格使用 JSON Schema 验证
- 保存前显示验证错误

### 并发修改冲突

多人同时编辑同一配置：
- 实现乐观锁（版本号检查）
- 冲突时提示用户刷新

### 热更新时机不当

服务正在处理请求时重载配置：
- 实现配置重载的队列机制
- 在空闲时执行重载

## 扩展指南

### 添加新配置文件支持

1. 创建对应的 JSON Schema
2. 在 Web UI 中添加配置文件入口
3. 在 API 中添加读写接口

## 相关 Skill

- [[server-architecture]] — 服务器架构（配置系统）
