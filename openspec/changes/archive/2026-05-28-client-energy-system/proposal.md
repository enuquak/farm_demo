## Why

Farm Demo 的物品交互系统目前没有任何资源约束——玩家可以无限砍石头、翻耕、播种。缺少能量机制使得交互没有策略性，面包作为食物也没有实际用途。需要引入能量系统，让每次交互消耗能量，面包等食物恢复能量，形成"劳作 → 消耗 → 进食 → 恢复"的资源管理循环。

## What Changes

- **新增能量数据模型（服务端）**：Energy 类管理当前能量值/最大能量值（100/100），支持消耗、恢复、持久化
- **改造交互效果表**：ITEM_EFFECTS 的每个 effect 新增 energyCost 字段（正数=消耗，负数=恢复）
- **改造交互逻辑**：物品使用前检查能量是否充足，不足时返回错误码
- **新增能量条 UI（客户端）**：右下角竖条能量条（PyGame 渲染），显示当前/最大值，颜色随能量比例变化（绿/黄/红）
- **新增精疲力尽弹窗（客户端）**：能量不足时弹出模态对话框，弹窗期间暂停游戏输入
- **扩展存档**：能量值纳入 DBMgr 持久化

## Capabilities

### New Capabilities

- `energy-system`: 能量数据模型 + 能量条 UI + 精疲力尽弹窗 + 交互能量消耗/恢复机制

### Modified Capabilities

- `item-registry`: ITEM_EFFECTS 每个 effect 新增 energyCost 字段
- `item-interaction`: 物品使用前新增能量检查/扣除逻辑
- `player-entity`: Player 数据扩展 energy 字段
- `game-server`: 新增能量相关消息处理

## Impact

- **新增文件**: `scripts/client/ui/energy_bar.py`、`scripts/client/ui/exhaustion_modal.py`、`scripts/server/energy.py`
- **修改文件**: `scripts/server/item_interaction.py`（+能量检查）、`scripts/server/item_registry.py`（+energyCost）、`scripts/client/main.py`（+UI 初始化）
- **存档格式变更**: Player 数据新增 energy 字段
