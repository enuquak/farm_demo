# Task: item-registry（物品定义注册表）

## 任务拆分

### 1. 定义物品类型枚举和物品定义表 [已完成 ✅]
- 定义 ItemType 枚举：RESOURCE, TOOL, SEED, FOOD
- 定义 ITEM_DEFS 字典：7 种物品的 id/name/type/max_stack
- 物品列表：木材(1)、石头(2)、斧头(3)、锄头(4)、种子(5)、面包(6)、作物(7)

### 2. 实现物品定义查询函数 get_item_def [已完成 ✅]
- get_item_def(item_id) -> dict | None
- 存在时返回 {"name": ..., "type": ..., "max_stack": ...}
- 不存在时返回 None

### 3. 定义使用效果表 ITEM_EFFECTS [已完成 ✅]
- 斧头(3) + obj:STONE -> remove_object + drops
- 锄头(4) + gnd:GRASS/gnd:DIRT -> set_ground: TILLED
- 种子(5) + gnd:TILLED -> place_object + consume_self
- 面包(6) + ANY -> consume_self + energy_restore
- 支持 energy_cost 字段（默认 0）
- 支持 interact_range 字段（默认 1）

### 4. 实现效果查询函数 get_item_effect [已完成 ✅]
- get_item_effect(item_id, obj_type, ground_type) -> dict | None
- 优先匹配 obj:xxx（有地物时）
- 其次匹配 gnd:xxx（无地物时）
- 兜底匹配 ANY
- 无匹配返回 None

### 5. 编写单元测试 [已完成 ✅]
- 测试所有物品定义查询
- 测试不存在的物品返回 None
- 测试所有效果查询场景
- 测试 energy_cost 和 interact_range 默认值
- 测试无匹配效果返回 None
