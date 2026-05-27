## Purpose

背包数据模型：在服务端管理 30 格背包（10 快捷栏 + 20 扩展），支持物品增删、堆叠合并、快捷栏 activeSlot 管理，以及持久化到 DBMgr。

## Requirements

### Requirement: 背包数据结构
Inventory SHALL 维护 30 格 slot 数组和 1 个 activeSlot 指针。

#### Scenario: 初始化空背包
- **WHEN** 新玩家首次进入游戏
- **THEN** 创建 30 格全为 None 的 slot 数组，activeSlot=0

#### Scenario: 初始物品
- **WHEN** 新玩家首次进入游戏
- **THEN** 背包中 SHALL 预置：slot[0]=斧头×1, slot[1]=锄头×1, slot[2]=种子×5, slot[3]=面包×3

#### Scenario: slot 数据结构
- **WHEN** slot 非空
- **THEN** 包含 `item_id`（物品ID）和 `count`（数量）两个字段

### Requirement: 添加物品
Inventory SHALL 支持 `add_item(item_id, count)` 方法，自动堆叠和找空位。

#### Scenario: 堆叠到已有 slot
- **WHEN** 背包中 slot[2]=种子×3，调用 `add_item(5, 2)`（种子）
- **THEN** slot[2] 更新为种子×5，返回 0（全部放入）

#### Scenario: 堆叠溢出到新 slot
- **WHEN** 背包中 slot[2]=种子×98（max_stack=99），调用 `add_item(5, 5)`
- **THEN** slot[2]=种子×99，找到空 slot 放入种子×4，返回 0

#### Scenario: 背包满
- **WHEN** 所有 slot 都已满，调用 `add_item(2, 3)`
- **THEN** 返回 3（未能放入的数量）

#### Scenario: 返回剩余数量
- **WHEN** 背包只有 1 个空 slot，调用 `add_item(2, 100)` 且 max_stack=99
- **THEN** 空 slot 放入石头×99，返回 1（剩余 1 个未放入）

### Requirement: 移除物品
Inventory SHALL 支持 `remove_item(item_id, count)` 方法。

#### Scenario: 从单个 slot 移除
- **WHEN** slot[3]=面包×3，调用 `remove_item(6, 1)`（面包）
- **THEN** slot[3] 更新为面包×2，返回 True

#### Scenario: 跨多个 slot 移除
- **WHEN** slot[3]=面包×2, slot[7]=面包×1，调用 `remove_item(6, 3)`
- **THEN** slot[3]=None, slot[7]=None，返回 True

#### Scenario: 数量不足
- **WHEN** 背包中总共只有 2 个面包，调用 `remove_item(6, 3)`
- **THEN** 返回 False，背包不变

#### Scenario: 物品不存在
- **WHEN** 背包中无木材，调用 `remove_item(1, 1)`
- **THEN** 返回 False

### Requirement: 查询物品数量
Inventory SHALL 支持 `get_count(item_id)` 方法。

#### Scenario: 查询存在的物品
- **WHEN** slot[2]=种子×5, slot[8]=种子×3，调用 `get_count(5)`
- **THEN** 返回 8

#### Scenario: 查询不存在的物品
- **WHEN** 背包中无木材，调用 `get_count(1)`
- **THEN** 返回 0

### Requirement: 快捷栏 activeSlot
Inventory SHALL 维护 activeSlot（0~9），指向当前选中的快捷栏格。

#### Scenario: 切换 activeSlot
- **WHEN** 调用 `set_active_slot(3)`
- **THEN** activeSlot 更新为 3

#### Scenario: activeSlot 范围限制
- **WHEN** 调用 `set_active_slot(15)`
- **THEN** 不生效，activeSlot 保持不变（仅 0~9 有效）

#### Scenario: 获取手持物品
- **WHEN** activeSlot=2, slot[2]=种子×5，调用 `get_active_item()`
- **THEN** 返回 `{"item_id": 5, "count": 5}`

#### Scenario: 手持空物品
- **WHEN** activeSlot=4, slot[4]=None，调用 `get_active_item()`
- **THEN** 返回 None

### Requirement: 序列化与反序列化
Inventory SHALL 支持序列化为 dict 和从 dict 反序列化。

#### Scenario: 序列化
- **WHEN** 调用 `inventory.serialize()`
- **THEN** 返回 `{"slots": [...], "active_slot": 0}`，空 slot 序列化为 None

#### Scenario: 反序列化
- **WHEN** 调用 `Inventory.deserialize(data)`
- **THEN** 恢复完整的背包状态，包括所有 slot 和 activeSlot

#### Scenario: 旧存档兼容
- **WHEN** 存档数据中无 inventory 字段
- **THEN** 使用默认初始物品创建 Inventory

### Requirement: 持久化到 DBMgr
玩家背包数据 SHALL 在离开时保存到 DBMgr。

#### Scenario: 玩家离开时保存
- **WHEN** 玩家下线或服务器关闭
- **THEN** 将 inventory.serialize() 数据通过 SET_ALL 写入 DBMgr

#### Scenario: 玩家登录时加载
- **WHEN** 玩家登录，DBMgr 返回 player data 包含 inventory 字段
- **THEN** 调用 Inventory.deserialize(data["inventory"]) 恢复背包
