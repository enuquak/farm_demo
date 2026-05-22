---
name: py-ai-coding-conventions
description: Python 通用 AI 编码规范：格式、命名、错误处理、日志、类型注解、代码组织等最佳实践。不涉及具体项目业务逻辑。
license: MIT
metadata:
  author: common
  version: "1.0"
---

# Python AI 编码规范

> 本文档是 Python 项目的**通用开发指导规范**。
> 适用于任何 Python 项目的基础编码、代码风格和工程实践。

---

## 一、编码格式

- 所有 `.py` 文件统一使用 **4 空格缩进**，**禁止使用 Tab**
- 换行符统一使用 **LF**（`\n`），**禁止使用 CRLF**（`\r\n`）
- 文件使用 **UTF-8** 编码，首行声明 `# -*- coding: utf-8 -*-`（Python 2 需要，Python 3 可省略）
- 文件末尾保留一个空行
- 每行最大宽度 **120 字符**（PEP 8 默认 79，项目可根据实际情况放宽）
- 运算符两侧加空格，逗号后加空格：

```python
# ✅ 正确
result = a + b
func(arg1, arg2, arg3)

# ❌ 错误
result=a+b
func(arg1,arg2,arg3)
```

---

## 二、命名规范

### 2.1 通用规则

- 使用有意义的英文命名，**禁止**拼音或无意义缩写
- 避免单字母变量（列表推导式中的循环变量除外）
- 布尔变量/函数用 `is_`、`has_`、`can_`、`should_` 前缀

### 2.2 各类型命名约定

| 类型 | 风格 | 示例 |
|------|------|------|
| 模块/包名 | 全小写下划线 | `tcp_server.py` |
| 类名 | PascalCase | `TcpConnection` |
| 函数/方法 | snake_case | `send_data()` |
| 实例变量 | snake_case | `self.buffer` |
| 类变量 | snake_case | `instance_count` |
| 常量 | 全大写下划线 | `MAX_RETRY_COUNT` |
| 私有成员 | 单下划线前缀 | `self._internal_state` |
| 名称修饰 | 双下划线前缀 | `self.__private_var`（慎用） |
| 类型变量 | PascalCase | `TypeVar('T')` |

---

## 三、导入规范

### 3.1 导入顺序

按以下顺序分组，组间空行分隔：

```python
# 1. 标准库
import os
import sys
from typing import List, Dict, Optional

# 2. 第三方库
import requests
from sqlalchemy import create_engine

# 3. 项目内模块
from myproject.core import handler
from myproject.utils import logger
```

### 3.2 导入风格

- 优先使用绝对导入
- 每行一个导入（`from x import a, b` 可接受但不推荐）
- **禁止** `from module import *`
- 模块内导入放在文件顶部，函数内延迟导入仅用于解决循环依赖

```python
# ✅ 正确
import os
import sys
from typing import List

# ❌ 错误
import os, sys
from typing import *
```

---

## 四、代码风格

### 4.1 Early Return

条件检查失败时应立即 return，**禁止**将后续逻辑放在 `else` 中：

```python
# ✅ 正确：early return，减少嵌套
def process_packet(self, data):
    if not self._validate_header(data):
        logger.error(f"process_packet, invalid header, {data[:20]=}")
        return False

    if not self._check_auth(data):
        logger.error(f"process_packet, auth failed")
        return False

    # 正常逻辑在顶层
    self._do_business(data)
    return True

# ❌ 错误：不必要的 else 增加嵌套
def process_packet(self, data):
    if not self._validate_header(data):
        logger.error("invalid header")
        return False
    else:
        if not self._check_auth(data):
            logger.error("auth failed")
            return False
        else:
            self._do_business(data)
            return True
```

### 4.2 return 后空行

`return` 语句后**必须**跟一个空行，与后续逻辑分隔：

```python
def calculate(self, value):
    if value < 0:
        return 0

    result = value * 2
    return result
```

### 4.3 类型注解（推荐）

公开接口建议添加类型注解，提高可读性和 IDE 支持：

```python
def send_data(self, data: bytes, timeout: float = 5.0) -> bool:
    """发送数据到远程端。"""
    ...

class PacketHandler:
    def __init__(self) -> None:
        self._handlers: Dict[int, Callable] = {}
```

---

## 五、错误处理

### 5.1 错误码

- 使用枚举或常量类定义错误码，避免裸数字
- 错误码命名清晰，附带注释说明

```python
from enum import IntEnum

class RetCode(IntEnum):
    SUCCESS         = 0
    INVALID_PARAM   = 1    # 参数不合法
    NETWORK_ERROR   = 2    # 网络异常
    TIMEOUT         = 3    # 超时
    AUTH_FAILED     = 4    # 认证失败
```

### 5.2 异常使用

- 捕获具体异常，**禁止**裸 `except:` 或 `except Exception:`
- 异常中保留原始信息（`raise ... from e`）
- 自定义异常继承自合适的内置异常类

```python
# ✅ 正确
try:
    result = json.loads(data)
except json.JSONDecodeError as e:
    logger.error(f"parse_json, decode failed, {e}")
    return None

# ❌ 错误
try:
    result = json.loads(data)
except:
    return None
```

### 5.3 资源管理

- 文件、锁、网络连接等资源使用 `with` 语句（上下文管理器）
- **禁止**手动 `open`/`close` 配对

```python
# ✅ 正确
with open("config.json", "r") as f:
    data = json.load(f)

# ❌ 错误
f = open("config.json", "r")
data = json.load(f)
f.close()  # 如果中间抛异常则文件未关闭
```

---

## 六、日志规范

### 6.1 格式约定

- 推荐使用 **f-string** 格式
- 日志消息以**方法名**开头，便于后续检索
- 使用 `{var=}` 语法同时打印变量名和值（Python 3.8+）

```python
# 推荐格式：方法名, 描述, key=value
logger.info(f"on_connect, new client connected, {fd=}, {addr=}")
logger.error(f"send_data, send failed, {fd=}, {errno=}")
logger.warning(f"check_timeout, connection slow, {fd=}, {latency_ms=}")
```

### 6.2 日志级别

| 级别 | 用途 |
|------|------|
| `DEBUG` | 开发调试信息，生产环境关闭 |
| `INFO` | 正常业务流程的关键节点 |
| `WARNING` | 异常但可恢复的情况（如参数不合法、条件不满足） |
| `ERROR` | 不应发生的错误（如数据不一致、外部调用失败） |
| `CRITICAL` | 严重错误，系统可能无法继续运行 |

### 6.3 注意事项

- 循环内高频日志用 `DEBUG` 级别或加频率限制
- 错误日志必须包含足够的上下文信息
- **禁止**在日志中打印敏感信息（密码、密钥、token）
- 使用 `logging` 模块，**禁止**直接 `print()`

---

## 七、字典访问规范

- 使用 `.get(key, default)` 访问字典，**禁止** `[]` 下标（防 `KeyError`）
- 嵌套字典逐层 `.get()` 或使用工具函数

```python
# ✅ 正确
value = config.get("database", {}).get("host", "localhost")

# ❌ 错误
value = config["database"]["host"]  # KeyError if key missing
```

---

## 八、类设计

### 8.1 方法顺序

```python
class MyClass:
    # 1. 类变量
    class_var = 42

    # 2. __init__
    def __init__(self, name: str) -> None:
        self._name = name

    # 3. 公开方法
    def do_something(self) -> None:
        ...

    # 4. 私有方法
    def _internal_helper(self) -> None:
        ...

    # 5. 魔术方法（非 __init__）
    def __repr__(self) -> str:
        return f"MyClass({self._name!r})"
```

### 8.2 数据类

简单数据容器优先使用 `dataclasses` 或 `NamedTuple`：

```python
from dataclasses import dataclass

@dataclass
class PlayerInfo:
    uid: int
    name: str
    level: int = 1
```

---

## 九、注释规范

### 9.1 文件头

```python
"""
模块功能简述。
"""
```

### 9.2 函数/方法文档字符串

```python
def send_data(self, data: bytes, timeout: float = 5.0) -> bool:
    """发送数据到远程端。

    Args:
        data: 要发送的字节数据
        timeout: 超时时间（秒），默认 5.0

    Returns:
        成功返回 True，失败返回 False
    """
```

### 9.3 注释原则

- 解释 **为什么**，而不是 **做了什么**（代码本身说明做了什么）
- 复杂逻辑、算法、workaround 必须加注释
- **禁止**注释掉的代码保留在提交中，直接删除（git 可追溯）

---

## 十、模块组织

### 10.1 文件结构

```
module_name/
├── __init__.py        # 包初始化，导出公共接口
├── core.py            # 核心逻辑
├── config.py          # 配置相关
├── utils.py           # 工具函数
└── exceptions.py      # 自定义异常
```

### 10.2 `__init__.py` 原则

- 保持简洁，主要做导入导出
- 避免在 `__init__.py` 中写复杂逻辑
- 明确导出 `__all__` 列表

---

## 十一、功能开发 Checklist

新增功能时，按以下清单逐项检查：

- [ ] 命名符合项目统一规范（snake_case 函数、PascalCase 类等）
- [ ] 导入按标准库 → 第三方 → 项目内分组
- [ ] 禁止 `from module import *`
- [ ] 字典访问使用 `.get()` 而非 `[]`
- [ ] 异常捕获具体类型，不裸 `except`
- [ ] 资源管理使用 `with` 语句
- [ ] 使用 `logging` 模块而非 `print()`
- [ ] 关键函数有文档字符串
- [ ] 无未使用的导入和变量
- [ ] 换行符为 LF，缩进为 4 空格
