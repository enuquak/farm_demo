## Purpose

客户端玩家移动系统：实现 WASD 键盘控制的自由像素级角色移动，支持四向朝向、客户端预测 + 服务器权威的位置同步架构，以及相机跟随。

## Requirements

### Requirement: 键盘控制移动
玩家 SHALL 能通过 WASD 或方向键控制角色在地图上自由移动。

#### Scenario: 单方向移动
- **WHEN** 玩家按住 W 键
- **THEN** 角色以 SPEED 像素/秒的速度向上移动

#### Scenario: 对角线移动归一化
- **WHEN** 玩家同时按住 W 和 D 键
- **THEN** 角色沿 45 度方向移动，速度与单方向一致（向量归一化）

#### Scenario: 无输入时静止
- **WHEN** 玩家未按任何方向键
- **THEN** 角色保持静止

#### Scenario: 帧率无关
- **WHEN** 游戏以 30 FPS 和 60 FPS 分别运行
- **THEN** 角色在两种帧率下的移动速度 SHALL 一致（使用 deltaTime）

### Requirement: 地图边界碰撞
角色 SHALL 不能移动到地图边界之外。

#### Scenario: 移动到左边界
- **WHEN** 角色位于 x=5，试图向左移动 10 像素
- **THEN** 角色停在 x=0，不超出地图左边界

#### Scenario: 移动到右边界
- **WHEN** 角色位于地图右边界附近，试图向右移动
- **THEN** 角色停在 mapPixelW - playerWidth

### Requirement: 四向朝向
角色 SHALL 维护朝向状态（up/down/left/right），移动时自动更新。

#### Scenario: 向右移动更新朝向
- **WHEN** 角色正在向上移动，玩家按下 D 键向右移动
- **THEN** 角色朝向更新为 'right'

#### Scenario: 朝向影响渲染
- **WHEN** 角色朝向为 'left'
- **THEN** 渲染时角色面朝左侧（朝向指示器显示在左侧）

### Requirement: 角色渲染
角色 SHALL 使用 PyGame 绘制，支持纯色占位和朝向指示。

#### Scenario: 角色绘制
- **WHEN** 角色位于像素坐标 (960, 800)
- **THEN** 在屏幕对应位置绘制 TILE_SIZE×TILE_SIZE 的纯色方块

#### Scenario: 朝向指示
- **WHEN** 角色朝向为 'down'
- **THEN** 在角色方块底部绘制小三角箭头指示朝向

### Requirement: 客户端位置预测
客户端 SHALL 在按键后立即本地移动角色，同时发送位置更新给服务器。

#### Scenario: 本地即时移动
- **WHEN** 玩家按下 W 键
- **THEN** 角色立即在本地向上移动，不等待服务器确认

#### Scenario: 定期发送位置更新
- **WHEN** 角色在移动中
- **THEN** 客户端每 100ms 发送一次 PositionUpdate 消息（包含 x, y, direction, timestamp）

#### Scenario: 静止时不发包
- **WHEN** 角色静止不动
- **THEN** 客户端不发送 PositionUpdate 消息（节省带宽）

### Requirement: 服务器位置验证
Game Server SHALL 验证客户端上报的位置合法性。

#### Scenario: 位置合法
- **WHEN** 服务器收到 PositionUpdate，位移距离 ≤ speed × elapsed × 1.1
- **THEN** 更新服务端 Player 数据，不发送纠正消息

#### Scenario: 位置异常（瞬移）
- **WHEN** 服务器收到 PositionUpdate，位移距离 > speed × elapsed × 1.1
- **THEN** 发送 PositionCorrect 消息，包含正确的 x, y, direction

#### Scenario: 目标位置不可通行
- **WHEN** 服务器收到 PositionUpdate，目标位置在水面 tile 上
- **THEN** 发送 PositionCorrect 消息，将玩家位置回退到合法位置

### Requirement: 位置纠正
客户端 SHALL 在收到服务器的 PositionCorrect 时平滑纠正位置。

#### Scenario: 收到位置纠正
- **WHEN** 客户端收到 PositionCorrect (x=500, y=300)
- **THEN** 角色在 200ms 内平滑插值（lerp）到 (500, 300)

#### Scenario: 纠正期间输入正常
- **WHEN** 角色正在平滑纠正位置，玩家按方向键
- **THEN** 玩家输入仍然生效，纠正和输入叠加
