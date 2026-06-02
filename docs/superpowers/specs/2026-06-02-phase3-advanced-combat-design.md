# Phase 3 进阶战斗系统设计

**日期**: 2026-06-02
**状态**: 已批准
**范围**: Boss 怪物、武器合成、武器强化、战斗经验+升级
**依据**: 基于 `2026-06-02-monster-combat-system-design.md` 第 6.3/8/9 节

---

## 1. 概述

Phase 3 在 Phase 1（基础战斗）和 Phase 2（矿洞场景）基础上，添加进阶战斗玩法：Boss 怪物挑战、武器合成与强化、战斗经验升级系统。

### 1.1 功能清单

| 功能 | 描述 |
|------|------|
| Boss 怪物 | 阶段战斗、特殊攻击、召唤小怪 |
| 武器合成 | 配方系统、材料收集、铁砧交互 |
| 武器强化 | 精炼石材料、成功率递减、失败惩罚 |
| 战斗经验 | 击杀获得经验、升级增加 HP、解锁配方 |

---

## 2. Boss 怪物系统

### 2.1 Boss 定义（boss_defs.json）

```json
{
  "bosses": [
    {
      "id": "boss_skeleton_king",
      "name": "骷髅王",
      "sprite_sheet": "boss_skeleton_king.png",
      "hp": 500,
      "attack": 25,
      "defense": 10,
      "speed": 20.0,
      "exp_reward": 500,
      "size": [2, 2],
      "phases": [
        {
          "hp_threshold": 1.0,
          "attack_pattern": "normal",
          "speed_multiplier": 1.0,
          "description": "普通阶段"
        },
        {
          "hp_threshold": 0.5,
          "attack_pattern": "aggressive",
          "speed_multiplier": 1.3,
          "summon_monsters": ["skeleton", "skeleton"],
          "description": "狂暴阶段：召唤小怪，速度提升"
        }
      ],
      "special_attacks": [
        {
          "id": "ground_slam",
          "name": "地震",
          "cooldown": 8.0,
          "range": 3,
          "damage_multiplier": 2.0,
          "aoe_radius": 2,
          "animation": "boss_slam"
        }
      ]
    }
  ]
}
```

### 2.2 Boss 战斗机制

**阶段转换**：
- Boss HP 降至 50% 时进入第二阶段
- 阶段转换时播放特效，短暂无敌（2 秒）
- 第二阶段：速度提升 30%、召唤 2 个骷髅小怪

**特殊攻击**：
- 地震（ground_slam）：范围 AOE，伤害 = Boss 攻击力 × 2.0
- 特殊攻击有明显前摇动画（1 秒），给玩家反应时间
- 冷却时间 8 秒防止连续释放

**Boss 房间**：
- 独立场景（cave_boss），空间 20x20
- 进入时关闭入口门，击败 Boss 后开门
- Boss 不自动刷新（一次性挑战）

### 2.3 Boss 击败奖励

- 固定掉落稀有物品（王冠碎片 ×1）
- 高额经验值（500）
- 概率掉落高级武器（金剑 15%）
- 首次击杀额外奖励（成就系统扩展）

---

## 3. 武器合成系统

### 3.1 武器配方（weapon_recipes.json）

```json
{
  "weapon_recipes": [
    {
      "result": "iron_sword",
      "materials": [
        {"item_id": 204, "count": 5},
        {"item_id": 301, "count": 3}
      ],
      "unlock_level": 5,
      "craft_station": "anvil"
    },
    {
      "result": "gold_sword",
      "materials": [
        {"item_id": 205, "count": 5},
        {"item_id": 204, "count": 3},
        {"item_id": 206, "count": 1}
      ],
      "unlock_level": 10,
      "craft_station": "anvil"
    }
  ]
}
```

### 3.2 合成流程

```
玩家与铁砧交互（按空格）
       │
       ▼
打开合成界面
       │
       ▼
显示可合成武器列表（灰色锁定/彩色可合成）
       │
       ▼
玩家选择武器
       │
       ▼
检查材料是否充足 & 等级是否达标
       │
       ▼ 是
扣除材料，生成武器到背包
       │
       ▼
播放合成成功特效
```

### 3.3 合成界面

- 铁砧交互打开合成界面（Pygame overlay）
- 左侧：武器列表（图标 + 名称 + 锁定状态）
- 右侧：选中武器的详情（材料需求、属性预览）
- 底部：合成按钮（材料不足时灰色）

---

## 4. 武器强化系统

### 4.1 强化材料

| 材料 | 来源 | 用途 |
|------|------|------|
| 精炼石 (item_id: 207) | 矿洞 2-3 层怪物掉落 | 强化武器 +1~+3 |
| 高级精炼石 (item_id: 208) | Boss 掉落 | 强化武器 +4~+6 |
| 传说精炼石 (item_id: 209) | Boss 首杀/稀有掉落 | 强化武器 +7~+10 |

### 4.2 强化规则

| 当前等级 | 成功率 | 失败惩罚 |
|----------|--------|----------|
| +0 → +1 | 100% | 无 |
| +1 → +2 | 90% | 无 |
| +2 → +3 | 80% | 无 |
| +3 → +4 | 70% | 等级不变 |
| +4 → +5 | 60% | 等级不变 |
| +5 → +6 | 50% | 等级 -1 |
| +6 → +7 | 40% | 等级 -1 |
| +7 → +8 | 30% | 等级 -2 |
| +8 → +9 | 20% | 等级 -2 |
| +9 → +10 | 10% | 等级 -3 |

### 4.3 强化加成

- 每级强化：攻击力 +5%，攻击速度 +2%
- +5 额外效果：武器发光（视觉标记）
- +10 额外效果：武器特效（粒子效果）

### 4.4 强化流程

```
玩家与铁砧交互，选择"强化"选项卡
       │
       ▼
显示当前装备的武器和强化等级
       │
       ▼
显示所需精炼石和成功率
       │
       ▼
玩家点击"强化"按钮
       │
       ▼
检查精炼石数量是否充足
       │
       ▼ 是
扣除精炼石，随机判定成功/失败
       │
       ▼
成功：强化等级 +1，播放成功特效
失败：根据惩罚规则降级，播放失败特效
```

---

## 5. 战斗经验与升级

### 5.1 经验获取

| 来源 | 经验值 |
|------|--------|
| 史莱姆 | 10 |
| 蝙蝠 | 15 |
| 骷髅战士 | 50 |
| Boss 骷髅王 | 500 |

### 5.2 升级公式

```
需要经验 = 50 + (level - 1) * 30 + level^2 * 10
```

### 5.3 等级表

| 等级 | 所需经验 | 累计经验 | 奖励 |
|------|----------|----------|------|
| 1→2 | 50 | 50 | +5 最大HP |
| 2→3 | 120 | 170 | +5 最大HP |
| 3→4 | 200 | 370 | +10 最大HP |
| 4→5 | 350 | 720 | +10 最大HP, 解锁铁剑配方 |
| 5→6 | 500 | 1220 | +15 最大HP |
| 6→7 | 720 | 1940 | +15 最大HP |
| 7→8 | 1010 | 2950 | +20 最大HP |
| 8→9 | 1370 | 4320 | +20 最大HP |
| 9→10 | 1800 | 6120 | +25 最大HP, 解锁金剑配方 |

### 5.4 升级效果

- 每次升级增加最大 HP（当前 HP 也恢复到新的最大值）
- 特定等级解锁武器合成配方（5 级解锁铁剑，10 级解锁金剑）
- 升级时播放特效和音效

### 5.5 消息协议

复用已有的消息 ID：
- `MSG_ID_COMBAT_EXP_UPDATE = 5030` — 经验值更新
- `MSG_ID_COMBAT_LEVEL_UP = 5031` — 升级通知

新增消息：
- `MSG_ID_WEAPON_CRAFT_REQ = 5040` — 武器合成请求
- `MSG_ID_WEAPON_CRAFT_RESP = 5041` — 武器合成响应
- `MSG_ID_WEAPON_ENHANCE_REQ = 5042` — 武器强化请求
- `MSG_ID_WEAPON_ENHANCE_RESP = 5043` — 武器强化响应

---

## 6. 新增数据文件

| 文件 | 内容 |
|------|------|
| `scripts/client/data/boss_defs.json` | Boss 定义（阶段、特殊攻击） |
| `scripts/client/data/weapon_recipes.json` | 武器合成配方 |
| `scripts/client/data/enhance_table.json` | 强化成功率和惩罚表 |

---

## 7. 新增/修改模块

### 7.1 客户端

| 模块 | 职责 |
|------|------|
| `boss_manager.py` (新增) | Boss 实例管理、阶段转换、特殊攻击 |
| `craft_ui.py` (新增) | 合成界面 UI |
| `enhance_ui.py` (新增) | 强化界面 UI |
| `combat_system.py` (修改) | 添加经验获取和升级逻辑 |
| `battle_ui.py` (修改) | 添加经验条和升级特效 |
| `game_scene.py` (修改) | 集成 Boss、合成、强化系统 |

### 7.2 服务器

| 模块 | 职责 |
|------|------|
| `boss_handler.h/.cpp` (新增) | Boss 战斗逻辑、阶段转换 |
| `craft_handler.h/.cpp` (新增) | 武器合成验证 |
| `enhance_handler.h/.cpp` (新增) | 武器强化验证 |
| `combat_handler.cpp` (修改) | 添加经验奖励和升级 |
| `player.h/.cpp` (修改) | 添加强化等级字段 |
| `game_server.cpp` (修改) | 注册新消息 handler |

---

## 8. 实现范围

### Phase 3 子任务

1. **Boss 数据与定义** — boss_defs.json, enhance_table.json
2. **Boss 战斗逻辑** — boss_handler (服务器), boss_manager (客户端)
3. **战斗经验与升级** — 经验获取、升级公式、HP 奖励
4. **武器合成系统** — weapon_recipes.json, craft_handler, craft_ui
5. **武器强化系统** — enhance_handler, enhance_ui
6. **集成与测试** — 端到端验证
