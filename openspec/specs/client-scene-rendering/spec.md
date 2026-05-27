## Purpose

客户端场景渲染系统：基于 pytmx + pyscroll 实现瓦片地图渲染，支持玩家精灵动画、摄像机跟随和游戏内 HUD 显示。

## Requirements

### Requirement: 瓦片地图渲染
系统 SHALL 使用 pytmx + pyscroll 渲染 16×16 像素的瓦片地图，4x 整数缩放。

#### Scenario: 加载并渲染地图
- **WHEN** 进入 PLAYING 状态
- **THEN** 加载 .tmx 地图文件，渲染地面层（草地/泥地/石径）和物体层（树木/围栏/建筑）

#### Scenario: 视口裁剪
- **WHEN** 地图尺寸大于屏幕可见区域
- **THEN** 仅渲染可见范围内的瓦片，不渲染屏幕外的瓦片

#### Scenario: 图层遮挡
- **WHEN** 物体层的瓦片（如树木、屋顶）位于玩家上方
- **THEN** 这些瓦片渲染在玩家精灵之上，形成正确的遮挡关系

### Requirement: 玩家精灵渲染
系统 SHALL 渲染玩家角色精灵，支持 4 方向行走动画。

#### Scenario: 初始显示玩家
- **WHEN** 进入 PLAYING 状态
- **THEN** 在 PlayerData 指定的 (pos_x, pos_y) 位置渲染玩家精灵，默认面朝下

#### Scenario: 行走动画
- **WHEN** 玩家移动中
- **THEN** 播放对应方向的行走动画帧（4 帧循环），动画速度与移动速度匹配

#### Scenario: 停止时显示静止帧
- **WHEN** 玩家停止移动
- **THEN** 显示当前朝向的静止帧（单帧），停止行走动画

### Requirement: 玩家移动
系统 SHALL 支持使用 WASD 或方向键控制玩家在地图上移动。

#### Scenario: 按键移动
- **WHEN** 用户按下 W/A/S/D 或方向键
- **THEN** 玩家向对应方向移动，移动速度为每秒 N 个瓦片（可配置）

#### Scenario: 碰撞检测
- **WHEN** 玩家移动目标位置位于碰撞层（collision layer）的瓦片上
- **THEN** 玩家不能移动到该位置，保持原位

#### Scenario: 地图边界限制
- **WHEN** 玩家移动到地图边界外
- **THEN** 玩家不能移出地图范围，停在边界处

#### Scenario: 斜向移动
- **WHEN** 用户同时按下两个方向键（如 W + D）
- **THEN** 玩家沿对角线移动，动画显示最后按下的方向

### Requirement: 摄像机跟随
系统 SHALL 让摄像机跟随玩家移动，保持玩家在屏幕中央。

#### Scenario: 摄像机跟随玩家
- **WHEN** 玩家移动
- **THEN** 摄像机平滑跟随（lerp 插值），玩家始终在屏幕中央附近

#### Scenario: 地图边界钳制
- **WHEN** 玩家靠近地图边缘
- **THEN** 摄像机停止滚动，不显示地图外的空白区域

### Requirement: 游戏内 HUD
系统 SHALL 在游戏场景上方叠加显示 HUD 信息。

#### Scenario: 显示玩家信息
- **WHEN** 处于 PLAYING 状态
- **THEN** HUD 显示角色名（role_name）、当前坐标（x, y）、场景名（scene_id）

#### Scenario: HUD 不遮挡游戏
- **WHEN** HUD 渲染在游戏画面上方
- **THEN** HUD 使用半透明背景或位于屏幕角落，不遮挡核心游戏区域
