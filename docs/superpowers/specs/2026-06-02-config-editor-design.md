# 配置编辑器 & 导表工具设计文档

## 概述

为 farm_demo 项目开发一个配置编辑器工具，用于管理游戏配置数据（物品、地面属性、作物等），并支持将 Excel 配置表导出为 Python 和 C++ 代码。

### 目标

- 将硬编码的游戏数据（item_registry.py、constants.py、item_effects.cpp 等）迁移到 Excel 表格管理
- 提供 GUI 编辑器方便策划/开发者编辑配置
- 支持单表导出和全表导出
- 生成的代码与现有数据结构兼容

### 非目标

- 不迁移服务端配置文件（dbmgr.json、game_server.json 等）
- 不支持二进制格式导出
- 不支持在线热更新

## Excel 表格规范

### 格式约定

每张 Excel 表遵循统一的行约定：

| 行号 | 内容 | 示例 (items.xlsx) |
|------|------|-------------------|
| 第 1 行 | 字段名（snake_case） | `item_id` \| `name` \| `type` \| `max_stack` |
| 第 2 行 | 字段类型 | `int` \| `str` \| `str` \| `int` |
| 第 3 行 | 字段描述（注释） | `# 物品ID` \| `# 物品名称` \| `# 物品类型` \| `# 最大堆叠数` |
| 第 4 行起 | 数据 | `1` \| `木材` \| `RESOURCE` \| `99` |

### 支持的字段类型

- `int` — 整数
- `float` — 浮点数
- `str` — 字符串
- `bool` — 布尔值（true/false）

### 命名规则

- Excel 文件名即为表名（如 `items.xlsx` → 表名 `items`）
- 字段名使用 snake_case
- 文件名使用 snake_case

## 架构设计

采用模块化架构，导表引擎独立于编辑器，可单独在命令行使用。

### 目录结构

```
farm_demo/
  tables/                          # Excel 源文件目录（新建）
    items.xlsx
    crops.xlsx
    ...
  tools/
    config_editor/                 # 配置编辑器 GUI（新建）
      __init__.py
      app.py                       # 主窗口
      table_editor.py              # 表格编辑组件
      sidebar.py                   # 左侧文件列表
      export_runner.py             # 导表触发逻辑
    table_export/                  # 独立导表引擎（新建）
      __init__.py
      excel_reader.py              # Excel 读取 + 解析
      validators.py                # 数据校验
      code_generator.py            # Python/C++ 代码生成
  scripts/
    client/data/                   # 生成的 Python 数据文件（新建）
      items.py
      crops.py
      ...
    server/data/                   # 生成的 C++ 数据文件（新建）
      items.h
      crops.h
      ...
```

### 依赖

- `openpyxl` — Excel 读写（新增，需添加到 requirements.txt）
- `customtkinter` — GUI 框架（已有，server_console 使用）

### 命令行使用

导表引擎可独立于编辑器在命令行使用：

```bash
# 单表导出
python -m tools.table_export tables/items.xlsx

# 全表导出
python -m tools.table_export --all tables/
```

## 导表引擎设计

### 模块职责

| 文件 | 职责 |
|------|------|
| `table_export/__init__.py` | 暴露 `export_table(file)` 和 `export_all(dir)` 入口函数 |
| `table_export/excel_reader.py` | 读取 Excel 文件，解析行 1/2/3/数据行，返回结构化数据 |
| `table_export/validators.py` | 校验数据（类型检查、主键唯一性检查、非空检查） |
| `table_export/code_generator.py` | 根据模板生成 Python `.py` 和 C++ `.h` 文件 |

### 核心流程

```
Excel 文件 → 读取(excel_reader) → 校验(validators) → 生成 Python(code_generator) → 生成 C++(code_generator)
```

### 单表导出 `export_table(items.xlsx)`

1. 读取 Excel 文件
2. 解析行 1(字段名)、行 2(类型)、行 3(描述)
3. 校验数据行（类型匹配、主键唯一、非空检查）
4. 校验通过 → 生成 Python 文件到 `scripts/client/data/`
5. 校验通过 → 生成 C++ 文件到 `scripts/server/data/`
6. 返回结果（成功/失败 + 错误详情）

### 全表导出 `export_all(tables/)`

1. 遍历 `tables/` 目录下所有 `.xlsx` 文件
2. 逐个执行单表导出
3. 汇总结果（成功 N 个，失败 M 个）
4. 返回汇总结果

### 数据校验规则

- **类型校验**：int 列不能包含非数字值，float 列不能包含非数字值，bool 列只能是 true/false
- **主键唯一**：第一列默认为主键列，其值必须唯一（后续可扩展为指定列）
- **非空检查**：除注释行外，数据行不能为空行
- **校验失败处理**：校验失败的表不会生成代码（原子性），错误信息包含文件名、行号、列名、具体错误

### 生成代码格式

**Python 命名约定：**
- 文件名：表名小写 + `.py`（如 `items.py`）
- 变量名：表名大写蛇形 + `_DEFS`（如 `ITEM_DEFS`）

**Python 输出（scripts/client/data/items.py）：**

```python
# 自动生成，请勿手动修改
# 生成时间：2026-06-02 22:30:00
# 源文件：tables/items.xlsx

ITEM_DEFS = {
    1: {"name": "木材", "type": "RESOURCE", "max_stack": 99},
    2: {"name": "石头", "type": "RESOURCE", "max_stack": 99},
    3: {"name": "斧头", "type": "TOOL", "max_stack": 1},
}
```

**C++ 命名约定：**
- 文件名：表名小写 + `.h`（如 `items.h`）
- struct 名：表名 PascalCase + `Def`（如 `items` → `ItemDef`）
- 数据变量名：表名大写 + `_DEFS`（如 `ITEM_DEFS`）

**C++ 输出（scripts/server/data/items.h）：**

```cpp
// 自动生成，请勿手动修改
// 生成时间：2026-06-02 22:30:00
// 源文件：tables/items.xlsx
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace farm {

struct ItemDef {
    int32_t item_id;
    std::string name;
    std::string type;
    int32_t max_stack;
};

inline const std::unordered_map<int32_t, ItemDef> ITEM_DEFS = {
    {1, {1, "木材", "RESOURCE", 99}},
    {2, {2, "石头", "RESOURCE", 99}},
    {3, {3, "斧头", "TOOL", 1}},
};

}  // namespace farm
```

## 编辑器 UI 设计

### 整体布局

采用左侧表列表 + 右侧编辑区的布局（布局 A）。

```
┌─────────────────────────────────────────────────────┐
│  工具栏：[打开目录] [新建表] [保存] [导出当前表] [导出全部]  │
├──────────┬──────────────────────────────────────────┤
│ 配置表    │  items.xlsx                              │
│          │  ┌────────────────────────────────────┐  │
│ 📦 items │  │ item_id | name | type | max_stack  │  │
│ 📦 crops │  │ int     | str  | str  | int        │  │
│ 📦 ...   │  │ #物品ID | #名称| #类型| #最大堆叠  │  │
│          │  ├────────────────────────────────────┤  │
│          │  │ 1 | 木材 | RESOURCE | 99           │  │
│          │  │ 2 | 石头 | RESOURCE | 99           │  │
│          │  │ 3 | 斧头 | TOOL     | 1            │  │
│          │  └────────────────────────────────────┘  │
├──────────┴──────────────────────────────────────────┤
│ 状态栏：就绪 | 已修改 ✓ | 共 7 行                    │
└─────────────────────────────────────────────────────┘
```

### 组件说明

**工具栏：**
- `打开目录` — 选择 tables 目录路径
- `新建表` — 创建新的 Excel 文件（输入表名 + 初始字段）
- `保存` — 保存当前编辑的 Excel 文件
- `导出当前表` — 对当前表执行单表导出
- `导出全部` — 对所有表执行全表导出

**左侧边栏：**
- 列出 `tables/` 目录下所有 `.xlsx` 文件
- 点击切换右侧编辑区内容
- 支持右键菜单（重命名文件、删除文件）

**右侧编辑区：**
- 以可编辑表格形式展示 Excel 内容
- 行 1（字段名）和行 2（字段类型）冻结在顶部，可编辑
- 行 3（字段描述）冻结在顶部，可编辑
- 数据行（行 4 起）可自由编辑
- 支持在数据区域插入行、删除行

**状态栏：**
- 显示当前状态（就绪/编辑中/导出中）
- 显示修改标记（已修改 ✓ / 未修改）
- 显示当前表数据行数

### 实时校验

- 编辑时实时校验（如 int 列不能输入字符串）
- 错误单元格标红，鼠标悬停显示错误详情
- 导出前全面校验，有错误时弹出汇总对话框

## 错误处理

### 导表错误

- 校验失败的表不会生成代码（原子性）
- 错误信息格式：`[文件名] 第 X 行, 列 'Y': 错误描述`
- 编辑器中错误行高亮标红
- 全表导出时，单个表失败不影响其他表继续导出

### 编辑器错误

- Excel 文件损坏时显示错误对话框，不崩溃
- 保存失败时提示用户检查文件权限
- 导出失败时在状态栏显示错误数量
