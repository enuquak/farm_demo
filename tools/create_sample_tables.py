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
