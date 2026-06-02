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
