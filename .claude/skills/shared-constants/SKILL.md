---
name: shared-constants
description: 共享常量系统架构、消息 ID 管理、错误码管理、代码自动生成。
metadata:
  type: reference
---

# 共享常量系统

## 概述

项目使用共享常量系统确保 C++ 服务器和 Python 客户端使用相同的消息 ID 和错误码。常量定义在 `shared/` 目录下的 JSON 文件中，通过代码生成脚本自动同步到两端。

## 架构设计

### 数据流

```
shared/*.json → file_watcher.py → generate_constants.py → scripts/client/*.py
                                                           scripts/server/common/include/*.h
```

### 消息 ID 分区规则

| 范围 | 用途 |
|------|------|
| 1-999 | 客户端基础消息（心跳、登录） |
| 1001-1999 | 角色相关（查询、创建、进入游戏） |
| 2001-2999 | 场景/位置/时钟 |
| 3001-3099 | Gate-Game 连接管理 |
| 3100-3199 | 玩家生命周期 |
| 3200-3299 | 消息转发 |
| 3300-3399 | 账号消息转发 |
| 4000-4099 | Game-DBMgr 连接管理 |
| 4100-4199 | 数据操作 |
| 4200-4299 | 账号数据操作 |
| 5000-5999 | 管理消息（停服） |
| 6000-6999 | 聊天系统 |

## 关键流程

### 添加新消息 ID 的完整步骤

1. 在 `shared/message_ids.json` 中添加新条目：
   ```json
   { "code": 6001, "name": "MSG_ID_CHAT_MSG_REQ", "description": "聊天消息请求" }
   ```
2. 运行代码生成：
   ```bash
   python scripts/tools/generate_constants.py
   ```
3. 验证生成文件：
   - C++: `scripts/server/common/include/message_ids.h`
   - Python: `scripts/client/message_ids.py`
4. 在 `scripts/common/proto/` 中定义对应的 Protobuf 消息
5. 在服务器中注册消息 handler

### 添加新错误码的步骤

1. 在 `shared/error_codes.json` 中添加新条目
2. 运行代码生成
3. 在客户端和服务器中使用生成的常量

## 关键代码路径

- 共享常量 JSON：`shared/message_ids.json`, `shared/error_codes.json`
- 生成脚本：`scripts/tools/generate_constants.py`
- 文件监控：`scripts/tools/file_watcher.py`
- 生成的 C++ 头文件：`scripts/server/common/include/message_ids.h`, `error_codes.h`
- 生成的 Python 模块：`scripts/client/message_ids.py`, `scripts/client/error_codes.py`

## 常见陷阱

### 常量不同步

客户端和服务器使用不同的消息 ID，导致消息无响应：
- 修改 `shared/` 下的 JSON 后必须运行 `generate_constants.py`
- 使用 `tools/build_all.bat` 构建时会自动运行生成脚本

### ID 冲突

新添加的消息 ID 与已有 ID 冲突：
- 添加前检查 `shared/message_ids.json` 中是否已存在该 ID
- 遵守分区规则，不同功能使用不同范围

### 生成脚本未运行

修改 JSON 后忘记运行生成脚本：
- 启动文件监控 `python scripts/tools/file_watcher.py` 自动触发生成
- 或在构建脚本中确认包含生成步骤

## 扩展指南

### 添加新的共享常量类型

1. 在 `shared/` 目录下创建新的 JSON 文件（如 `item_ids.json`）
2. 在 `generate_constants.py` 中添加新文件的处理逻辑
3. 运行生成脚本验证输出

## 相关 Skill

- [[server-architecture]] — 服务器架构与消息协议
- [[client-architecture]] — 客户端架构与消息处理
