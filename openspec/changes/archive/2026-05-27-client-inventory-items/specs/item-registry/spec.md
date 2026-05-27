## Purpose

物品定义注册表：在服务端静态配置所有物品的 ID、名称、类型、堆叠上限、使用效果映射。数据驱动设计，新增物品只需添加配置，不改逻辑代码。

## Requirements

### Requirement: 物品定义
系统 SHALL 在 `item_registry.py` 中定义所有物品的静态配置。

#### Scenario: 物品类型枚举
- **WHEN** 系统初始化
- **THEN** 定义以下物品类型：RESOURCE（资源）、TOOL（工具）、SEED（种子）、FOOD（食物）

#### Scenario: 物品定义列表
- **WHEN** 系统初始化
- **THEN** 定义以下物品：
  - id=1, name="木材", type=RESOURCE, max_stack=99
  - id=2, name="石头", type=RESOURCE, max_stack=99
  - id=3, name="斧头", type=TOOL, max_stack=1
  - id=4, name="锄头", type=TOOL, max_stack=1
  - id=5, name="种子", type=SEED, max_stack=99
  - id=6, name="面包", type=FOOD, max_stack=20
  - id=7, name="作物", type=RESOURCE, max_stack=99

#### Scenario: 查询物品定义
- **WHEN** 调用 `get_item_def(3)`
- **THEN** 返回斧头的定义 dict：`{"name": "斧头", "type": "TOOL", "max_stack": 1}`

#### Scenario: 查询不存在的物品
- **WHEN** 调用 `get_item_def(999)`
- **THEN** 返回 None

### Requirement: 使用效果表
系统 SHALL 维护 ITEM_EFFECTS 配置表，定义物品在不同目标上的效果。

#### Scenario: 斧头砍石头
- **WHEN** 查询 ITEM_EFFECTS[3]["obj:STONE"]
- **THEN** 返回 `{"remove_object": True, "drops": [{"id": 2, "min": 1, "max": 3}]}`

#### Scenario: 锄头翻耕
- **WHEN** 查询 ITEM_EFFECTS[4]["gnd:GRASS"]
- **THEN** 返回 `{"set_ground": "TILLED"}`

#### Scenario: 种子播种
- **WHEN** 查询 ITEM_EFFECTS[5]["gnd:TILLED"]
- **THEN** 返回 `{"place_object": "CROP_GROWING", "consume_self": True}`

#### Scenario: 面包食用
- **WHEN** 查询 ITEM_EFFECTS[6]["ANY"]
- **THEN** 返回 `{"consume_self": True, "energy_restore": 15}`

#### Scenario: 无匹配效果
- **WHEN** 查询 ITEM_EFFECTS[1]["gnd:GRASS"]（木材 + 草地）
- **THEN** 返回 None（木材无使用效果）

### Requirement: 效果查询函数
系统 SHALL 提供 `get_item_effect(item_id, obj_type, ground_type)` 查询函数。

#### Scenario: 有地物时查询
- **WHEN** 调用 `get_item_effect(3, "STONE", "GRASS")`（斧头 + 石头 + 草地）
- **THEN** 返回 ITEM_EFFECTS[3]["obj:STONE"]（优先匹配地物）

#### Scenario: 无地物时查询
- **WHEN** 调用 `get_item_effect(4, None, "GRASS")`（锄头 + 草地）
- **THEN** 返回 ITEM_EFFECTS[4]["gnd:GRASS"]（匹配地面）

#### Scenario: 兜底查询
- **WHEN** 调用 `get_item_effect(6, None, "GRASS")`（面包 + 草地）
- **THEN** 返回 ITEM_EFFECTS[6]["ANY"]

### Requirement: 能量消耗配置
ITEM_EFFECTS 中每个效果 SHALL 支持可选的 `energy_cost` 字段。

#### Scenario: 消耗能量
- **WHEN** 查询 ITEM_EFFECTS[3]["obj:STONE"]["energy_cost"]
- **THEN** 返回 4（斧头砍石头消耗 4 点能量）

#### Scenario: 无能量消耗
- **WHEN** 查询的效果中无 energy_cost 字段
- **THEN** 默认为 0（免费）

### Requirement: 交互距离配置
ITEM_EFFECTS 中每个效果 SHALL 支持可选的 `interact_range` 字段。

#### Scenario: 九宫格范围
- **WHEN** 查询的效果中 interact_range 缺失
- **THEN** 默认为 1（九宫格范围）

#### Scenario: 无限制范围
- **WHEN** 查询 ITEM_EFFECTS[6]["ANY"]["interact_range"]
- **THEN** 返回 -1（面包食用无距离限制）
