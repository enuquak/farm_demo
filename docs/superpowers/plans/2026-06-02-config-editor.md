# 配置编辑器 & 导表工具实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建一个配置编辑器 GUI 工具和独立导表引擎，支持从 Excel 读取游戏配置数据并生成 Python/C++ 代码。

**Architecture:** 模块化架构，导表引擎 (`tools/table_export/`) 独立于编辑器 (`tools/config_editor/`)，可单独在命令行使用。编辑器使用 CustomTkinter，左侧文件列表 + 右侧表格编辑区布局。

**Tech Stack:** Python 3, openpyxl (Excel读写), customtkinter (GUI)

---

## 文件结构

```
farm_demo/
  tables/                              # Excel 源文件目录（新建）
    items.xlsx                         # 示例表
  tools/
    table_export/                      # 独立导表引擎（新建）
      __init__.py                      # 入口：export_table(), export_all(), CLI
      excel_reader.py                  # Excel 读取 + 解析
      validators.py                    # 数据校验
      code_generator.py                # Python/C++ 代码生成
    config_editor/                     # 配置编辑器 GUI（新建）
      __init__.py                      # 包入口
      app.py                           # 主窗口
      sidebar.py                       # 左侧文件列表
      table_editor.py                  # 表格编辑组件
      export_runner.py                 # GUI 导表触发
  scripts/
    client/data/                       # 生成的 Python 数据（新建目录）
    server/data/                       # 生成的 C++ 数据（新建目录）
```

---

### Task 1: 项目初始化

**Files:**
- Modify: `requirements.txt`
- Create: `tables/` (目录)
- Create: `tools/table_export/__init__.py`
- Create: `tools/config_editor/__init__.py`
- Create: `scripts/client/data/` (目录)
- Create: `scripts/server/data/` (目录)

- [ ] **Step 1: 添加 openpyxl 依赖**

将 `requirements.txt` 修改为：

```
# Shared constants auto-generation tools
watchdog>=6.0.0
# Config editor & table export
openpyxl>=3.1.0
```

- [ ] **Step 2: 创建目录结构和包文件**

```bash
mkdir -p tables tools/table_export tools/config_editor scripts/client/data scripts/server/data
```

创建 `tools/table_export/__init__.py`：

```python
# -*- coding: utf-8 -*-
"""表导出引擎 - 从 Excel 配置表生成 Python 和 C++ 代码。"""
```

创建 `tools/config_editor/__init__.py`：

```python
# -*- coding: utf-8 -*-
"""配置编辑器 GUI - 可视化编辑 Excel 配置表。"""
```

创建 `scripts/client/data/__init__.py`（空文件，使目录可被 Python 导入）：

```python
```

创建 `scripts/server/data/.gitkeep`（空文件，保持目录被 git 跟踪）。

- [ ] **Step 3: 安装依赖并验证**

```bash
cd D:/mb_workspace/farm_demo
pip install -r requirements.txt
python -c "import openpyxl; print(openpyxl.__version__)"
```

Expected: 输出 openpyxl 版本号（如 `3.1.5`）

- [ ] **Step 4: 提交**

```bash
git add requirements.txt tables/ tools/table_export/__init__.py tools/config_editor/__init__.py scripts/client/data/__init__.py scripts/server/data/.gitkeep
git commit -m "feat: initialize config editor project structure"
```

---

### Task 2: Excel 读取器 (excel_reader.py)

**Files:**
- Create: `tools/table_export/excel_reader.py`
- Create: `tests/test_excel_reader.py`

- [ ] **Step 1: 编写测试**

创建 `tests/test_excel_reader.py`：

```python
# -*- coding: utf-8 -*-
"""Excel 读取器测试。"""
import os
import tempfile
import pytest
from openpyxl import Workbook

from tools.table_export.excel_reader import read_excel


def _create_test_excel(path, fields, types, comments, data_rows):
    """辅助函数：创建测试用 Excel 文件。"""
    wb = Workbook()
    ws = wb.active
    ws.append(fields)
    ws.append(types)
    ws.append(comments)
    for row in data_rows:
        ws.append(row)
    wb.save(path)


class TestReadExcel:
    """read_excel 函数测试。"""

    def test_basic_read(self, tmp_path):
        """读取标准格式 Excel，返回正确结构。"""
        file_path = str(tmp_path / "items.xlsx")
        _create_test_excel(
            file_path,
            fields=["item_id", "name", "type", "max_stack"],
            types=["int", "str", "str", "int"],
            comments=["# 物品ID", "# 物品名称", "# 物品类型", "# 最大堆叠数"],
            data_rows=[
                [1, "木材", "RESOURCE", 99],
                [2, "石头", "RESOURCE", 99],
                [3, "斧头", "TOOL", 1],
            ],
        )

        result = read_excel(file_path)

        assert result["table_name"] == "items"
        assert result["fields"] == ["item_id", "name", "type", "max_stack"]
        assert result["types"] == ["int", "str", "str", "int"]
        assert result["comments"] == ["# 物品ID", "# 物品名称", "# 物品类型", "# 最大堆叠数"]
        assert len(result["rows"]) == 3
        assert result["rows"][0] == [1, "木材", "RESOURCE", 99]

    def test_empty_data_rows(self, tmp_path):
        """只有表头没有数据行。"""
        file_path = str(tmp_path / "empty.xlsx")
        _create_test_excel(
            file_path,
            fields=["id", "name"],
            types=["int", "str"],
            comments=["# ID", "# 名称"],
            data_rows=[],
        )

        result = read_excel(file_path)
        assert result["rows"] == []

    def test_file_not_found(self):
        """文件不存在时抛出 FileNotFoundError。"""
        with pytest.raises(FileNotFoundError):
            read_excel("/nonexistent/items.xlsx")

    def test_invalid_type_in_row2(self, tmp_path):
        """行 2 包含不支持的类型时抛出 ValueError。"""
        file_path = str(tmp_path / "bad.xlsx")
        _create_test_excel(
            file_path,
            fields=["id"],
            types=["datetime"],  # 不支持的类型
            comments=["# ID"],
            data_rows=[[1]],
        )

        with pytest.raises(ValueError, match="不支持的字段类型"):
            read_excel(file_path)

    def test_type_conversion(self, tmp_path):
        """int/float/bool 类型自动转换。"""
        file_path = str(tmp_path / "types.xlsx")
        _create_test_excel(
            file_path,
            fields=["id", "value", "active"],
            types=["int", "float", "bool"],
            comments=["# ID", "# 值", "# 启用"],
            data_rows=[[1, 3.14, True]],
        )

        result = read_excel(file_path)
        assert result["rows"][0] == [1, 3.14, True]
```

- [ ] **Step 2: 运行测试确认失败**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_excel_reader.py -v
```

Expected: FAIL — `ModuleNotFoundError: No module named 'tools.table_export.excel_reader'`

- [ ] **Step 3: 实现 Excel 读取器**

创建 `tools/table_export/excel_reader.py`：

```python
# -*- coding: utf-8 -*-
"""Excel 配置表读取器。

读取符合规范的 Excel 文件，解析字段名、类型、注释和数据行。

Excel 格式约定：
- 行 1：字段名（snake_case）
- 行 2：字段类型（int/float/str/bool）
- 行 3：字段描述（注释）
- 行 4 起：数据
"""
import os
from pathlib import Path
from typing import Any, Dict, List

from openpyxl import load_workbook

# 支持的字段类型及对应的 Python 类型转换函数
TYPE_CONVERTERS = {
    "int": int,
    "float": float,
    "str": str,
    "bool": bool,
}

SUPPORTED_TYPES = set(TYPE_CONVERTERS.keys())


def read_excel(file_path: str) -> Dict[str, Any]:
    """读取 Excel 配置文件，返回结构化数据。

    Args:
        file_path: Excel 文件路径。

    Returns:
        dict，包含以下键：
        - table_name: str，表名（文件名去扩展名）
        - fields: list[str]，字段名列表
        - types: list[str]，字段类型列表
        - comments: list[str]，字段描述列表
        - rows: list[list]，数据行列表（已做类型转换）

    Raises:
        FileNotFoundError: 文件不存在。
        ValueError: 字段类型不支持或 Excel 格式错误。
    """
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"Excel 文件不存在: {file_path}")

    wb = load_workbook(file_path, read_only=True, data_only=True)
    ws = wb.active

    # 读取行 1：字段名
    fields = [cell.value for cell in ws[1]]
    if not fields or all(f is None for f in fields):
        raise ValueError(f"{file_path}: 行 1（字段名）为空")

    # 读取行 2：字段类型
    types = [cell.value for cell in ws[2]]
    if len(types) != len(fields):
        raise ValueError(f"{file_path}: 行 2（字段类型）列数与行 1 不匹配")

    # 验证类型
    for i, t in enumerate(types):
        if t is None:
            raise ValueError(f"{file_path}: 行 2, 第 {i+1} 列类型为空")
        if str(t).lower() not in SUPPORTED_TYPES:
            raise ValueError(
                f"{file_path}: 不支持的字段类型 '{t}'，"
                f"支持的类型: {', '.join(sorted(SUPPORTED_TYPES))}"
            )

    # 读取行 3：注释
    comments = [cell.value for cell in ws[3]]

    # 读取行 4 起：数据行
    rows = []
    for row_idx, row in enumerate(ws.iter_rows(min_row=4, values_only=True), start=4):
        # 跳过全空行
        if all(cell is None for cell in row):
            continue
        converted = _convert_row(row, types, file_path, row_idx)
        rows.append(converted)

    wb.close()

    table_name = Path(file_path).stem
    return {
        "table_name": table_name,
        "fields": fields,
        "types": [str(t).lower() for t in types],
        "comments": comments,
        "rows": rows,
    }


def _convert_row(
    row: tuple, types: List[str], file_path: str, row_idx: int
) -> List[Any]:
    """将一行原始数据按类型转换。

    Args:
        row: 原始数据行（tuple）。
        types: 字段类型列表。
        file_path: 文件路径（用于错误信息）。
        row_idx: 行号（用于错误信息）。

    Returns:
        转换后的数据列表。

    Raises:
        ValueError: 类型转换失败。
    """
    result = []
    for col_idx, (value, type_name) in enumerate(zip(row, types)):
        if value is None:
            result.append(None)
            continue
        converter = TYPE_CONVERTERS[type_name.lower()]
        try:
            result.append(converter(value))
        except (ValueError, TypeError) as e:
            raise ValueError(
                f"{file_path}: 第 {row_idx} 行, 第 {col_idx+1} 列: "
                f"无法将 '{value}' 转换为 {type_name}: {e}"
            )
    return result
```

- [ ] **Step 4: 运行测试确认通过**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_excel_reader.py -v
```

Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add tools/table_export/excel_reader.py tests/test_excel_reader.py
git commit -m "feat: add Excel reader for table export"
```

---

### Task 3: 数据校验器 (validators.py)

**Files:**
- Create: `tools/table_export/validators.py`
- Create: `tests/test_validators.py`

- [ ] **Step 1: 编写测试**

创建 `tests/test_validators.py`：

```python
# -*- coding: utf-8 -*-
"""数据校验器测试。"""
import pytest

from tools.table_export.validators import validate_table


class TestValidateTable:
    """validate_table 函数测试。"""

    def test_valid_data(self):
        """合法数据通过校验。"""
        table = {
            "table_name": "items",
            "fields": ["item_id", "name", "max_stack"],
            "types": ["int", "str", "int"],
            "comments": ["# ID", "# 名称", "# 堆叠"],
            "rows": [
                [1, "木材", 99],
                [2, "石头", 99],
            ],
        }
        errors = validate_table(table)
        assert errors == []

    def test_type_mismatch_int(self):
        """int 列包含非数字值。"""
        table = {
            "table_name": "items",
            "fields": ["item_id", "name"],
            "types": ["int", "str"],
            "comments": ["# ID", "# 名称"],
            "rows": [
                [1, "木材"],
                ["abc", "石头"],  # item_id 应为 int
            ],
        }
        errors = validate_table(table)
        assert len(errors) == 1
        assert "第 3 行" in errors[0]
        assert "item_id" in errors[0]

    def test_duplicate_primary_key(self):
        """主键（第一列）重复。"""
        table = {
            "table_name": "items",
            "fields": ["item_id", "name"],
            "types": ["int", "str"],
            "comments": ["# ID", "# 名称"],
            "rows": [
                [1, "木材"],
                [1, "石头"],  # 重复 ID
            ],
        }
        errors = validate_table(table)
        assert len(errors) == 1
        assert "重复" in errors[0]

    def test_empty_row(self):
        """数据行包含空行。"""
        table = {
            "table_name": "items",
            "fields": ["item_id", "name"],
            "types": ["int", "str"],
            "comments": ["# ID", "# 名称"],
            "rows": [
                [1, "木材"],
                [None, None],  # 空行
            ],
        }
        errors = validate_table(table)
        assert len(errors) == 1
        assert "空行" in errors[0]

    def test_bool_validation(self):
        """bool 列只能是 True/False。"""
        table = {
            "table_name": "config",
            "fields": ["id", "active"],
            "types": ["int", "bool"],
            "comments": ["# ID", "# 启用"],
            "rows": [
                [1, True],
                [2, "yes"],  # 不是合法 bool
            ],
        }
        errors = validate_table(table)
        assert len(errors) == 1
        assert "active" in errors[0]

    def test_multiple_errors(self):
        """多个错误同时返回。"""
        table = {
            "table_name": "items",
            "fields": ["item_id", "name"],
            "types": ["int", "str"],
            "comments": ["# ID", "# 名称"],
            "rows": [
                [1, "木材"],
                ["bad", "石头"],
                [1, "斧头"],  # 重复
            ],
        }
        errors = validate_table(table)
        assert len(errors) == 2
```

- [ ] **Step 2: 运行测试确认失败**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_validators.py -v
```

Expected: FAIL — `ModuleNotFoundError`

- [ ] **Step 3: 实现校验器**

创建 `tools/table_export/validators.py`：

```python
# -*- coding: utf-8 -*-
"""配置表数据校验器。

校验规则：
- 类型校验：int 列不能包含非数字值，bool 列只能是 True/False
- 主键唯一：第一列默认为主键列，其值必须唯一
- 非空检查：数据行不能为空行
"""
from typing import Any, Dict, List


def validate_table(table: Dict[str, Any]) -> List[str]:
    """校验配置表数据。

    Args:
        table: read_excel() 返回的结构化数据。

    Returns:
        错误信息列表，空列表表示校验通过。
        每条错误格式："[表名] 第 X 行, 列 'Y': 错误描述"
    """
    errors = []
    table_name = table["table_name"]
    fields = table["fields"]
    types = table["types"]
    rows = table["rows"]

    for row_idx, row in enumerate(rows, start=4):  # 数据从第 4 行开始
        # 检查空行
        if all(cell is None for cell in row):
            errors.append(f"[{table_name}] 第 {row_idx} 行: 数据行不能为空行")
            continue

        for col_idx, (value, field_type) in enumerate(zip(row, types)):
            if value is None:
                continue  # 允许 None（空单元格）
            field_name = fields[col_idx]

            # 类型校验
            type_error = _check_type(value, field_type, table_name, row_idx, field_name)
            if type_error:
                errors.append(type_error)

    # 主键唯一性检查（第一列）
    pk_errors = _check_primary_key_unique(table, rows)
    errors.extend(pk_errors)

    return errors


def _check_type(
    value: Any, field_type: str, table_name: str, row_idx: int, field_name: str
) -> str | None:
    """检查单个值的类型是否匹配。"""
    prefix = f"[{table_name}] 第 {row_idx} 行, 列 '{field_name}'"

    if field_type == "int":
        if not isinstance(value, int) or isinstance(value, bool):
            return f"{prefix}: 期望 int，实际为 {type(value).__name__} '{value}'"
    elif field_type == "float":
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            return f"{prefix}: 期望 float，实际为 {type(value).__name__} '{value}'"
    elif field_type == "str":
        if not isinstance(value, str):
            return f"{prefix}: 期望 str，实际为 {type(value).__name__} '{value}'"
    elif field_type == "bool":
        if not isinstance(value, bool):
            return f"{prefix}: 期望 bool (true/false)，实际为 '{value}'"

    return None


def _check_primary_key_unique(table: Dict[str, Any], rows: List[List]) -> List[str]:
    """检查第一列（主键）的值是否唯一。"""
    errors = []
    table_name = table["table_name"]
    pk_field = table["fields"][0]
    seen = {}

    for row_idx, row in enumerate(rows, start=4):
        pk_value = row[0] if row else None
        if pk_value is None:
            continue
        if pk_value in seen:
            errors.append(
                f"[{table_name}] 第 {row_idx} 行, 列 '{pk_field}': "
                f"值 {pk_value} 重复（首次出现在第 {seen[pk_value]} 行）"
            )
        else:
            seen[pk_value] = row_idx

    return errors
```

- [ ] **Step 4: 运行测试确认通过**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_validators.py -v
```

Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add tools/table_export/validators.py tests/test_validators.py
git commit -m "feat: add table data validators"
```

---

### Task 4: 代码生成器 (code_generator.py)

**Files:**
- Create: `tools/table_export/code_generator.py`
- Create: `tests/test_code_generator.py`

- [ ] **Step 1: 编写测试**

创建 `tests/test_code_generator.py`：

```python
# -*- coding: utf-8 -*-
"""代码生成器测试。"""
import os
import pytest

from tools.table_export.code_generator import generate_python, generate_cpp


SAMPLE_TABLE = {
    "table_name": "items",
    "fields": ["item_id", "name", "type", "max_stack"],
    "types": ["int", "str", "str", "int"],
    "comments": ["# 物品ID", "# 物品名称", "# 物品类型", "# 最大堆叠数"],
    "rows": [
        [1, "木材", "RESOURCE", 99],
        [2, "石头", "RESOURCE", 99],
    ],
}


class TestGeneratePython:
    """Python 代码生成测试。"""

    def test_basic_output(self, tmp_path):
        """生成的 Python 文件内容正确。"""
        output_path = str(tmp_path / "items.py")
        generate_python(SAMPLE_TABLE, output_path)

        with open(output_path, "r", encoding="utf-8") as f:
            content = f.read()

        assert "自动生成" in content
        assert "ITEM_DEFS" in content
        assert '"name": "木材"' in content
        assert '"type": "RESOURCE"' in content
        assert '"max_stack": 99' in content

    def test_contains_source_info(self, tmp_path):
        """生成的文件包含源文件信息。"""
        output_path = str(tmp_path / "items.py")
        generate_python(SAMPLE_TABLE, output_path)

        with open(output_path, "r", encoding="utf-8") as f:
            content = f.read()

        assert "items.xlsx" in content

    def test_atomic_write(self, tmp_path):
        """生成过程是原子写入（先写临时文件再重命名）。"""
        output_path = str(tmp_path / "items.py")
        generate_python(SAMPLE_TABLE, output_path)

        # 临时文件不应存在
        assert not os.path.exists(output_path + ".tmp")
        assert os.path.exists(output_path)


class TestGenerateCpp:
    """C++ 代码生成测试。"""

    def test_basic_output(self, tmp_path):
        """生成的 C++ 文件内容正确。"""
        output_path = str(tmp_path / "items.h")
        generate_cpp(SAMPLE_TABLE, output_path)

        with open(output_path, "r", encoding="utf-8") as f:
            content = f.read()

        assert "自动生成" in content
        assert "#pragma once" in content
        assert "namespace farm" in content
        assert "struct ItemDef" in content
        assert "ITEM_DEFS" in content
        assert '"木材"' in content

    def test_struct_name_convention(self, tmp_path):
        """struct 命名遵循 PascalCase + Def。"""
        output_path = str(tmp_path / "crops.h")
        table = {**SAMPLE_TABLE, "table_name": "crops"}
        generate_cpp(table, output_path)

        with open(output_path, "r", encoding="utf-8") as f:
            content = f.read()

        assert "struct CropDef" in content
        assert "CROP_DEFS" in content

    def test_type_mapping(self, tmp_path):
        """字段类型正确映射到 C++ 类型。"""
        output_path = str(tmp_path / "test.h")
        table = {
            "table_name": "test",
            "fields": ["id", "value", "name", "active"],
            "types": ["int", "float", "str", "bool"],
            "comments": ["# ID", "# 值", "# 名称", "# 启用"],
            "rows": [[1, 3.14, "hello", True]],
        }
        generate_cpp(table, output_path)

        with open(output_path, "r", encoding="utf-8") as f:
            content = f.read()

        assert "int32_t id" in content
        assert "double value" in content
        assert "std::string name" in content
        assert "bool active" in content
```

- [ ] **Step 2: 运行测试确认失败**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_code_generator.py -v
```

Expected: FAIL — `ModuleNotFoundError`

- [ ] **Step 3: 实现代码生成器**

创建 `tools/table_export/code_generator.py`：

```python
# -*- coding: utf-8 -*-
"""配置表代码生成器。

将结构化表数据生成为 Python 和 C++ 代码文件。

命名约定：
- Python 变量名：表名大写蛇形 + _DEFS（如 items → ITEM_DEFS）
- C++ struct 名：表名 PascalCase + Def（如 items → ItemDef）
- C++ 变量名：表名大写蛇形 + _DEFS（如 items → ITEM_DEFS）
"""
import os
from datetime import datetime
from typing import Any, Dict

# Python 类型到 C++ 类型的映射
TYPE_TO_CPP = {
    "int": "int32_t",
    "float": "double",
    "str": "std::string",
    "bool": "bool",
}


def _to_upper_snake(name: str) -> str:
    """将表名转为大写蛇形。"""
    return name.upper()


def _to_pascal_case(name: str) -> str:
    """将表名转为 PascalCase。"""
    return "".join(word.capitalize() for word in name.split("_"))


def generate_python(table: Dict[str, Any], output_path: str) -> None:
    """生成 Python 数据文件。

    Args:
        table: read_excel() 返回的结构化数据。
        output_path: 输出文件路径。
    """
    table_name = table["table_name"]
    fields = table["fields"]
    rows = table["rows"]
    var_name = f"{_to_upper_snake(table_name)}_DEFS"
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    lines = [
        "# 自动生成，请勿手动修改",
        f"# 生成时间：{timestamp}",
        f"# 源文件：tables/{table_name}.xlsx",
        "",
        f"{var_name} = {{",
    ]

    for row in rows:
        pk_value = row[0]
        # 构建 dict 内容
        items = []
        for field, value in zip(fields[1:], row[1:]):
            items.append(f'"{field}": {_format_python_value(value)}')
        dict_content = ", ".join(items)
        lines.append(f"    {pk_value}: {{{dict_content}}},")

    lines.extend(["}", ""])

    _atomic_write(output_path, "\n".join(lines), ".py.tmp")


def generate_cpp(table: Dict[str, Any], output_path: str) -> None:
    """生成 C++ 头文件。

    Args:
        table: read_excel() 返回的结构化数据。
        output_path: 输出文件路径。
    """
    table_name = table["table_name"]
    fields = table["fields"]
    types = table["types"]
    rows = table["rows"]
    struct_name = f"{_to_pascal_case(table_name)}Def"
    var_name = f"{_to_upper_snake(table_name)}_DEFS"
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    # 需要 string 头文件
    has_string = any(t == "str" for t in types)

    lines = [
        "// 自动生成，请勿手动修改",
        f"// 生成时间：{timestamp}",
        f"// 源文件：tables/{table_name}.xlsx",
        "#pragma once",
        "",
        "#include <cstdint>",
    ]

    if has_string:
        lines.append("#include <string>")

    lines.extend([
        "#include <unordered_map>",
        "",
        "namespace farm {",
        "",
        f"struct {struct_name} {{",
    ])

    # struct 字段
    for field, field_type in zip(fields, types):
        cpp_type = TYPE_TO_CPP[field_type]
        lines.append(f"    {cpp_type} {field};")

    lines.extend(["};", ""])

    # 数据 map
    lines.append(f"inline const std::unordered_map<{_get_map_key_type(fields)}, {struct_name}> {var_name} = {{")

    for row in rows:
        pk_value = row[0]
        init_list = ", ".join(_format_cpp_value(v, t) for v, t in zip(row, types))
        lines.append(f"    {{{pk_value}, {{{init_list}}}}},")

    lines.extend(["};", "", "}  // namespace farm", ""])

    _atomic_write(output_path, "\n".join(lines), ".h.tmp")


def _get_map_key_type(fields: list) -> str:
    """根据第一列字段名推断 map key 类型。"""
    # 简单启发式：字段名含 id 则用 int32_t，否则用 std::string
    pk_field = fields[0].lower()
    if "id" in pk_field or pk_field.endswith("_id"):
        return "int32_t"
    return "std::string"


def _format_python_value(value: Any) -> str:
    """格式化 Python 值。"""
    if isinstance(value, bool):
        return "True" if value else "False"
    if isinstance(value, str):
        return f'"{value}"'
    if isinstance(value, (int, float)):
        return str(value)
    if value is None:
        return "None"
    return f'"{value}"'


def _format_cpp_value(value: Any, field_type: str) -> str:
    """格式化 C++ 值。"""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, str):
        return f'"{value}"'
    if isinstance(value, (int, float)):
        if field_type == "float":
            return f"{value}" if "." in str(value) else f"{value}.0"
        return str(value)
    if value is None:
        return "{}"  # 默认初始化
    return f'"{value}"'


def _atomic_write(file_path: str, content: str, temp_suffix: str) -> None:
    """原子写入：先写临时文件，再重命名。"""
    temp_path = file_path + temp_suffix
    with open(temp_path, "w", encoding="utf-8") as f:
        f.write(content)
    os.replace(temp_path, file_path)
```

- [ ] **Step 4: 运行测试确认通过**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_code_generator.py -v
```

Expected: 全部 PASS

- [ ] **Step 5: 提交**

```bash
git add tools/table_export/code_generator.py tests/test_code_generator.py
git commit -m "feat: add Python/C++ code generator"
```

---

### Task 5: 导表引擎入口 (__init__.py)

**Files:**
- Modify: `tools/table_export/__init__.py`
- Create: `tests/test_table_export.py`

- [ ] **Step 1: 编写测试**

创建 `tests/test_table_export.py`：

```python
# -*- coding: utf-8 -*-
"""导表引擎集成测试。"""
import os
import pytest
from openpyxl import Workbook

from tools.table_export import export_table, export_all


def _create_items_excel(path):
    """创建 items.xlsx 测试文件。"""
    wb = Workbook()
    ws = wb.active
    ws.append(["item_id", "name", "type", "max_stack"])
    ws.append(["int", "str", "str", "int"])
    ws.append(["# 物品ID", "# 物品名称", "# 物品类型", "# 最大堆叠数"])
    ws.append([1, "木材", "RESOURCE", 99])
    ws.append([2, "石头", "RESOURCE", 99])
    wb.save(path)


def _create_crops_excel(path):
    """创建 crops.xlsx 测试文件。"""
    wb = Workbook()
    ws = wb.active
    ws.append(["crop_id", "name", "grow_time"])
    ws.append(["int", "str", "int"])
    ws.append(["# 作物ID", "# 作物名称", "# 生长时间"])
    ws.append([1, "小麦", 60])
    wb.save(path)


class TestExportTable:
    """单表导出测试。"""

    def test_export_success(self, tmp_path):
        """正常导出生成 Python 和 C++ 文件。"""
        # 创建目录结构
        tables_dir = tmp_path / "tables"
        tables_dir.mkdir()
        client_data = tmp_path / "scripts" / "client" / "data"
        client_data.mkdir(parents=True)
        server_data = tmp_path / "scripts" / "server" / "data"
        server_data.mkdir(parents=True)

        # 创建 Excel
        excel_path = str(tables_dir / "items.xlsx")
        _create_items_excel(excel_path)

        # 导出
        result = export_table(excel_path, tmp_path)

        assert result["success"] is True
        assert os.path.exists(result["python_path"])
        assert os.path.exists(result["cpp_path"])

        # 验证 Python 内容
        with open(result["python_path"], "r", encoding="utf-8") as f:
            content = f.read()
        assert "ITEM_DEFS" in content

    def test_export_validation_error(self, tmp_path):
        """校验失败时返回错误信息，不生成文件。"""
        tables_dir = tmp_path / "tables"
        tables_dir.mkdir()
        (tmp_path / "scripts" / "client" / "data").mkdir(parents=True)
        (tmp_path / "scripts" / "server" / "data").mkdir(parents=True)

        # 创建有错误的 Excel（重复主键）
        wb = Workbook()
        ws = wb.active
        ws.append(["item_id", "name"])
        ws.append(["int", "str"])
        ws.append(["# ID", "# 名称"])
        ws.append([1, "木材"])
        ws.append([1, "石头"])  # 重复
        excel_path = str(tables_dir / "bad.xlsx")
        wb.save(excel_path)

        result = export_table(excel_path, tmp_path)

        assert result["success"] is False
        assert len(result["errors"]) > 0


class TestExportAll:
    """全表导出测试。"""

    def test_export_all_success(self, tmp_path):
        """导出目录下所有 Excel 文件。"""
        tables_dir = tmp_path / "tables"
        tables_dir.mkdir()
        (tmp_path / "scripts" / "client" / "data").mkdir(parents=True)
        (tmp_path / "scripts" / "server" / "data").mkdir(parents=True)

        _create_items_excel(str(tables_dir / "items.xlsx"))
        _create_crops_excel(str(tables_dir / "crops.xlsx"))

        result = export_all(str(tables_dir), tmp_path)

        assert result["total"] == 2
        assert result["success_count"] == 2
        assert result["fail_count"] == 0

    def test_export_all_mixed(self, tmp_path):
        """部分成功部分失败。"""
        tables_dir = tmp_path / "tables"
        tables_dir.mkdir()
        (tmp_path / "scripts" / "client" / "data").mkdir(parents=True)
        (tmp_path / "scripts" / "server" / "data").mkdir(parents=True)

        # 好的表
        _create_items_excel(str(tables_dir / "items.xlsx"))

        # 坏的表（重复主键）
        wb = Workbook()
        ws = wb.active
        ws.append(["id", "name"])
        ws.append(["int", "str"])
        ws.append(["# ID", "# 名称"])
        ws.append([1, "a"])
        ws.append([1, "b"])
        wb.save(str(tables_dir / "bad.xlsx"))

        result = export_all(str(tables_dir), tmp_path)

        assert result["total"] == 2
        assert result["success_count"] == 1
        assert result["fail_count"] == 1
```

- [ ] **Step 2: 运行测试确认失败**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_table_export.py -v
```

Expected: FAIL — `ImportError: cannot import name 'export_table'`

- [ ] **Step 3: 实现导表引擎入口**

修改 `tools/table_export/__init__.py`：

```python
# -*- coding: utf-8 -*-
"""表导出引擎 - 从 Excel 配置表生成 Python 和 C++ 代码。

用法：
    # Python API
    from tools.table_export import export_table, export_all

    # 命令行
    python -m tools.table_export tables/items.xlsx        # 单表导出
    python -m tools.table_export --all tables/             # 全表导出
"""
import glob
import os
import sys
from pathlib import Path
from typing import Any, Dict, List

from .excel_reader import read_excel
from .validators import validate_table
from .code_generator import generate_python, generate_cpp


def export_table(excel_path: str, project_root: str | Path) -> Dict[str, Any]:
    """导出单个 Excel 配置表。

    Args:
        excel_path: Excel 文件路径。
        project_root: 项目根目录。

    Returns:
        dict，包含：
        - success: bool
        - table_name: str
        - python_path: str（生成的 Python 文件路径，失败时为空字符串）
        - cpp_path: str（生成的 C++ 文件路径，失败时为空字符串）
        - errors: list[str]（错误信息列表）
    """
    project_root = Path(project_root)
    table_name = Path(excel_path).stem

    result = {
        "success": False,
        "table_name": table_name,
        "python_path": "",
        "cpp_path": "",
        "errors": [],
    }

    # 读取
    try:
        table = read_excel(excel_path)
    except (FileNotFoundError, ValueError) as e:
        result["errors"].append(str(e))
        return result

    # 校验
    errors = validate_table(table)
    if errors:
        result["errors"] = errors
        return result

    # 生成 Python
    python_dir = project_root / "scripts" / "client" / "data"
    python_dir.mkdir(parents=True, exist_ok=True)
    python_path = python_dir / f"{table_name}.py"
    generate_python(table, str(python_path))
    result["python_path"] = str(python_path)

    # 生成 C++
    cpp_dir = project_root / "scripts" / "server" / "data"
    cpp_dir.mkdir(parents=True, exist_ok=True)
    cpp_path = cpp_dir / f"{table_name}.h"
    generate_cpp(table, str(cpp_path))
    result["cpp_path"] = str(cpp_path)

    result["success"] = True
    return result


def export_all(tables_dir: str, project_root: str | Path) -> Dict[str, Any]:
    """导出目录下所有 Excel 配置表。

    Args:
        tables_dir: Excel 文件目录路径。
        project_root: 项目根目录。

    Returns:
        dict，包含：
        - total: int（总表数）
        - success_count: int
        - fail_count: int
        - results: list[dict]（每张表的导出结果）
    """
    excel_files = sorted(glob.glob(os.path.join(tables_dir, "*.xlsx")))
    results = []

    for excel_path in excel_files:
        result = export_table(excel_path, project_root)
        results.append(result)

    success_count = sum(1 for r in results if r["success"])
    fail_count = len(results) - success_count

    return {
        "total": len(results),
        "success_count": success_count,
        "fail_count": fail_count,
        "results": results,
    }


def main():
    """命令行入口。"""
    if len(sys.argv) < 2:
        print("用法:")
        print("  python -m tools.table_export <excel_file>     # 单表导出")
        print("  python -m tools.table_export --all <dir>      # 全表导出")
        sys.exit(1)

    project_root = Path(__file__).parent.parent.parent

    if sys.argv[1] == "--all":
        if len(sys.argv) < 3:
            print("错误: --all 需要指定目录路径")
            sys.exit(1)
        tables_dir = sys.argv[2]
        result = export_all(tables_dir, project_root)
        print(f"导出完成: 成功 {result['success_count']}, 失败 {result['fail_count']}")
        for r in result["results"]:
            status = "✓" if r["success"] else "✗"
            print(f"  {status} {r['table_name']}")
            if r["errors"]:
                for err in r["errors"]:
                    print(f"    - {err}")
        sys.exit(0 if result["fail_count"] == 0 else 1)
    else:
        excel_path = sys.argv[1]
        result = export_table(excel_path, project_root)
        if result["success"]:
            print(f"导出成功: {result['table_name']}")
            print(f"  Python: {result['python_path']}")
            print(f"  C++:    {result['cpp_path']}")
        else:
            print(f"导出失败: {result['table_name']}")
            for err in result["errors"]:
                print(f"  - {err}")
            sys.exit(1)


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: 运行测试确认通过**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_table_export.py -v
```

Expected: 全部 PASS

- [ ] **Step 5: 运行全部导表引擎测试**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_excel_reader.py tests/test_validators.py tests/test_code_generator.py tests/test_table_export.py -v
```

Expected: 全部 PASS

- [ ] **Step 6: 提交**

```bash
git add tools/table_export/__init__.py tests/test_table_export.py
git commit -m "feat: add table export engine with CLI support"
```

---

### Task 6: 示例 Excel 表

**Files:**
- Create: `tables/items.xlsx`

- [ ] **Step 1: 创建 items.xlsx 示例表**

运行以下 Python 脚本创建示例 Excel：

```python
# tools/create_sample_tables.py
"""创建示例配置表。"""
from openpyxl import Workbook
from pathlib import Path

def create_items():
    tables_dir = Path(__file__).parent.parent / "tables"
    tables_dir.mkdir(exist_ok=True)

    wb = Workbook()
    ws = wb.active

    # 行 1: 字段名
    ws.append(["item_id", "name", "type", "max_stack"])
    # 行 2: 字段类型
    ws.append(["int", "str", "str", "int"])
    # 行 3: 字段描述
    ws.append(["# 物品ID", "# 物品名称", "# 物品类型", "# 最大堆叠数"])
    # 行 4+: 数据
    ws.append([1, "木材", "RESOURCE", 99])
    ws.append([2, "石头", "RESOURCE", 99])
    ws.append([3, "斧头", "TOOL", 1])
    ws.append([4, "锄头", "TOOL", 1])
    ws.append([5, "种子", "SEED", 99])
    ws.append([6, "面包", "FOOD", 20])
    ws.append([7, "作物", "RESOURCE", 99])

    wb.save(tables_dir / "items.xlsx")
    print("创建 tables/items.xlsx")

if __name__ == "__main__":
    create_items()
```

运行：

```bash
cd D:/mb_workspace/farm_demo
python tools/create_sample_tables.py
```

Expected: 输出 `创建 tables/items.xlsx`

- [ ] **Step 2: 用导表引擎测试导出**

```bash
cd D:/mb_workspace/farm_demo
python -m tools.table_export tables/items.xlsx
```

Expected: 输出 `导出成功: items`，并生成 `scripts/client/data/items.py` 和 `scripts/server/data/items.h`

- [ ] **Step 3: 验证生成的文件**

```bash
cat scripts/client/data/items.py
cat scripts/server/data/items.h
```

Expected: 两个文件内容正确，与现有 item_registry.py 数据一致

- [ ] **Step 4: 提交**

```bash
git add tables/items.xlsx tools/create_sample_tables.py scripts/client/data/items.py scripts/server/data/items.h
git commit -m "feat: add sample items.xlsx and generated code"
```

---

### Task 7: 编辑器侧边栏 (sidebar.py)

**Files:**
- Create: `tools/config_editor/sidebar.py`

- [ ] **Step 1: 实现侧边栏组件**

创建 `tools/config_editor/sidebar.py`：

```python
# -*- coding: utf-8 -*-
"""配置编辑器侧边栏组件。

显示 tables/ 目录下的 Excel 文件列表，支持点击切换和右键菜单。
"""
import os
import logging
from typing import Callable, Optional

import customtkinter as ctk

logger = logging.getLogger(__name__)


class Sidebar(ctk.CTkFrame):
    """左侧文件列表侧边栏。"""

    def __init__(self, master, on_file_select: Callable[[str], None], **kwargs):
        """初始化侧边栏。

        Args:
            master: 父组件。
            on_file_select: 文件选中回调，参数为文件完整路径。
        """
        super().__init__(master, width=200, **kwargs)
        self.pack_propagate(False)

        self._on_file_select = on_file_select
        self._tables_dir: Optional[str] = None
        self._selected_btn: Optional[ctk.CTkButton] = None

        # 标题
        self._title_label = ctk.CTkLabel(
            self, text="配置表", font=ctk.CTkFont(size=14, weight="bold")
        )
        self._title_label.pack(padx=10, pady=(10, 5), anchor="w")

        # 文件列表容器（可滚动）
        self._list_frame = ctk.CTkScrollableFrame(self)
        self._list_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # 文件按钮列表
        self._file_buttons: list[ctk.CTkButton] = []

    def set_tables_dir(self, tables_dir: str) -> None:
        """设置 tables 目录路径并刷新文件列表。"""
        self._tables_dir = tables_dir
        self._refresh_file_list()

    def refresh(self) -> None:
        """刷新文件列表。"""
        self._refresh_file_list()

    def _refresh_file_list(self) -> None:
        """重新扫描目录并更新文件列表。"""
        # 清除旧按钮
        for btn in self._file_buttons:
            btn.destroy()
        self._file_buttons.clear()

        if not self._tables_dir or not os.path.isdir(self._tables_dir):
            return

        # 扫描 .xlsx 文件
        xlsx_files = sorted(
            f for f in os.listdir(self._tables_dir)
            if f.endswith(".xlsx") and not f.startswith("~$")  # 排除临时文件
        )

        for filename in xlsx_files:
            btn = ctk.CTkButton(
                self._list_frame,
                text=f"📦 {filename}",
                anchor="w",
                height=32,
                font=ctk.CTkFont(size=13),
                fg_color="transparent",
                text_color=("gray10", "gray90"),
                hover_color=("gray75", "gray25"),
                command=lambda p=os.path.join(self._tables_dir, filename): self._select_file(p),
            )
            btn.pack(fill=tk.X, padx=2, pady=1)
            self._file_buttons.append(btn)

    def _select_file(self, file_path: str) -> None:
        """选中文件并高亮按钮。"""
        # 取消旧选中
        if self._selected_btn:
            self._selected_btn.configure(fg_color="transparent")

        # 高亮新选中
        for btn in self._file_buttons:
            full_path = os.path.join(self._tables_dir, btn.cget("text").replace("📦 ", ""))
            if full_path == file_path:
                btn.configure(fg_color=("gray75", "gray25"))
                self._selected_btn = btn
                break

        self._on_file_select(file_path)
```

注意：需要在文件顶部添加 `import tkinter as tk`（因为使用了 `tk.BOTH`）。修正：

```python
import tkinter as tk
import customtkinter as ctk
```

- [ ] **Step 2: 验证语法**

```bash
cd D:/mb_workspace/farm_demo
python -c "from tools.config_editor.sidebar import Sidebar; print('OK')"
```

Expected: 输出 `OK`

- [ ] **Step 3: 提交**

```bash
git add tools/config_editor/sidebar.py
git commit -m "feat: add sidebar component for config editor"
```

---

### Task 8: 表格编辑器 (table_editor.py)

**Files:**
- Create: `tools/config_editor/table_editor.py`

- [ ] **Step 1: 实现表格编辑器组件**

创建 `tools/config_editor/table_editor.py`：

```python
# -*- coding: utf-8 -*-
"""配置编辑器表格编辑组件。

以可编辑表格形式展示 Excel 内容，支持实时类型校验。
行 1（字段名）、行 2（字段类型）、行 3（字段描述）冻结在顶部。
"""
import logging
import tkinter as tk
from tkinter import ttk
from typing import Any, Callable, Dict, List, Optional

import customtkinter as ctk

logger = logging.getLogger(__name__)

# 类型校验规则
TYPE_VALIDATORS = {
    "int": lambda v: _try_int(v),
    "float": lambda v: _try_float(v),
    "str": lambda v: (True, str(v)),
    "bool": lambda v: _try_bool(v),
}


def _try_int(value):
    """尝试转换为 int。"""
    if value in ("", None):
        return True, None
    try:
        return True, int(value)
    except (ValueError, TypeError):
        return False, f"'{value}' 不是有效的整数"


def _try_float(value):
    """尝试转换为 float。"""
    if value in ("", None):
        return True, None
    try:
        return True, float(value)
    except (ValueError, TypeError):
        return False, f"'{value}' 不是有效的浮点数"


def _try_bool(value):
    """尝试转换为 bool。"""
    if value in ("", None):
        return True, None
    lower = str(value).lower()
    if lower in ("true", "1", "yes"):
        return True, True
    if lower in ("false", "0", "no"):
        return True, False
    return False, f"'{value}' 不是有效的布尔值 (true/false)"


class TableEditor(ctk.CTkFrame):
    """表格编辑器组件。"""

    def __init__(self, master, on_modified: Callable[[], None] = None, **kwargs):
        """初始化表格编辑器。

        Args:
            master: 父组件。
            on_modified: 数据修改回调。
        """
        super().__init__(master, **kwargs)

        self._on_modified = on_modified
        self._table_data: Optional[Dict[str, Any]] = None
        self._is_modified = False

        # 工具栏（插入行、删除行）
        self._toolbar = ctk.CTkFrame(self, height=36)
        self._toolbar.pack(fill=tk.X, padx=5, pady=(5, 0))

        self._insert_btn = ctk.CTkButton(
            self._toolbar, text="➕ 插入行", width=80, height=28,
            font=ctk.CTkFont(size=12), command=self._insert_row
        )
        self._insert_btn.pack(side=tk.LEFT, padx=5, pady=4)

        self._delete_btn = ctk.CTkButton(
            self._toolbar, text="🗑 删除行", width=80, height=28,
            font=ctk.CTkFont(size=12), fg_color="#d9534f", hover_color="#c9302c",
            command=self._delete_row
        )
        self._delete_btn.pack(side=tk.LEFT, padx=5, pady=4)

        # 表格容器
        self._table_frame = ctk.CTkFrame(self)
        self._table_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # 使用 tkinter Treeview 实现表格（CTk 没有原生表格组件）
        style = ttk.Style()
        style.theme_use("default")
        style.configure(
            "Custom.Treeview",
            background="#2b2b2b",
            foreground="white",
            fieldbackground="#2b2b2b",
            borderwidth=0,
            font=("Consolas", 11),
        )
        style.configure(
            "Custom.Treeview.Heading",
            background="#1f538d",
            foreground="white",
            font=("Consolas", 11, "bold"),
        )
        style.map("Custom.Treeview", background=[("selected", "#264f78")])

        self._tree = ttk.Treeview(
            self._table_frame, style="Custom.Treeview", show="headings"
        )

        # 滚动条
        vsb = ttk.Scrollbar(self._table_frame, orient="vertical", command=self._tree.yview)
        hsb = ttk.Scrollbar(self._table_frame, orient="horizontal", command=self._tree.xview)
        self._tree.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)

        self._tree.grid(row=0, column=0, sticky="nsew")
        vsb.grid(row=0, column=1, sticky="ns")
        hsb.grid(row=1, column=0, sticky="ew")
        self._table_frame.grid_rowconfigure(0, weight=1)
        self._table_frame.grid_columnconfigure(0, weight=1)

        # 绑定双击编辑
        self._tree.bind("<Double-1>", self._on_double_click)

        # 当前编辑状态
        self._editing_item = None
        self._editing_col = None

    def load_table(self, table_data: Dict[str, Any]) -> None:
        """加载表数据到编辑器。"""
        self._table_data = table_data
        self._is_modified = False
        self._refresh_display()

    def get_table_data(self) -> Optional[Dict[str, Any]]:
        """获取当前表数据。"""
        return self._table_data

    def is_modified(self) -> bool:
        """返回数据是否被修改。"""
        return self._is_modified

    def _refresh_display(self) -> None:
        """刷新表格显示。"""
        if not self._table_data:
            return

        # 清除旧数据
        self._tree.delete(*self._tree.get_children())

        # 设置列
        fields = self._table_data["fields"]
        self._tree["columns"] = list(range(len(fields)))
        for i, field in enumerate(fields):
            self._tree.heading(i, text=field)
            self._tree.column(i, width=100, minwidth=60)

        # 行 2: 类型（固定显示为第一行，灰色样式）
        types = self._table_data["types"]
        self._tree.insert("", "end", values=types, tags=("type_row",))

        # 行 3: 注释
        comments = self._table_data["comments"]
        self._tree.insert("", "end", values=comments, tags=("comment_row",))

        # 数据行
        for row in self._table_data["rows"]:
            display_row = ["" if v is None else str(v) for v in row]
            self._tree.insert("", "end", values=display_row)

        # 行样式
        self._tree.tag_configure("type_row", foreground="#888888", font=("Consolas", 10, "italic"))
        self._tree.tag_configure("comment_row", foreground="#569cd6", font=("Consolas", 10))

    def _on_double_click(self, event) -> None:
        """双击单元格进入编辑模式。"""
        item = self._tree.identify_row(event.y)
        column = self._tree.identify_column(event.x)

        if not item or not column:
            return

        # 获取列索引（#0, #1, #2...）
        col_idx = int(column.replace("#", "")) - 1
        if col_idx < 0:
            return

        # 获取当前值
        values = self._tree.item(item, "values")
        if not values:
            return
        current_value = values[col_idx]

        # 获取单元格位置
        bbox = self._tree.bbox(item, column)
        if not bbox:
            return

        x, y, width, height = bbox

        # 创建编辑框
        entry = tk.Entry(self._tree, font=("Consolas", 11))
        entry.insert(0, current_value)
        entry.select_range(0, tk.END)
        entry.focus()

        entry.place(x=x, y=y, width=width, height=height)

        def on_confirm(e=None):
            new_value = entry.get()
            entry.destroy()
            self._apply_edit(item, col_idx, new_value)

        def on_cancel(e=None):
            entry.destroy()

        entry.bind("<Return>", on_confirm)
        entry.bind("<Escape>", on_cancel)
        entry.bind("<FocusOut>", on_confirm)

    def _apply_edit(self, item: str, col_idx: int, new_value: str) -> None:
        """应用单元格编辑。"""
        values = list(self._tree.item(item, "values"))
        old_value = values[col_idx]

        if new_value == old_value:
            return

        # 类型校验（仅对数据行，跳过类型行和注释行）
        all_items = self._tree.get_children()
        item_idx = all_items.index(item)
        if item_idx < 2:  # 前两行是类型和注释
            values[col_idx] = new_value
            self._tree.item(item, values=values)
            # 更新内部数据
            if item_idx == 0:
                self._table_data["types"][col_idx] = new_value
            elif item_idx == 1:
                self._table_data["comments"][col_idx] = new_value
            self._mark_modified()
            return

        # 数据行：类型校验
        field_type = self._table_data["types"][col_idx]
        validator = TYPE_VALIDATORS.get(field_type)
        if validator:
            ok, result = validator(new_value)
            if not ok:
                # 校验失败，显示红色边框提示
                logger.warning(f"类型校验失败: {result}")
                # 仍然允许输入，但标记为错误
                pass

        values[col_idx] = new_value
        self._tree.item(item, values=values)

        # 更新内部数据
        data_row_idx = item_idx - 2  # 减去类型行和注释行
        if data_row_idx < len(self._table_data["rows"]):
            if col_idx < len(self._table_data["rows"][data_row_idx]):
                # 尝试类型转换
                if field_type == "int" and new_value:
                    try:
                        self._table_data["rows"][data_row_idx][col_idx] = int(new_value)
                    except ValueError:
                        self._table_data["rows"][data_row_idx][col_idx] = new_value
                elif field_type == "float" and new_value:
                    try:
                        self._table_data["rows"][data_row_idx][col_idx] = float(new_value)
                    except ValueError:
                        self._table_data["rows"][data_row_idx][col_idx] = new_value
                elif field_type == "bool" and new_value:
                    lower = new_value.lower()
                    self._table_data["rows"][data_row_idx][col_idx] = lower in ("true", "1", "yes")
                else:
                    self._table_data["rows"][data_row_idx][col_idx] = new_value

        self._mark_modified()

    def _insert_row(self) -> None:
        """在末尾插入空数据行。"""
        if not self._table_data:
            return
        num_cols = len(self._table_data["fields"])
        new_row = [None] * num_cols
        self._table_data["rows"].append(new_row)

        display_row = [""] * num_cols
        self._tree.insert("", "end", values=display_row)
        self._mark_modified()

    def _delete_row(self) -> None:
        """删除选中的数据行。"""
        if not self._table_data:
            return
        selected = self._tree.selection()
        if not selected:
            return

        all_items = self._tree.get_children()
        for item in selected:
            item_idx = all_items.index(item)
            if item_idx < 2:  # 不能删除类型行和注释行
                continue
            data_row_idx = item_idx - 2
            if 0 <= data_row_idx < len(self._table_data["rows"]):
                self._table_data["rows"].pop(data_row_idx)
            self._tree.delete(item)
        self._mark_modified()

    def _mark_modified(self) -> None:
        """标记数据已修改。"""
        self._is_modified = True
        if self._on_modified:
            self._on_modified()
```

- [ ] **Step 2: 验证语法**

```bash
cd D:/mb_workspace/farm_demo
python -c "from tools.config_editor.table_editor import TableEditor; print('OK')"
```

Expected: 输出 `OK`

- [ ] **Step 3: 提交**

```bash
git add tools/config_editor/table_editor.py
git commit -m "feat: add table editor component with inline editing"
```

---

### Task 9: 导表触发器 (export_runner.py)

**Files:**
- Create: `tools/config_editor/export_runner.py`

- [ ] **Step 1: 实现导表触发器**

创建 `tools/config_editor/export_runner.py`：

```python
# -*- coding: utf-8 -*-
"""配置编辑器导表触发模块。

在 GUI 中触发导表操作，显示结果对话框。
"""
import logging
import threading
import tkinter as tk
from typing import Any, Callable, Dict, Optional

import customtkinter as ctk

from tools.table_export import export_table, export_all

logger = logging.getLogger(__name__)


class ExportRunner:
    """导表触发器。"""

    def __init__(self, project_root: str):
        """初始化。

        Args:
            project_root: 项目根目录路径。
        """
        self._project_root = project_root

    def run_single(self, excel_path: str, parent: ctk.CTkFrame, callback: Callable[[Dict], None]) -> None:
        """在后台线程执行单表导出。

        Args:
            excel_path: Excel 文件路径。
            parent: 父窗口（用于显示结果对话框）。
            callback: 完成回调，参数为导出结果 dict。
        """
        def _do_export():
            result = export_table(excel_path, self._project_root)
            parent.after(0, lambda: self._show_result(result, parent))
            parent.after(0, lambda: callback(result))

        thread = threading.Thread(target=_do_export, daemon=True)
        thread.start()

    def run_all(self, tables_dir: str, parent: ctk.CTkFrame, callback: Callable[[Dict], None]) -> None:
        """在后台线程执行全表导出。

        Args:
            tables_dir: tables 目录路径。
            parent: 父窗口（用于显示结果对话框）。
            callback: 完成回调，参数为导出结果 dict。
        """
        def _do_export():
            result = export_all(tables_dir, self._project_root)
            parent.after(0, lambda: self._show_all_result(result, parent))
            parent.after(0, lambda: callback(result))

        thread = threading.Thread(target=_do_export, daemon=True)
        thread.start()

    def _show_result(self, result: Dict[str, Any], parent: ctk.CTkFrame) -> None:
        """显示单表导出结果对话框。"""
        if result["success"]:
            ctk.CTkMessagebox(
                title="导出成功",
                message=f"表 '{result['table_name']}' 导出成功！\n\n"
                        f"Python: {result['python_path']}\n"
                        f"C++: {result['cpp_path']}",
                icon="check",
                parent=parent,
            )
        else:
            errors_text = "\n".join(result["errors"])
            ctk.CTkMessagebox(
                title="导出失败",
                message=f"表 '{result['table_name']}' 导出失败：\n\n{errors_text}",
                icon="cancel",
                parent=parent,
            )

    def _show_all_result(self, result: Dict[str, Any], parent: ctk.CTkFrame) -> None:
        """显示全表导出结果对话框。"""
        total = result["total"]
        success = result["success_count"]
        fail = result["fail_count"]

        details = []
        for r in result["results"]:
            status = "✓" if r["success"] else "✗"
            details.append(f"{status} {r['table_name']}")
            if r["errors"]:
                for err in r["errors"]:
                    details.append(f"  - {err}")

        details_text = "\n".join(details)
        message = f"导出完成！\n\n成功: {success}, 失败: {fail}, 总计: {total}\n\n{details_text}"

        icon = "check" if fail == 0 else "warning"
        ctk.CTkMessagebox(
            title="导出结果",
            message=message,
            icon=icon,
            parent=parent,
        )
```

注意：`CTkMessagebox` 是 customtkinter 的消息框组件，需要确认是否可用。如果不可用，使用 tkinter 的 `messagebox` 替代。修正版本：

```python
# -*- coding: utf-8 -*-
"""配置编辑器导表触发模块。

在 GUI 中触发导表操作，显示结果对话框。
"""
import logging
import threading
import tkinter as tk
from tkinter import messagebox
from typing import Any, Callable, Dict

import customtkinter as ctk

from tools.table_export import export_table, export_all

logger = logging.getLogger(__name__)


class ExportRunner:
    """导表触发器。"""

    def __init__(self, project_root: str):
        """初始化。

        Args:
            project_root: 项目根目录路径。
        """
        self._project_root = project_root

    def run_single(self, excel_path: str, parent: ctk.CTkFrame, callback: Callable[[Dict], None]) -> None:
        """在后台线程执行单表导出。

        Args:
            excel_path: Excel 文件路径。
            parent: 父窗口（用于显示结果对话框）。
            callback: 完成回调，参数为导出结果 dict。
        """
        def _do_export():
            result = export_table(excel_path, self._project_root)
            parent.after(0, lambda: self._show_single_result(result))
            parent.after(0, lambda: callback(result))

        thread = threading.Thread(target=_do_export, daemon=True)
        thread.start()

    def run_all(self, tables_dir: str, parent: ctk.CTkFrame, callback: Callable[[Dict], None]) -> None:
        """在后台线程执行全表导出。

        Args:
            tables_dir: tables 目录路径。
            parent: 父窗口（用于显示结果对话框）。
            callback: 完成回调，参数为导出结果 dict。
        """
        def _do_export():
            result = export_all(tables_dir, self._project_root)
            parent.after(0, lambda: self._show_all_result(result))
            parent.after(0, lambda: callback(result))

        thread = threading.Thread(target=_do_export, daemon=True)
        thread.start()

    def _show_single_result(self, result: Dict[str, Any]) -> None:
        """显示单表导出结果对话框。"""
        if result["success"]:
            messagebox.showinfo(
                "导出成功",
                f"表 '{result['table_name']}' 导出成功！\n\n"
                f"Python: {result['python_path']}\n"
                f"C++: {result['cpp_path']}",
            )
        else:
            errors_text = "\n".join(result["errors"])
            messagebox.showerror(
                "导出失败",
                f"表 '{result['table_name']}' 导出失败：\n\n{errors_text}",
            )

    def _show_all_result(self, result: Dict[str, Any]) -> None:
        """显示全表导出结果对话框。"""
        total = result["total"]
        success = result["success_count"]
        fail = result["fail_count"]

        details = []
        for r in result["results"]:
            status = "✓" if r["success"] else "✗"
            details.append(f"{status} {r['table_name']}")
            if r["errors"]:
                for err in r["errors"]:
                    details.append(f"  - {err}")

        details_text = "\n".join(details)
        message = f"导出完成！\n\n成功: {success}, 失败: {fail}, 总计: {total}\n\n{details_text}"

        if fail == 0:
            messagebox.showinfo("导出结果", message)
        else:
            messagebox.showwarning("导出结果", message)
```

- [ ] **Step 2: 验证语法**

```bash
cd D:/mb_workspace/farm_demo
python -c "from tools.config_editor.export_runner import ExportRunner; print('OK')"
```

Expected: 输出 `OK`

- [ ] **Step 3: 提交**

```bash
git add tools/config_editor/export_runner.py
git commit -m "feat: add export runner for config editor GUI"
```

---

### Task 10: 编辑器主窗口 (app.py)

**Files:**
- Create: `tools/config_editor/app.py`

- [ ] **Step 1: 实现主窗口**

创建 `tools/config_editor/app.py`：

```python
# -*- coding: utf-8 -*-
"""配置编辑器主窗口。

工具栏 + 左侧文件列表 + 右侧表格编辑区 + 状态栏。
"""
import logging
import os
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog
from pathlib import Path
from typing import Optional

import customtkinter as ctk

from .sidebar import Sidebar
from .table_editor import TableEditor
from .export_runner import ExportRunner
from tools.table_export import read_excel

logger = logging.getLogger(__name__)


class ConfigEditorApp:
    """配置编辑器主窗口。"""

    def __init__(self, project_root: str):
        """初始化编辑器。

        Args:
            project_root: 项目根目录路径。
        """
        self._project_root = project_root
        self._tables_dir: Optional[str] = None
        self._current_file: Optional[str] = None

        # 配置 CustomTkinter 外观
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")

        # 创建主窗口
        self.root = ctk.CTk()
        self.root.title("配置编辑器")
        self.root.geometry("1200x700")
        self.root.minsize(800, 500)

        # 导表触发器
        self._export_runner = ExportRunner(project_root)

        # 构建 UI
        self._build_toolbar()
        self._build_main_area()
        self._build_status_bar()

        # 默认尝试加载 tables/ 目录
        default_tables = os.path.join(project_root, "tables")
        if os.path.isdir(default_tables):
            self._tables_dir = default_tables
            self._sidebar.set_tables_dir(default_tables)

    def run(self) -> None:
        """启动 GUI 主循环。"""
        self.root.mainloop()

    # ---- UI 构建 ----

    def _build_toolbar(self) -> None:
        """构建工具栏。"""
        toolbar = ctk.CTkFrame(self.root, height=50)
        toolbar.pack(fill=tk.X, padx=10, pady=(10, 5))
        toolbar.pack_propagate(False)

        buttons = [
            ("打开目录", self._on_open_dir, "#0e639c", "#0a4a6e"),
            ("新建表", self._on_new_table, "#16825d", "#0e5c3f"),
            ("保存", self._on_save, "#0e639c", "#0a4a6e"),
            ("导出当前表", self._on_export_current, "#c2432c", "#8b2e1e"),
            ("导出全部", self._on_export_all, "#c2432c", "#8b2e1e"),
        ]

        for text, command, fg_color, hover_color in buttons:
            btn = ctk.CTkButton(
                toolbar, text=text, width=100, height=35,
                font=ctk.CTkFont(size=13),
                fg_color=fg_color, hover_color=hover_color,
                command=command,
            )
            btn.pack(side=tk.LEFT, padx=5, pady=7)

    def _build_main_area(self) -> None:
        """构建主内容区（左侧边栏 + 右侧编辑区）。"""
        main_frame = ctk.CTkFrame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # 左侧边栏
        self._sidebar = Sidebar(main_frame, on_file_select=self._on_file_select)
        self._sidebar.pack(side=tk.LEFT, fill=tk.Y)

        # 右侧编辑区
        self._table_editor = TableEditor(main_frame, on_modified=self._on_data_modified)
        self._table_editor.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(5, 0))

    def _build_status_bar(self) -> None:
        """构建底部状态栏。"""
        status_bar = ctk.CTkFrame(self.root, height=30)
        status_bar.pack(fill=tk.X, padx=10, pady=(5, 10))
        status_bar.pack_propagate(False)

        self._status_label = ctk.CTkLabel(
            status_bar, text="就绪", font=ctk.CTkFont(size=11), text_color="gray"
        )
        self._status_label.pack(side=tk.LEFT, padx=10)

        self._info_label = ctk.CTkLabel(
            status_bar, text="", font=ctk.CTkFont(size=11), text_color="gray"
        )
        self._info_label.pack(side=tk.RIGHT, padx=10)

    # ---- 事件处理 ----

    def _on_open_dir(self) -> None:
        """打开目录对话框。"""
        directory = filedialog.askdirectory(
            title="选择 tables 目录",
            initialdir=self._tables_dir or self._project_root,
        )
        if directory:
            self._tables_dir = directory
            self._sidebar.set_tables_dir(directory)
            self._update_status(f"已打开目录: {directory}")

    def _on_new_table(self) -> None:
        """创建新表。"""
        if not self._tables_dir:
            messagebox.showwarning("提示", "请先打开 tables 目录")
            return

        name = simpledialog.askstring("新建表", "请输入表名（英文，snake_case）：")
        if not name:
            return

        # 创建空白 Excel
        from openpyxl import Workbook

        wb = Workbook()
        ws = wb.active
        ws.append(["id", "name"])
        ws.append(["int", "str"])
        ws.append(["# ID", "# 名称"])

        file_path = os.path.join(self._tables_dir, f"{name}.xlsx")
        if os.path.exists(file_path):
            messagebox.showwarning("提示", f"文件 {name}.xlsx 已存在")
            return

        wb.save(file_path)
        self._sidebar.refresh()
        self._update_status(f"已创建: {name}.xlsx")

    def _on_save(self) -> None:
        """保存当前编辑的表。"""
        if not self._current_file or not self._table_editor.is_modified():
            return

        table_data = self._table_editor.get_table_data()
        if not table_data:
            return

        try:
            self._save_excel(self._current_file, table_data)
            self._update_status(f"已保存: {os.path.basename(self._current_file)}")
        except Exception as e:
            messagebox.showerror("保存失败", f"保存失败: {e}")

    def _on_export_current(self) -> None:
        """导出当前表。"""
        if not self._current_file:
            messagebox.showwarning("提示", "请先选择一张表")
            return

        # 先保存
        self._on_save()

        self._update_status("正在导出...")
        self._export_runner.run_single(
            self._current_file, self._table_editor,
            callback=lambda r: self._update_status(
                f"导出{'成功' if r['success'] else '失败'}: {r['table_name']}"
            ),
        )

    def _on_export_all(self) -> None:
        """导出全部表。"""
        if not self._tables_dir:
            messagebox.showwarning("提示", "请先打开 tables 目录")
            return

        # 保存当前表
        self._on_save()

        self._update_status("正在导出全部...")
        self._export_runner.run_all(
            self._tables_dir, self._table_editor,
            callback=lambda r: self._update_status(
                f"导出完成: 成功 {r['success_count']}, 失败 {r['fail_count']}"
            ),
        )

    def _on_file_select(self, file_path: str) -> None:
        """文件选中事件。"""
        # 检查是否有未保存的修改
        if self._table_editor.is_modified():
            if not messagebox.askyesno("未保存", "当前修改未保存，是否放弃？"):
                return

        # 加载新文件
        try:
            table_data = read_excel(file_path)
            self._table_editor.load_table(table_data)
            self._current_file = file_path
            self._update_status(f"已加载: {os.path.basename(file_path)}")
            self._update_info(f"共 {len(table_data['rows'])} 行数据")
        except Exception as e:
            messagebox.showerror("加载失败", f"无法加载文件: {e}")

    def _on_data_modified(self) -> None:
        """数据修改回调。"""
        self._update_status(f"已修改: {os.path.basename(self._current_file or '')}")

    # ---- 辅助方法 ----

    def _save_excel(self, file_path: str, table_data: dict) -> None:
        """将表数据保存为 Excel 文件。"""
        from openpyxl import Workbook

        wb = Workbook()
        ws = wb.active

        ws.append(table_data["fields"])
        ws.append(table_data["types"])
        ws.append(table_data["comments"])
        for row in table_data["rows"]:
            ws.append(row)

        wb.save(file_path)

    def _update_status(self, text: str) -> None:
        """更新状态栏文本。"""
        self._status_label.configure(text=text)

    def _update_info(self, text: str) -> None:
        """更新状态栏右侧信息。"""
        self._info_label.configure(text=text)
```

- [ ] **Step 2: 验证语法**

```bash
cd D:/mb_workspace/farm_demo
python -c "from tools.config_editor.app import ConfigEditorApp; print('OK')"
```

Expected: 输出 `OK`

- [ ] **Step 3: 提交**

```bash
git add tools/config_editor/app.py
git commit -m "feat: add config editor main window"
```

---

### Task 11: 编辑器入口脚本

**Files:**
- Create: `tools/run_config_editor.py`

- [ ] **Step 1: 创建入口脚本**

创建 `tools/run_config_editor.py`：

```python
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""配置编辑器启动脚本。"""
import os
import sys

# 确保项目根目录在 Python 路径中
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, project_root)

from tools.config_editor.app import ConfigEditorApp


def main():
    app = ConfigEditorApp(project_root)
    app.run()


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 验证语法**

```bash
cd D:/mb_workspace/farm_demo
python -c "import tools.run_config_editor; print('OK')"
```

Expected: 输出 `OK`

- [ ] **Step 3: 提交**

```bash
git add tools/run_config_editor.py
git commit -m "feat: add config editor entry script"
```

---

### Task 12: 集成测试

**Files:**
- None (手动测试)

- [ ] **Step 1: 启动编辑器验证 GUI**

```bash
cd D:/mb_workspace/farm_demo
python tools/run_config_editor.py
```

Expected: 编辑器窗口正常打开，左侧显示 items.xlsx

- [ ] **Step 2: 测试完整工作流**

1. 编辑器中打开 items.xlsx
2. 编辑一个单元格
3. 点击"保存"
4. 点击"导出当前表"
5. 验证 scripts/client/data/items.py 和 scripts/server/data/items.h 已更新

- [ ] **Step 3: 测试命令行导表**

```bash
cd D:/mb_workspace/farm_demo
python -m tools.table_export tables/items.xlsx
python -m tools.table_export --all tables/
```

Expected: 两个命令都成功执行

- [ ] **Step 4: 运行全部测试**

```bash
cd D:/mb_workspace/farm_demo
python -m pytest tests/test_excel_reader.py tests/test_validators.py tests/test_code_generator.py tests/test_table_export.py -v
```

Expected: 全部 PASS

- [ ] **Step 5: 最终提交**

```bash
git add -A
git commit -m "feat: complete config editor and table export tool"
```
