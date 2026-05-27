## Context

Farm Demo 的物品交互系统需要资源约束。在 C++ 服务器 + Python 客户端架构下，能量数据由服务端管理，客户端负责 UI 展示。

## Goals / Non-Goals

**Goals:**
- 每次交互消耗对应能量值，能量不足时阻止交互并弹窗提示
- 面包食用恢复 15 能量，成为有意义的消耗品
- 右下角竖条能量条 UI，颜色随能量比例变化
- "精疲力尽" 弹窗暂停游戏输入
- 能量值纳入存档系统

**Non-Goals:**
- 不实现睡眠/休息恢复机制（后续 game-clock 迭代）
- 不实现能量上限升级
- 不实现能量 buff/debuff
- 不实现能量随时间自然恢复
- 不实现移动消耗能量

## Decisions

### D1: 能量数据挂在 ITEM_EFFECTS 中

**选择**: 在现有 effect 对象中添加 `energy_cost` 字段

```python
ITEM_EFFECTS = {
    3: {"obj:STONE": {"remove_object": True, "drops": [...], "energy_cost": 4}},
    4: {"gnd:GRASS": {"set_ground": "TILLED", "energy_cost": 2}},
    5: {"gnd:TILLED": {"place_object": "CROP_GROWING", "consume_self": True, "energy_cost": 1}},
    6: {"ANY": {"consume_self": True, "energy_restore": 15}},
}

# energy_cost 语义：
# 正数 = 消耗能量
# 负数 = 恢复能量（或用 energy_restore 字段）
# 0 / 缺失 = 免费
```

**能量消耗/恢复表：**

| 交互 | 物品 | 目标 | energy_cost | 说明 |
|------|------|------|------------|------|
| 敲碎石头 | 斧头 | STONE | 4 | 重体力 |
| 翻耕草地 | 锄头 | GRASS | 2 | 中体力 |
| 翻耕泥土 | 锄头 | DIRT | 2 | 中体力 |
| 播种 | 种子 | TILLED | 1 | 轻体力 |
| 吃面包 | 面包 | ANY | -15 | 恢复能量 |
| 收割 | 空手 | CROP_READY | 1 | 轻体力 |

**理由**: 零新配置表，复用现有数据驱动架构。新增物品时只需在 effect 中加一个数字。

### D2: Energy 数据模型 — 服务端管理

**选择**: `scripts/server/energy.py` 独立类

```python
class Energy:
    def __init__(self, max_energy=100):
        self.current = max_energy
        self.max_energy = max_energy

    def can_consume(self, cost) -> bool:
        return self.current >= cost

    def consume(self, cost) -> bool:
        if cost > 0 and not self.can_consume(cost):
            return False
        self.current = max(0, min(self.max_energy, self.current - cost))
        return True

    def restore(self, amount):
        self.current = min(self.max_energy, self.current + amount)

    def serialize(self) -> dict:
        return {"current": self.current, "max": self.max_energy}

    @classmethod
    def deserialize(cls, data: dict) -> 'Energy':
        e = cls(data.get("max", 100))
        e.current = data.get("current", e.max_energy)
        return e
```

**理由**: 独立类方便后续扩展（升级上限、buff 等），也便于序列化。

### D3: ItemInteraction 改造 — 服务端

**选择**: 在物品使用流程中插入能量检查

```
服务器处理 ItemUseReq:
  1. 获取 active item + 匹配效果
  2. 获取 energy_cost
  3. if energy_cost > 0 and not energy.can_consume(energy_cost):
       → 返回 ItemUseResp code=ENERGY_EXHAUSTED
  4. energy.consume(energy_cost)  # 负数自动变为 restore
  5. 执行效果
```

**收割的能量消耗**: 收割路径硬编码 energy_cost=1，因为收割不走 ITEM_EFFECTS。

### D4: 能量条 UI — 客户端 PyGame

**选择**: 右下角竖条，PyGame 绘制

```
┌────────┐
│ 75/100 │  ← 数字标签（PyGame font）
├────────┤
│ ██████ │  ← 填充部分（颜色随比例变化）
│ ██████ │
│ ██████ │
│ ░░░░░░ │  ← 空白部分（深灰）
│ ░░░░░░ │
└────────┘
```

**布局**: 右下角，宽 40px，高 120px，从下往上填充

**颜色梯度**:
```python
if ratio >= 0.6: color = (76, 175, 80)    # 绿色
elif ratio >= 0.3: color = (255, 152, 0)   # 橙黄
else: color = (244, 67, 54)                # 红色
```

### D5: 精疲力尽弹窗 — 客户端

**选择**: PyGame 绘制模态覆盖层

```
┌──────────────────────────────────┐
│                                  │
│         精疲力尽！               │
│                                  │
│    能量不足，无法执行此操作       │
│                                  │
│           [ 确定 ]               │
│                                  │
└──────────────────────────────────┘
```

**行为**:
- 弹窗出现时暂停游戏输入
- 点击"确定"或按 Enter/Escape 关闭
- 关闭后恢复正常游戏

### D6: 协议扩展

```protobuf
// 能量同步（服务器 → 客户端，登录时 + 变更时）
message EnergySync {
    int32 current = 1;
    int32 max = 2;
}

// ItemUseResp 扩展
message ItemUseResp {
    int32 code = 1;  // 新增: ENERGY_EXHAUSTED = 3
    EnergySync energy = 5;
}
```

### D7: 存档扩展

```json
{
    "energy": {"current": 75, "max": 100}
}
```

**向后兼容**: 旧存档无 energy 字段时默认 100/100。

## Risks / Trade-offs

**[Risk] 能量耗尽后无恢复手段**
→ 面包吃完就真的无法操作了。
→ Mitigation: 初始 3 个面包 + 收割作物后续可获得更多食物。后续 game-clock 引入睡觉恢复。

**[Risk] 数值平衡**
→ 消耗太快/太慢影响体验。
→ Mitigation: 数值集中在 ITEM_EFFECTS 中，易于调整。

**[Trade-off] 客户端弹窗 vs 服务器推送**
→ 服务器返回 ENERGY_EXHAUSTED 错误码，客户端显示弹窗。
→ 简单清晰，一次网络往返。
