# 共享常量自动生成机制设计文档

## 1. 概述

### 1.1 目标

建立一套自动化机制，将客户端和服务器共用的常量（错误码、消息ID、物品ID等）统一存放在 `shared/` 目录下，通过文件监控服务自动检测变化并生成对应语言的代码文件，消除手动同步的维护负担。

### 1.2 背景

当前项目中，消息ID等常量需要在 Python 客户端和 C++ 服务器端分别手动维护：
- `scripts/common/proto/msg_ids.h` (C++)
- `scripts/client/msg_ids.py` (Python)

手动同步容易导致不一致，且随着项目增长，维护成本会持续上升。

### 1.3 设计原则

- **单一数据源**：所有共享常量只在 `shared/` 目录下的 JSON 文件中定义
- **自动生成**：文件变化时自动触发代码生成，无需人工干预
- **类型安全**：生成的代码应提供类型检查和IDE自动补全支持
- **向后兼容**：新机制不应破坏现有代码结构

## 2. 架构设计

### 2.1 整体架构

```
shared/                          # 配置源文件目录
├── error_codes.json             # 错误码配置
├── message_ids.json             # 消息ID配置
└── item_ids.json                # 物品ID配置

scripts/tools/                   # 工具脚本目录
├── generate_constants.py        # 代码生成主脚本（包含内嵌模板）
└── file_watcher.py              # 文件监控服务

scripts/client/                  # 客户端生成代码
├── error_codes.py               # 自动生成
├── message_ids.py               # 自动生成（替换现有）
└── item_ids.py                  # 自动生成

scripts/server/common/include/   # 服务器生成代码
├── error_codes.h                # 自动生成
├── message_ids.h                # 自动生成（替换现有）
└── item_ids.h                   # 自动生成
```

### 2.2 数据流

```
shared/*.json → file_watcher.py → generate_constants.py → scripts/client/*.py
                                                           scripts/server/common/include/*.h
```

### 2.3 组件职责

| 组件 | 职责 |
|------|------|
| shared/*.json | 存储所有共享常量的定义 |
| generate_constants.py | 读取JSON，生成Python和C++代码 |
| file_watcher.py | 监控shared/目录变化，触发生成脚本 |

## 3. 配置文件格式

### 3.1 通用格式

所有配置文件采用统一的JSON格式，每个文件对应一种常量类型：

```json
{
  "<常量类型>": [
    {
      "code": 1001,
      "name": "CONSTANT_NAME",
      "description": "常量描述"
    }
  ]
}
```

其中 `<常量类型>` 是文件名（不含扩展名），例如：
- `error_codes.json` → `"error_codes"`
- `message_ids.json` → `"message_ids"`
- `item_ids.json` → `"item_ids"`

### 3.2 错误码配置 (error_codes.json)

```json
{
  "error_codes": [
    {
      "code": 0,
      "name": "SUCCESS",
      "description": "操作成功"
    },
    {
      "code": 1001,
      "name": "INVALID_PASSWORD",
      "description": "密码错误"
    },
    {
      "code": 1002,
      "name": "USER_NOT_FOUND",
      "description": "用户不存在"
    }
  ]
}
```

### 3.3 消息ID配置 (message_ids.json)

```json
{
  "message_ids": [
    {
      "code": 1,
      "name": "MSG_ID_HEARTBEAT",
      "description": "心跳消息"
    },
    {
      "code": 2,
      "name": "MSG_ID_HEARTBEAT_RESP",
      "description": "心跳响应"
    }
  ]
}
```

### 3.4 物品ID配置 (item_ids.json)

```json
{
  "item_ids": [
    {
      "code": 1,
      "name": "ITEM_NONE",
      "description": "无物品"
    },
    {
      "code": 3,
      "name": "ITEM_AXE",
      "description": "斧头"
    }
  ]
}
```

## 4. 代码生成

### 4.1 Python 生成规则

**输出文件**：`scripts/client/{config_name}.py`

**生成格式**：
```python
# 自动生成，请勿手动修改
# 生成时间：2026-06-01 22:30:00
# 源文件：shared/error_codes.json

class ErrorCode:
    """错误码常量定义"""
    SUCCESS = 0            # 操作成功
    INVALID_PASSWORD = 1001 # 密码错误
    USER_NOT_FOUND = 1002   # 用户不存在
```

### 4.2 C++ 生成规则

**输出文件**：`scripts/server/common/include/{config_name}.h`

**生成格式**：
```cpp
// 自动生成，请勿手动修改
// 生成时间：2026-06-01 22:30:00
// 源文件：shared/error_codes.json
#pragma once

#include <cstdint>

namespace farm {

enum class ErrorCode : uint32_t {
    SUCCESS = 0,            // 操作成功
    INVALID_PASSWORD = 1001, // 密码错误
    USER_NOT_FOUND = 1002   // 用户不存在
};

} // namespace farm
```

### 4.3 命名规范

- **常量名称**：使用 UPPER_SNAKE_CASE
- **类名/枚举名**：使用 PascalCase
- **文件名**：使用 snake_case

## 5. 文件监控服务

### 5.1 技术方案

使用 Python `watchdog` 库实现文件监控：

```python
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

class ConfigFileHandler(FileSystemEventHandler):
    def on_modified(self, event):
        if event.src_path.endswith('.json'):
            trigger_generation()
```

### 5.2 监控范围

- **监控目录**：`shared/`
- **监控文件类型**：`*.json`
- **触发事件**：文件创建、修改、删除

### 5.3 防抖机制

为避免频繁触发，实现防抖机制：
- 文件变化后等待 500ms
- 如果 500ms 内再次变化，重置计时器
- 计时器到期后触发生成

## 6. 错误处理

### 6.1 配置验证

在生成代码前验证配置文件：

1. **JSON格式验证**：确保文件是有效的JSON
2. **必需字段检查**：确保每个常量包含 `code`、`name` 字段
3. **名称唯一性**：确保常量名称不重复
4. **值唯一性**：确保常量值不冲突

### 6.2 生成失败处理

- **验证失败**：输出详细错误信息，不生成代码
- **写入失败**：保留原文件，输出错误信息
- **回滚机制**：生成失败时，恢复到上一个有效版本

### 6.3 日志记录

所有操作记录到日志文件：
- 文件监控事件
- 生成开始/结束时间
- 错误和警告信息

## 7. 集成方案

### 7.1 开发环境

1. 启动文件监控服务：
   ```bash
   python scripts/tools/file_watcher.py
   ```

2. 修改 `shared/` 目录下的配置文件

3. 自动生成代码文件

### 7.2 构建流程

在 `build_all.bat` 中添加代码生成步骤：

```batch
REM 生成共享常量代码
echo Generating shared constants...
python scripts/tools/generate_constants.py
if errorlevel 1 (
    echo ERROR: Failed to generate shared constants
    exit /b 1
)
```

### 7.3 部署流程

1. 开发时：使用文件监控服务自动生成
2. 构建时：在构建脚本中调用生成脚本
3. 部署时：确保生成的代码文件包含在部署包中

## 8. 迁移策略

### 8.1 现有代码迁移

1. **消息ID迁移**：
   - 将 `scripts/common/proto/msg_ids.h` 中的定义迁移到 `shared/message_ids.json`
   - 将 `scripts/client/msg_ids.py` 中的定义迁移到 `shared/message_ids.json`
   - 生成新的代码文件替换原文件

2. **错误码迁移**：
   - 创建 `shared/error_codes.json`
   - 生成 `scripts/client/error_codes.py` 和 `scripts/server/common/include/error_codes.h`

### 8.2 向后兼容

- 保持现有的 `#include` 和 `import` 路径不变
- 生成的代码文件使用相同的命名空间和类名
- 现有代码无需修改即可使用新生成的常量

## 9. 测试策略

### 9.1 单元测试

1. **配置验证测试**：验证各种错误配置的处理
2. **代码生成测试**：验证生成的Python和C++代码正确性
3. **文件监控测试**：验证文件变化检测和触发机制

### 9.2 集成测试

1. **端到端测试**：从配置文件到生成代码的完整流程
2. **并发测试**：多文件同时变化的处理
3. **错误恢复测试**：生成失败后的恢复机制

## 10. 性能考虑

### 10.1 文件监控性能

- 使用 `watchdog` 库的原生文件系统监控
- 避免轮询，减少CPU使用
- 防抖机制减少不必要的生成

### 10.2 代码生成性能

- 增量生成：只重新生成变化的文件
- 缓存机制：缓存已解析的配置文件
- 异步处理：生成过程不阻塞文件监控

## 11. 扩展性

### 11.1 新增常量类型

添加新的常量类型只需：
1. 在 `shared/` 目录下创建新的JSON文件
2. 在 `generate_constants.py` 中添加对应的生成逻辑（模板已内嵌在脚本中）

### 11.2 新增目标语言

添加新的目标语言只需：
1. 在 `generate_constants.py` 中添加对应的生成函数和模板

## 12. 风险与缓解

### 12.1 风险

1. **文件损坏**：生成过程中断可能导致文件损坏
2. **性能影响**：频繁的文件监控可能影响系统性能
3. **兼容性问题**：新生成的代码可能与现有代码不兼容

### 12.2 缓解措施

1. **原子写入**：使用临时文件+重命名的方式写入
2. **性能优化**：防抖机制和增量生成
3. **兼容性测试**：生成前后进行代码对比和测试

## 13. 成功标准

1. **功能完整性**：支持所有共享常量的自动生成
2. **可靠性**：生成过程稳定，错误处理完善
3. **易用性**：配置简单，使用方便
4. **性能**：生成速度快，不影响开发体验
5. **可维护性**：代码清晰，易于扩展和修改
