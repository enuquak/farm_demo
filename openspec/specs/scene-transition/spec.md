## Purpose

场景过渡动效：实现 Iris 圆形遮罩过渡效果，以玩家屏幕位置为圆心，收缩→切场景→展开，使用 PyGame Surface mask 实现。

## Requirements

### Requirement: 过渡状态机
SceneTransition SHALL 维护状态机：IDLE → IRIS_CLOSE → SWITCHING → IRIS_OPEN → IDLE。

#### Scenario: 初始状态
- **WHEN** 无场景切换进行中
- **THEN** 状态为 IDLE

#### Scenario: 启动过渡
- **WHEN** 调用 `transition.start(player_screen_pos)`
- **THEN** 状态切换为 IRIS_CLOSE，记录圆心位置和最大半径

#### Scenario: 状态自动推进
- **WHEN** update(dt) 被每帧调用
- **THEN** 状态按 IDLE → IRIS_CLOSE → SWITCHING → IRIS_OPEN → IDLE 自动推进

### Requirement: Iris 收缩阶段
IRIS_CLOSE 阶段 SHALL 在 300ms 内将圆形遮罩从最大半径收缩到 0。

#### Scenario: 收缩动画
- **WHEN** 状态为 IRIS_CLOSE，最大半径=600px
- **THEN** 每帧 radius 减少 600 * dt / 0.3，300ms 后 radius=0

#### Scenario: 收缩完成
- **WHEN** radius ≤ 0
- **THEN** 状态切换为 SWITCHING

### Requirement: 场景切换阶段
SWITCHING 阶段 SHALL 在 1 帧内完成场景数据替换。

#### Scenario: 切换时机
- **WHEN** 状态为 SWITCHING
- **THEN** SceneManager 执行场景数据替换（替换 TileMap、更新玩家位置），然后状态切换为 IRIS_OPEN

#### Scenario: 切换期间屏幕全黑
- **WHEN** 状态为 SWITCHING（radius=0）
- **THEN** 遮罩完全覆盖屏幕，玩家看到全黑

### Requirement: Iris 展开阶段
IRIS_OPEN 阶段 SHALL 在 300ms 内将圆形遮罩从 0 展开到最大半径。

#### Scenario: 展开动画
- **WHEN** 状态为 IRIS_OPEN，最大半径=600px
- **THEN** 每帧 radius 增加 600 * dt / 0.3，300ms 后 radius=600

#### Scenario: 展开完成
- **WHEN** radius ≥ max_radius
- **THEN** 状态切换为 IDLE，过渡结束

### Requirement: 圆形遮罩渲染
系统 SHALL 使用 PyGame Surface 操作实现圆形遮罩。

#### Scenario: 遮罩绘制
- **WHEN** 过渡进行中（非 IDLE）
- **THEN** 创建 SRCALPHA Surface，填充黑色 (0,0,0,255)，以玩家位置为圆心绘制透明圆形

#### Scenario: 圆心跟随玩家
- **WHEN** 过渡启动时玩家在屏幕 (400, 300)
- **THEN** 圆心固定在 (400, 300)，不随玩家移动

#### Scenario: 最大半径
- **WHEN** 屏幕 800×600
- **THEN** max_radius = sqrt(800² + 600²) / 2 = 500px

### Requirement: 过渡期间输入屏蔽
过渡期间 SHALL 屏蔽所有游戏输入。

#### Scenario: IRIS_CLOSE 期间按键
- **WHEN** 状态为 IRIS_CLOSE，玩家按 WASD
- **THEN** 输入被忽略

#### Scenario: IDLE 后恢复
- **WHEN** 状态回到 IDLE
- **THEN** 正常处理输入
