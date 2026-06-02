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
