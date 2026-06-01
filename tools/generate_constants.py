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

    os.replace(temp_path, output_path)


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

    os.replace(temp_path, output_path)


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
