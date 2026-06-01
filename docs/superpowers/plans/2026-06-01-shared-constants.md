# 共享常量自动生成机制 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立自动化机制，将客户端和服务器共用的常量统一存放在 shared/ 目录下，通过文件监控服务自动检测变化并生成对应语言的代码文件。

**Architecture:** 使用 JSON 作为单一数据源，Python watchdog 库监控文件变化，生成 Python 类和 C++ 枚举。模板内嵌在生成脚本中，支持错误码、消息ID、物品ID等多种常量类型。

**Tech Stack:** Python 3.12, watchdog, JSON

---

## File Structure

```
shared/                              # 新建 - 配置源文件目录
├── error_codes.json                 # 新建 - 错误码配置
└── message_ids.json                 # 新建 - 消息ID配置

tools/                               # 已存在
├── generate_constants.py            # 新建 - 代码生成主脚本
├── file_watcher.py                  # 新建 - 文件监控服务
└── build_all.bat                    # 修改 - 添加代码生成步骤

scripts/client/                      # 已存在
├── error_codes.py                   # 新建 - 自动生成的错误码
└── msg_ids.py                       # 替换 - 改为自动生成

scripts/server/common/include/       # 已存在
├── error_codes.h                    # 新建 - 自动生成的错误码
└── msg_ids.h                        # 替换 - 改为自动生成（路径变更）
```

---

### Task 1: 创建 shared 目录和错误码配置

**Files:**
- Create: `shared/error_codes.json`

- [ ] **Step 1: 创建 shared 目录**

```bash
mkdir -p shared
```

- [ ] **Step 2: 创建 error_codes.json**

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
    },
    {
      "code": 1003,
      "name": "ALREADY_LOGGED_IN",
      "description": "用户已登录"
    },
    {
      "code": 2001,
      "name": "INVALID_ITEM",
      "description": "无效物品"
    },
    {
      "code": 2002,
      "name": "INVENTORY_FULL",
      "description": "背包已满"
    }
  ]
}
```

- [ ] **Step 3: 验证 JSON 格式**

```bash
python -c "import json; json.load(open('shared/error_codes.json', encoding='utf-8')); print('JSON valid')"
```

Expected: `JSON valid`

- [ ] **Step 4: 提交**

```bash
git add shared/error_codes.json
git commit -m "feat(config): add shared error codes configuration"
```

---

### Task 2: 创建消息ID配置（从现有代码迁移）

**Files:**
- Create: `shared/message_ids.json`

- [ ] **Step 1: 创建 message_ids.json**

从 `scripts/client/msg_ids.py` 和 `scripts/common/proto/msg_ids.h` 提取现有定义：

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
    },
    {
      "code": 3,
      "name": "MSG_ID_LOGIN_REQ",
      "description": "登录请求"
    },
    {
      "code": 4,
      "name": "MSG_ID_LOGIN_RESP",
      "description": "登录响应"
    },
    {
      "code": 1001,
      "name": "MSG_ID_QUERY_ROLES_REQ",
      "description": "查询角色请求"
    },
    {
      "code": 1002,
      "name": "MSG_ID_QUERY_ROLES_RESP",
      "description": "查询角色响应"
    },
    {
      "code": 1003,
      "name": "MSG_ID_CREATE_ROLE_REQ",
      "description": "创建角色请求"
    },
    {
      "code": 1004,
      "name": "MSG_ID_CREATE_ROLE_RESP",
      "description": "创建角色响应"
    },
    {
      "code": 1005,
      "name": "MSG_ID_ENTER_GAME_REQ",
      "description": "进入游戏请求"
    },
    {
      "code": 1006,
      "name": "MSG_ID_ENTER_GAME_RESP",
      "description": "进入游戏响应"
    },
    {
      "code": 2001,
      "name": "MSG_ID_MAP_DATA_NOTIFY",
      "description": "地图数据通知"
    },
    {
      "code": 2101,
      "name": "MSG_ID_POSITION_UPDATE",
      "description": "位置更新"
    },
    {
      "code": 2102,
      "name": "MSG_ID_POSITION_CORRECT",
      "description": "位置纠正"
    },
    {
      "code": 2201,
      "name": "MSG_ID_SCENE_CHANGE_REQ",
      "description": "场景切换请求"
    },
    {
      "code": 2202,
      "name": "MSG_ID_SCENE_CHANGE_RESP",
      "description": "场景切换响应"
    },
    {
      "code": 2301,
      "name": "MSG_ID_CLOCK_SYNC",
      "description": "时钟同步"
    },
    {
      "code": 2302,
      "name": "MSG_ID_FORCE_SLEEP_NOTIFY",
      "description": "强制睡觉通知"
    },
    {
      "code": 2303,
      "name": "MSG_ID_FORCE_SLEEP_READY",
      "description": "强制睡觉就绪"
    },
    {
      "code": 3001,
      "name": "MSG_ID_ITEM_USE_REQ",
      "description": "物品使用请求"
    },
    {
      "code": 3002,
      "name": "MSG_ID_ITEM_USE_RESP",
      "description": "物品使用响应"
    },
    {
      "code": 3003,
      "name": "MSG_ID_DROP_ITEM_SYNC",
      "description": "掉落物同步"
    },
    {
      "code": 3100,
      "name": "MSG_ID_INVENTORY_SYNC",
      "description": "背包数据同步"
    },
    {
      "code": 3101,
      "name": "MSG_ID_ACTIVE_SLOT_CHANGE",
      "description": "快捷栏选中格切换"
    }
  ]
}
```

- [ ] **Step 2: 验证 JSON 格式**

```bash
python -c "import json; json.load(open('shared/message_ids.json', encoding='utf-8')); print('JSON valid')"
```

Expected: `JSON valid`

- [ ] **Step 3: 提交**

```bash
git add shared/message_ids.json
git commit -m "feat(config): add shared message IDs configuration"
```

---

### Task 3: 创建代码生成脚本

**Files:**
- Create: `tools/generate_constants.py`

- [ ] **Step 1: 创建 generate_constants.py**

```python
#!/usr/bin/env python3
"""
常量代码生成器
读取 shared/ 目录下的 JSON 配置文件，生成 Python 和 C++ 代码
"""

import json
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Any


def load_json_config(file_path: Path) -> Dict[str, Any]:
    """加载 JSON 配置文件"""
    with open(file_path, 'r', encoding='utf-8') as f:
        return json.load(f)


def validate_config(config: Dict[str, Any], file_path: Path) -> List[str]:
    """验证配置文件格式"""
    errors = []

    # 检查顶层结构
    if not isinstance(config, dict):
        errors.append(f"{file_path}: 配置必须是 JSON 对象")
        return errors

    # 获取常量列表
    constants_key = file_path.stem
    if constants_key not in config:
        errors.append(f"{file_path}: 缺少 '{constants_key}' 键")
        return errors

    constants = config[constants_key]
    if not isinstance(constants, list):
        errors.append(f"{file_path}: '{constants_key}' 必须是数组")
        return errors

    # 检查每个常量
    names_seen = set()
    codes_seen = set()

    for i, item in enumerate(constants):
        if not isinstance(item, dict):
            errors.append(f"{file_path}: 第 {i+1} 项必须是对象")
            continue

        # 检查必需字段
        if 'code' not in item:
            errors.append(f"{file_path}: 第 {i+1} 项缺少 'code' 字段")
        if 'name' not in item:
            errors.append(f"{file_path}: 第 {i+1} 项缺少 'name' 字段")

        # 检查名称唯一性
        name = item.get('name')
        if name:
            if name in names_seen:
                errors.append(f"{file_path}: 名称 '{name}' 重复")
            names_seen.add(name)

            # 检查命名规范
            if not name.isupper() or not name.replace('_', '').isalnum():
                errors.append(f"{file_path}: 名称 '{name}' 不符合 UPPER_SNAKE_CASE 规范")

        # 检查值唯一性
        code = item.get('code')
        if code is not None:
            if code in codes_seen:
                errors.append(f"{file_path}: 值 {code} 重复")
            codes_seen.add(code)

    return errors


def to_pascal_case(snake_str: str) -> str:
    """将 snake_case 转换为 PascalCase"""
    return ''.join(word.capitalize() for word in snake_str.split('_'))


def generate_python_code(config_name: str, constants: List[Dict[str, Any]], output_path: Path) -> None:
    """生成 Python 代码"""
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    class_name = to_pascal_case(config_name)

    lines = [
        '# 自动生成，请勿手动修改',
        f'# 生成时间：{timestamp}',
        f'# 源文件：shared/{config_name}.json',
        '',
        '',
        f'class {class_name}:',
        f'    """{config_name} 常量定义"""',
    ]

    for item in constants:
        name = item['name']
        code = item['code']
        desc = item.get('description', '')
        lines.append(f'    {name} = {code}  # {desc}')

    lines.append('')  # 文件末尾换行

    # 原子写入：先写临时文件，再重命名
    temp_path = output_path.with_suffix('.py.tmp')
    with open(temp_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    # 备份原文件
    if output_path.exists():
        backup_path = output_path.with_suffix('.py.bak')
        output_path.rename(backup_path)

    temp_path.rename(output_path)


def generate_cpp_code(config_name: str, constants: List[Dict[str, Any]], output_path: Path) -> None:
    """生成 C++ 代码"""
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    enum_name = to_pascal_case(config_name)

    lines = [
        '// 自动生成，请勿手动修改',
        f'// 生成时间：{timestamp}',
        f'// 源文件：shared/{config_name}.json',
        '#pragma once',
        '',
        '#include <cstdint>',
        '',
        'namespace farm {',
        '',
        f'enum class {enum_name} : uint32_t {{',
    ]

    for i, item in enumerate(constants):
        name = item['name']
        code = item['code']
        desc = item.get('description', '')
        comma = ',' if i < len(constants) - 1 else ''
        lines.append(f'    {name} = {code}{comma}  // {desc}')

    lines.extend([
        '};',
        '',
        '} // namespace farm',
        '',  # 文件末尾换行
    ])

    # 原子写入：先写临时文件，再重命名
    temp_path = output_path.with_suffix('.h.tmp')
    with open(temp_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    # 备份原文件
    if output_path.exists():
        backup_path = output_path.with_suffix('.h.bak')
        output_path.rename(backup_path)

    temp_path.rename(output_path)


def process_config_file(json_file: Path, project_root: Path) -> bool:
    """处理单个配置文件"""
    config_name = json_file.stem

    try:
        # 加载配置
        config = load_json_config(json_file)

        # 验证配置
        errors = validate_config(config, json_file)
        if errors:
            for error in errors:
                print(f"错误: {error}", file=sys.stderr)
            return False

        constants = config[config_name]

        # 生成 Python 代码
        python_output = project_root / 'scripts' / 'client' / f'{config_name}.py'
        generate_python_code(config_name, constants, python_output)
        print(f"生成 Python 代码: {python_output}")

        # 生成 C++ 代码
        cpp_output = project_root / 'scripts' / 'server' / 'common' / 'include' / f'{config_name}.h'
        generate_cpp_code(config_name, constants, cpp_output)
        print(f"生成 C++ 代码: {cpp_output}")

        return True

    except json.JSONDecodeError as e:
        print(f"错误: {json_file} JSON 格式错误: {e}", file=sys.stderr)
        return False
    except Exception as e:
        print(f"错误: 处理 {json_file} 时出错: {e}", file=sys.stderr)
        return False


def main() -> int:
    """主函数"""
    # 获取项目根目录
    project_root = Path(__file__).parent.parent
    shared_dir = project_root / 'shared'

    # 检查 shared 目录是否存在
    if not shared_dir.exists():
        print(f"错误: shared 目录不存在: {shared_dir}", file=sys.stderr)
        return 1

    # 遍历 shared 目录下的所有 JSON 文件
    json_files = list(shared_dir.glob('*.json'))
    if not json_files:
        print("警告: shared 目录下没有 JSON 文件")
        return 0

    success_count = 0
    fail_count = 0

    for json_file in sorted(json_files):
        print(f"\n处理文件: {json_file}")
        if process_config_file(json_file, project_root):
            success_count += 1
        else:
            fail_count += 1

    print(f"\n生成完成: 成功 {success_count}, 失败 {fail_count}")

    return 1 if fail_count > 0 else 0


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 2: 测试生成脚本**

```bash
python tools/generate_constants.py
```

Expected:
```
处理文件: D:\mb_workspace\farm_demo\shared\error_codes.json
生成 Python 代码: D:\mb_workspace\farm_demo\scripts\client\error_codes.py
生成 C++ 代码: D:\mb_workspace\farm_demo\scripts\server\common\include\error_codes.h

处理文件: D:\mb_workspace\farm_demo\shared\message_ids.json
生成 Python 代码: D:\mb_workspace\farm_demo\scripts\client\msg_ids.py
生成 C++ 代码: D:\mb_workspace\farm_demo\scripts\server\common\include\msg_ids.h

生成完成: 成功 2, 失败 0
```

- [ ] **Step 3: 验证生成的 Python 文件**

```bash
python -c "from scripts.client.error_codes import ErrorCode; print(ErrorCode.SUCCESS, ErrorCode.INVALID_PASSWORD)"
```

Expected: `0 1001`

- [ ] **Step 4: 验证生成的 C++ 文件**

```bash
head -20 scripts/server/common/include/error_codes.h
```

Expected: 看到正确的 C++ 枚举定义

- [ ] **Step 5: 提交**

```bash
git add tools/generate_constants.py
git commit -m "feat(tools): add constants code generator script"
```

---

### Task 4: 创建文件监控服务

**Files:**
- Create: `tools/file_watcher.py`

- [ ] **Step 1: 安装 watchdog 依赖**

```bash
pip install watchdog
```

- [ ] **Step 2: 创建 file_watcher.py**

```python
#!/usr/bin/env python3
"""
文件监控服务
监控 shared/ 目录下的 JSON 文件变化，自动触发代码生成
"""

import os
import sys
import time
import threading
from pathlib import Path
from typing import Set

from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler, FileModifiedEvent, FileCreatedEvent, FileDeletedEvent

# 导入生成脚本
sys.path.insert(0, str(Path(__file__).parent))
from generate_constants import process_config_file


class ConfigFileHandler(FileSystemEventHandler):
    """配置文件变化处理器"""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.shared_dir = project_root / 'shared'
        self.pending_files: Set[Path] = set()
        self.debounce_timer: threading.Timer | None = None
        self.debounce_delay = 0.5  # 500ms 防抖延迟

    def _is_config_file(self, path: str) -> bool:
        """检查是否是配置文件"""
        return path.endswith('.json') and 'shared' in path

    def _debounce_callback(self) -> None:
        """防抖回调：处理所有待处理的文件"""
        if not self.pending_files:
            return

        print(f"\n检测到文件变化，开始重新生成...")

        success_count = 0
        fail_count = 0

        for json_file in sorted(self.pending_files):
            if json_file.exists():  # 文件删除时不处理
                if process_config_file(json_file, self.project_root):
                    success_count += 1
                else:
                    fail_count += 1

        self.pending_files.clear()
        print(f"重新生成完成: 成功 {success_count}, 失败 {fail_count}")

    def _schedule_generation(self, path: str) -> None:
        """调度生成任务（带防抖）"""
        if not self._is_config_file(path):
            return

        json_file = Path(path)
        self.pending_files.add(json_file)

        # 取消之前的定时器
        if self.debounce_timer is not None:
            self.debounce_timer.cancel()

        # 创建新的定时器
        self.debounce_timer = threading.Timer(self.debounce_delay, self._debounce_callback)
        self.debounce_timer.daemon = True
        self.debounce_timer.start()

    def on_modified(self, event: FileModifiedEvent) -> None:
        """文件修改事件"""
        if not event.is_directory:
            self._schedule_generation(event.src_path)

    def on_created(self, event: FileCreatedEvent) -> None:
        """文件创建事件"""
        if not event.is_directory:
            self._schedule_generation(event.src_path)

    def on_deleted(self, event: FileDeletedEvent) -> None:
        """文件删除事件"""
        if not event.is_directory:
            self._schedule_generation(event.src_path)


def main() -> int:
    """主函数"""
    # 获取项目根目录
    project_root = Path(__file__).parent.parent
    shared_dir = project_root / 'shared'

    # 检查 shared 目录是否存在
    if not shared_dir.exists():
        print(f"错误: shared 目录不存在: {shared_dir}", file=sys.stderr)
        return 1

    print(f"启动文件监控服务...")
    print(f"监控目录: {shared_dir}")
    print(f"按 Ctrl+C 停止服务")

    # 创建事件处理器
    event_handler = ConfigFileHandler(project_root)

    # 创建观察者
    observer = Observer()
    observer.schedule(event_handler, str(shared_dir), recursive=False)

    # 启动观察者
    observer.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n停止文件监控服务...")
        observer.stop()

    observer.join()
    print("文件监控服务已停止")

    return 0


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 3: 测试文件监控服务**

启动监控服务：
```bash
python tools/file_watcher.py
```

Expected:
```
启动文件监控服务...
监控目录: D:\mb_workspace\farm_demo\shared
按 Ctrl+C 停止服务
```

- [ ] **Step 4: 测试文件变化检测**

在另一个终端修改 shared/error_codes.json，添加一个新错误码：

```bash
echo '{"error_codes":[{"code":0,"name":"SUCCESS","description":"操作成功"},{"code":1001,"name":"INVALID_PASSWORD","description":"密码错误"}]}' > shared/error_codes.json
```

Expected: 监控服务输出检测到变化并重新生成

- [ ] **Step 5: 提交**

```bash
git add tools/file_watcher.py
git commit -m "feat(tools): add file watcher service for auto-generation"
```

---

### Task 5: 集成到构建流程

**Files:**
- Modify: `tools/build_all.bat`

- [ ] **Step 1: 修改 build_all.bat**

在构建服务之前添加代码生成步骤：

```batch
@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Build All Services
echo ========================================

REM 获取脚本所在目录的父目录（项目根目录）
set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"

echo Project Directory: %PROJECT_DIR%
echo.

REM 生成共享常量代码
echo ========================================
echo Generating shared constants...
echo ========================================
python tools\generate_constants.py
if errorlevel 1 (
    echo ERROR: Failed to generate shared constants
    exit /b 1
)
echo.

REM 定义服务列表
set "SERVICES=dbmgr game_server gate_server"

REM 创建 bin 目录（如果不存在）
if not exist "bin" (
    echo Creating bin directory...
    mkdir bin
)

REM 遍历每个服务
set "BUILD_SUCCESS=0"
set "BUILD_FAILED=0"

for %%s in (%SERVICES%) do (
    echo.
    echo ========================================
    echo Building %%s...
    echo ========================================

    REM 检查服务目录是否存在
    if not exist "scripts\server\%%s" (
        echo ERROR: Service directory not found: scripts\server\%%s
        set /a "BUILD_FAILED+=1"
        goto :next_service
    )

    REM 检查 main.cpp 是否存在
    if not exist "scripts\server\%%s\src\main.cpp" (
        echo ERROR: main.cpp not found: scripts\server\%%s\src\main.cpp
        set /a "BUILD_FAILED+=1"
        goto :next_service
    )

    REM 调用构建脚本
    echo Calling build_cpp14.bat for %%s...
    cmd /c "D:\mb_workspace\farm_demo\tool\build_cpp14.bat" "D:\mb_workspace\farm_demo\scripts\server\%%s"

    if errorlevel 1 (
        echo ERROR: Build failed for %%s
        set /a "BUILD_FAILED+=1"
    ) else (
        echo Build successful for %%s

        REM 复制 exe 到 bin 目录
        if exist "scripts\server\%%s\Release\%%s.exe" (
            echo Copying %%s.exe to bin\...
            copy /y "scripts\server\%%s\Release\%%s.exe" "bin\%%s.exe" >nul
            if errorlevel 1 (
                echo WARNING: Failed to copy %%s.exe to bin\
            ) else (
                echo Copied %%s.exe to bin\
            )
        ) else (
            echo WARNING: %%s.exe not found in Release directory
        )

        set /a "BUILD_SUCCESS+=1"
    )

    :next_service
)

echo.
echo ========================================
echo Build Summary
echo ========================================
echo Successful: %BUILD_SUCCESS%
echo Failed: %BUILD_FAILED%
echo.

REM 检查 bin 目录内容
echo Contents of bin\:
dir /b bin\ 2>nul
echo.

REM 检查 DLL 依赖
echo Checking DLL dependencies...
set "MISSING_DLL=0"

REM 检查 libevent DLL
if not exist "bin\event.dll" (
    echo WARNING: Missing event.dll
    set /a "MISSING_DLL+=1"
)
if not exist "bin\event_core.dll" (
    echo WARNING: Missing event_core.dll
    set /a "MISSING_DLL+=1"
)
if not exist "bin\event_extra.dll" (
    echo WARNING: Missing event_extra.dll
    set /a "MISSING_DLL+=1"
)

if %MISSING_DLL% gtr 0 (
    echo.
    echo WARNING: %MISSING_DLL% DLL(s) missing from bin\ directory
    echo Please copy DLLs from C:\libevent_install\lib\ to bin\
    echo.
    echo Required DLLs:
    echo   - event.dll
    echo   - event_core.dll
    echo   - event_extra.dll
) else (
    echo All DLL dependencies found
)

echo.
if %BUILD_FAILED% equ 0 (
    echo ========================================
    echo All services built successfully
    echo ========================================
) else (
    echo ========================================
    echo Build completed with %BUILD_FAILED% failure(s)
    echo ========================================
)

endlocal
```

- [ ] **Step 2: 测试构建流程**

```bash
tools/build_all.bat
```

Expected: 看到代码生成步骤在构建服务之前执行

- [ ] **Step 3: 提交**

```bash
git add tools/build_all.bat
git commit -m "build: integrate constants generation into build process"
```

---

### Task 6: 清理旧的手动维护文件

**Files:**
- Delete: `scripts/common/proto/msg_ids.h` (已被自动生成的文件替代)
- Modify: `scripts/client/msg_ids.py` (已改为自动生成)

- [ ] **Step 1: 确认生成的文件正确**

```bash
python -c "from scripts.client.msg_ids import MSG_ID_HEARTBEAT; print(MSG_ID_HEARTBEAT)"
```

Expected: `1`

- [ ] **Step 2: 删除旧的 msg_ids.h**

```bash
git rm scripts/common/proto/msg_ids.h
```

- [ ] **Step 3: 更新 C++ 代码中的 include 路径**

检查所有包含 `msg_ids.h` 的 C++ 文件，更新 include 路径：

```bash
grep -r "msg_ids.h" scripts/server/ --include="*.cpp" --include="*.h"
```

将 `#include "msg_ids.h"` 或 `#include "common/proto/msg_ids.h"` 更新为 `#include "msg_ids.h"`（因为新文件在 `scripts/server/common/include/` 目录下）

- [ ] **Step 4: 提交**

```bash
git add -A
git commit -m "refactor: remove manually maintained msg_ids.h, use generated version"
```

---

### Task 7: 添加日志记录

**Files:**
- Modify: `tools/generate_constants.py`
- Modify: `tools/file_watcher.py`

- [ ] **Step 1: 添加日志配置到 generate_constants.py**

在文件开头添加：

```python
import logging
from datetime import datetime

# 配置日志
log_dir = Path(__file__).parent.parent / 'logs'
log_dir.mkdir(exist_ok=True)

log_file = log_dir / 'generate_constants.log'
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(log_file, encoding='utf-8'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)
```

- [ ] **Step 2: 更新日志记录**

将 `print` 语句替换为 `logger.info` 和 `logger.error`

- [ ] **Step 3: 添加日志配置到 file_watcher.py**

类似地添加日志配置

- [ ] **Step 4: 测试日志记录**

```bash
python tools/generate_constants.py
cat logs/generate_constants.log
```

Expected: 看到日志文件中有生成记录

- [ ] **Step 5: 提交**

```bash
git add tools/generate_constants.py tools/file_watcher.py
git commit -m "feat(tools): add logging to constants generation"
```

---

### Task 8: 添加 README 文档

**Files:**
- Create: `shared/README.md`

- [ ] **Step 1: 创建 README.md**

```markdown
# 共享常量配置

本目录存放客户端和服务器共用的常量配置文件。

## 文件格式

每个 JSON 文件对应一种常量类型，格式如下：

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

## 命名规范

- **文件名**：使用 snake_case，如 `error_codes.json`
- **常量名称**：使用 UPPER_SNAKE_CASE，如 `INVALID_PASSWORD`
- **类名/枚举名**：使用 PascalCase，如 `ErrorCode`

## 使用方法

### 手动生成

```bash
python tools/generate_constants.py
```

### 自动监控

```bash
python tools/file_watcher.py
```

修改本目录下的 JSON 文件后，会自动生成对应的 Python 和 C++ 代码文件。

## 生成的文件

- **Python**：`scripts/client/<常量类型>.py`
- **C++**：`scripts/server/common/include/<常量类型>.h`

## 现有配置

- `error_codes.json` - 错误码定义
- `message_ids.json` - 消息ID定义
```

- [ ] **Step 2: 提交**

```bash
git add shared/README.md
git commit -m "docs: add README for shared constants directory"
```

---

### Task 9: 验证完整流程

- [ ] **Step 1: 清理生成的文件**

```bash
rm -f scripts/client/error_codes.py scripts/client/msg_ids.py
rm -f scripts/server/common/include/error_codes.h scripts/server/common/include/msg_ids.h
```

- [ ] **Step 2: 运行生成脚本**

```bash
python tools/generate_constants.py
```

Expected: 所有文件重新生成成功

- [ ] **Step 3: 验证 Python 导入**

```bash
python -c "from scripts.client.error_codes import ErrorCode; from scripts.client.msg_ids import MessageIds; print('SUCCESS')"
```

Expected: `SUCCESS`

- [ ] **Step 4: 验证 C++ 编译**

如果可能，验证 C++ 代码可以正确编译

- [ ] **Step 5: 测试文件监控**

启动监控服务，修改一个 JSON 文件，验证自动生成

- [ ] **Step 6: 最终提交**

```bash
git add -A
git commit -m "feat: complete shared constants auto-generation mechanism"
```
