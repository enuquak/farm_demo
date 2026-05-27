## Why

牧场类游戏的核心循环依赖"日"节奏——白天劳作、夜晚休息。当前游戏没有时间概念，玩家可以无限制地活动。加入游戏内时钟后，体力消耗、作物生长、强制休息等系统才有锚点，形成"一天"的完整游戏循环。

## What Changes

- **新增 GameClock 模块（服务端）**：以 30 分钟为粒度推进游戏时间（现实 1 分钟 = 游戏 30 分钟），每天 AM 6:00 起床 → AM 2:00 强制睡觉，共 40 个 slot / 天
- **新增左上角 TimeHUD（客户端）**：PyGame 渲染，显示当前游戏时间（如 "AM 6:00 第 1 天"）和天数
- **新增强制睡觉机制**：AM 2:00 到达时，服务端通知客户端执行 Iris 黑屏过渡 → 传送至 House 床旁 → 体力恢复 50% → 新的一天开始
- **时间暂停**：打开背包、弹窗期间客户端通知服务端暂停时间推进
- **存档升级**：新增 clock 数据纳入 DBMgr 持久化

## Capabilities

### New Capabilities

- `game-clock`: 游戏内时钟核心逻辑（tick 推进、日/时/分计算、暂停/恢复、slot 事件）
- `time-hud`: 左上角时间显示 UI（PyGame 渲染，显示时间和天数）
- `forced-sleep`: 强制睡觉流程（AM 2:00 触发 → Iris 黑屏 → 场景切换 → 体力恢复 → 新一天）

### Modified Capabilities

- `energy-system`: 强制睡觉时体力恢复 50% 最大值
- `scene-management`: 处理强制睡觉的场景切换
- `game-server`: 集成 GameClock，管理时间推进与广播
- `player-entity`: Player 数据扩展 clock 字段

## Impact

- **新增文件**: `scripts/client/ui/time_hud.py`、`scripts/server/game_clock.py`
- **修改文件**: `scripts/client/main.py`（TimeHUD 集成）、`scripts/client/scene/scene_manager.py`（强制睡觉过渡）、`scripts/server/`（时钟消息处理）
- **存档格式变更**: Player 数据新增 clock 字段
