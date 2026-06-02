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
