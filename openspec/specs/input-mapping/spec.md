## Purpose

输入映射系统：提供 Action Map 抽象层将物理按键归并为语义动作，支持鼠标状态追踪和屏幕→世界坐标转换，以及基于切比雪夫距离的交互范围校验。

## Requirements

### Requirement: Action Map 抽象层
InputManager SHALL 维护硬编码的 Action Map，将多个物理键映射到语义动作。

#### Scenario: 动作定义
- **WHEN** 系统初始化
- **THEN** 定义以下动作：move_up, move_down, move_left, move_right, interact, open_inventory, hotbar_1~hotbar_0

#### Scenario: 多键映射到同一动作
- **WHEN** Action Map 中 move_up 映射到 [K_w, K_UP]
- **THEN** 按下 W 或上方向键都触发 move_up 动作

#### Scenario: 查询动作状态
- **WHEN** 调用 `input.is_action_pressed("move_up")` 且 W 键被按住
- **THEN** 返回 True

#### Scenario: 查询未触发的动作
- **WHEN** 调用 `input.is_action_pressed("interact")` 且空格键未按下
- **THEN** 返回 False

### Requirement: 鼠标状态追踪
InputManager SHALL 追踪鼠标位置和按键状态。

#### Scenario: 鼠标位置
- **WHEN** 鼠标移动到屏幕坐标 (400, 300)
- **THEN** `input.mouse_pos` 更新为 (400, 300)

#### Scenario: 鼠标按键状态
- **WHEN** 鼠标左键被按下
- **THEN** `input.mouse_pressed[1]` 为 True

#### Scenario: 鼠标 justPressed
- **WHEN** 鼠标左键在本帧按下（上一帧未按下）
- **THEN** `input.mouse_just_pressed[1]` 为 True

### Requirement: 屏幕坐标转世界坐标
Camera SHALL 支持将屏幕像素坐标转换为世界像素坐标和 tile 坐标。

#### Scenario: 屏幕转世界像素
- **WHEN** 相机位于 (560, 500)，调用 `camera.screen_to_world(400, 300)`
- **THEN** 返回世界像素坐标 (960, 800)

#### Scenario: 屏幕转 tile
- **WHEN** 调用 `camera.screen_to_tile(400, 300)` 且 TILE_SIZE=32
- **THEN** 返回 tile 坐标 (30, 25)

### Requirement: 鼠标点击交互
玩家 SHALL 能通过鼠标左键点击世界中的 tile 触发物品交互。

#### Scenario: 鼠标左键点击有效区域
- **WHEN** 玩家在游戏画面上左键点击 tile (15, 10)
- **THEN** 客户端计算 tile 坐标，检查距离后发送 ItemUseReq

#### Scenario: 点击超出交互范围
- **WHEN** 玩家点击距离 > interactRange 的 tile
- **THEN** 不发送 ItemUseReq，忽略交互

#### Scenario: UI 层屏蔽
- **WHEN** 背包面板或弹窗打开时鼠标左键点击
- **THEN** 不触发游戏交互

### Requirement: 距离校验
系统 SHALL 使用切比雪夫距离校验交互范围。

#### Scenario: 九宫格范围（range=1）
- **WHEN** 玩家在 tile (10, 10)，目标 tile (11, 11)，interactRange=1
- **THEN** 切比雪夫距离 = max(|10-11|, |10-11|) = 1 ≤ 1，允许交互

#### Scenario: 超出九宫格
- **WHEN** 玩家在 tile (10, 10)，目标 tile (12, 10)，interactRange=1
- **THEN** 切比雪夫距离 = 2 > 1，拒绝交互

#### Scenario: 无限制范围（range=-1）
- **WHEN** 玩家在 tile (10, 10)，目标 tile (50, 50)，interactRange=-1
- **THEN** 不检查距离，允许交互

### Requirement: interactRange 默认值
ITEM_EFFECTS 中未声明 interactRange 的效果 SHALL 默认为 1。

#### Scenario: 默认范围
- **WHEN** ITEM_EFFECTS[item_id][target] 中无 interactRange 字段
- **THEN** 默认 interactRange=1（九宫格）

#### Scenario: 食物无限制
- **WHEN** ITEM_EFFECTS[6]["ANY"] 声明 interactRange=-1
- **THEN** 面包食用无距离限制
