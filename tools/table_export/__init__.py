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
            status = "[OK]" if r["success"] else "[FAIL]"
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
