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


def _singularize(name: str) -> str:
    """将表名单数化（简单规则：去掉末尾 s）。"""
    if name.endswith("s"):
        return name[:-1]
    return name


def _to_upper_snake(name: str) -> str:
    """将表名转为大写蛇形单数形式。"""
    return _singularize(name).upper()


def _to_pascal_case(name: str) -> str:
    """将表名转为 PascalCase 单数形式。"""
    return "".join(word.capitalize() for word in _singularize(name).split("_"))


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
        return "{}"
    return f'"{value}"'


def _atomic_write(file_path: str, content: str, temp_suffix: str) -> None:
    """原子写入：先写临时文件，再重命名。"""
    temp_path = file_path + temp_suffix
    with open(temp_path, "w", encoding="utf-8") as f:
        f.write(content)
    os.replace(temp_path, file_path)
