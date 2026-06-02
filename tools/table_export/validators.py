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

    for row_idx, row in enumerate(rows, start=2):  # 数据从第 2 行开始
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

    for row_idx, row in enumerate(rows, start=2):
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
