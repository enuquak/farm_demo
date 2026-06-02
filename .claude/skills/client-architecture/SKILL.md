---
name: client-architecture
description: Python 客户端架构：PyGame 客户端、网络层、场景管理、玩家控制器、渲染协调、UI 系统。
metadata:
  type: reference
---

## 概述

Farm Demo 客户端基于 Python/PyGame 实现，采用独立网络线程与 PyGame 主循环分离的架构。客户端通过 TCP 长连接与 Gate 服务器通信，使用 Protobuf 序列化消息。核心子系统包括：网络连接层、场景渲染系统、玩家移动控制器、输入映射系统、UI 系统（背包/快捷栏）和登录界面。

## 架构设计

### PyGame 主循环 + 独立网络线程 + 线程安全队列

客户端采用双线程架构：

- **PyGame 主循环**：负责输入采集、游戏逻辑更新、渲染绘制。每帧从接收队列取出所有待处理消息，分发给对应的游戏逻辑处理器。
- **网络线程**：连接建立后启动独立线程，负责消息收发和心跳维持。接收到的消息放入线程安全的接收队列，主循环发送的消息通过发送队列异步发出。
- **线程安全队列**：网络线程与主循环之间通过线程安全队列（queue.Queue）交换数据，避免竞态条件。

### GateConnection（TCP 长连接）

客户端与 Gate 服务器建立 TCP 长连接，核心特性：

- **Protobuf 消息协议**：使用长度前缀解决粘包/拆包问题，消息格式为 `Packet(msg_id + payload)`
- **心跳机制**：每 5 秒发送一次 Heartbeat 消息（含 timestamp），超过 15 秒未收到响应则标记连接断开
- **连接状态管理**：维护三种状态（连接中、已连接、已断开），提供状态查询接口和异常回调
- **登录流程**：发送 LoginReq(token) -> 收到 LoginResp(code, msg) -> 查询角色列表 -> 自动创建或进入游戏

### NetworkDispatcher（dict 映射 MsgID -> 回调）

消息分发器维护一个 MsgID 到回调函数的映射表（dict），主循环每帧从接收队列取消息后，根据 msg_id 查找并调用对应的处理函数。新消息类型只需注册映射即可扩展。

## 关键流程

### 玩家控制器

- **WASD 输入**：通过 InputManager 的 Action Map 抽象层，将物理按键（W/A/S/D、方向键）映射为语义动作（move_up/move_down/move_left/move_right）。对角线移动时进行向量归一化，保证各方向速度一致。使用 deltaTime 实现帧率无关移动。
- **客户端预测**：按键后角色立即在本地移动（不等待服务器确认），实现低延迟手感。
- **100ms 位置更新**：角色移动中每 100ms 发送一次 PositionUpdate 消息（含 x, y, direction, timestamp），静止时不发包以节省带宽。
- **服务器验证**：服务器校验位移距离是否超过 `speed * elapsed * 1.1`，异常时发送 PositionCorrect 消息。
- **200ms lerp 修正**：收到 PositionCorrect 后，角色在 200ms 内平滑插值到正确位置，修正期间玩家输入仍然生效（纠正和输入叠加）。
- **四向朝向**：维护 up/down/left/right 朝向状态，移动时自动更新，影响精灵动画帧选择。
- **碰撞检测**：地图碰撞层（collision layer）阻止移动，地图边界硬限制。

### 场景管理

- **SceneManager**：管理游戏场景的切换，支持从登录界面到游戏场景的状态流转。
- **TMX 地图**：使用 pytmx 加载 .tmx 地图文件，pyscroll 负责渲染。地图基于 16x16 像素瓦片，4x 整数缩放。分为地面层（草地/泥地/石径）和物体层（树木/围栏/建筑）。
- **图层遮挡**：物体层中位于玩家上方的瓦片（如树木、屋顶）渲染在玩家精灵之上，形成正确的遮挡关系。
- **Iris 过渡动画**：场景切换时使用 Iris（虹膜）过渡效果，从中心向外扩展或收缩。

### 渲染协调

- **GameRenderer**：协调各层渲染顺序，负责将游戏世界渲染到屏幕。
- **双层渲染**：底层渲染瓦片地图地面层，中层渲染玩家精灵，顶层渲染物体层遮挡物和 HUD。
- **摄像机 lerp + 边界约束**：摄像机平滑跟随玩家（lerp 插值），靠近地图边缘时停止滚动，不显示地图外的空白区域。
- **玩家精灵**：支持 4 方向行走动画（4 帧循环），动画速度与移动速度匹配，停止时显示当前朝向的静止帧。
- **HUD 叠加**：在游戏画面上方显示角色名、坐标、场景名等信息，使用半透明背景避免遮挡核心游戏区域。

### 输入系统

- **Action Map**：InputManager 维护硬编码的动作映射表，支持多键映射到同一动作。定义的动作包括：move_up/down/left/right、interact（空格）、open_inventory（E 键）、hotbar_1~hotbar_0（数字键 1-0）。
- **鼠标状态追踪**：追踪鼠标位置（mouse_pos）、按键状态（mouse_pressed）和帧级按下检测（mouse_just_pressed）。
- **屏幕坐标转世界坐标**：Camera 支持 screen_to_world() 和 screen_to_tile() 转换，用于鼠标点击交互。
- **交互距离校验**：使用切比雪夫距离校验交互范围（默认 range=1 即九宫格），range=-1 表示无距离限制。

### UI 系统

- **快捷栏**：屏幕底部固定显示 10 格快捷栏，每格 TILE_SIZE x TILE_SIZE。物品使用 8x8 像素图标拉伸到 32x32，同种物品共享图标 Surface 缓存。数字键 1-0 切换选中格（activeSlot），选中格显示高亮边框，切换时发送 ActiveSlotChange 消息。
- **背包面板**：按 E 键打开/关闭，居中显示 10x3 网格（第 1 行为快捷栏，第 2-3 行为扩展背包）。面板打开时暂停角色移动并屏蔽 WASD、空格、鼠标交互输入。
- **数据同步**：根据服务器 InventorySync 消息更新 UI，仅更新变化的 slot 而非全量刷新。

### 登录界面

- **登录流程**：显示 "Farm Demo" 标题、账号 ID 输入框、"Enter Game" 按钮。支持回车和按钮点击触发登录。
- **自动角色管理**：登录成功后查询角色列表，无角色则自动创建（role_name = account_id），有角色则直接进入游戏。
- **状态反馈**：登录流程执行期间显示 "连接中..." 加载提示，禁用输入框和按钮。
- **错误处理**：连接失败、登录失败、创建角色失败、进入游戏失败、网络超时（10 秒）均显示错误信息并返回登录界面允许重试。

## 关键代码路径

- `main.py` — 客户端入口，PyGame 主循环初始化、状态机驱动
- `connection.py` / `gate_connection.py` — GateConnection 实现，TCP 连接、Protobuf 收发、心跳、网络线程
- `network_dispatcher.py` — NetworkDispatcher，MsgID -> 回调的 dict 映射表
- `scene/` — 场景管理目录，包含 SceneManager、各场景状态（LoginScene、PlayingScene 等）
- `ui/` — UI 组件目录，包含快捷栏（Hotbar）、背包面板（InventoryPanel）、HUD 等
- `input_manager.py` — InputManager，Action Map、鼠标状态追踪
- `player_controller.py` — 玩家移动控制，客户端预测、位置同步、碰撞检测
- `camera.py` — Camera，跟随逻辑、坐标转换、边界约束
- `renderer.py` / `game_renderer.py` — GameRenderer，双层渲染协调

## 常见陷阱

### 线程安全问题

网络线程和 PyGame 主循环共享数据时必须使用线程安全队列。直接在主循环中调用网络操作（如阻塞式 recv）会导致游戏卡顿。同样，网络线程不应直接修改游戏状态对象，必须通过队列传递。

### 网络断开未重连

心跳超时（15 秒）后连接标记为断开，但当前实现可能缺少自动重连逻辑。断开后应通知上层并提供重连入口，而不是静默失败。

### 位置预测抖动

客户端预测与服务器验证之间的延迟差异可能导致位置抖动。收到 PositionCorrect 时使用 200ms lerp 平滑过渡，但如果纠正频繁发生，玩家会感觉角色在"漂移"。需要确保服务器验证阈值（speed * elapsed * 1.1）足够宽容，避免正常移动被误判为异常。

### UI 层未阻塞游戏输入

背包面板打开时必须屏蔽 WASD 移动、空格交互和鼠标点击交互。如果 InputManager 未正确检查 UI 层状态，玩家在操作背包时角色会同时移动，导致体验混乱。需要在输入处理链的入口处统一检查 UI 遮挡状态。

### 对角线移动未归一化

同时按下两个方向键时，如果不对速度向量进行归一化，对角线移动速度会是单方向的 sqrt(2) 倍（约 1.41 倍），造成移动速度不一致。

## 扩展指南

### 添加新 UI 组件

1. 在 `ui/` 目录下创建新的 UI 组件类
2. 实现 `update()` 和 `draw()` 方法
3. 在 InputManager 中注册相关按键动作（如需要）
4. 在 GameRenderer 的渲染链中添加绘制调用
5. 确保组件打开时在输入处理入口处屏蔽游戏交互输入
6. 如需与服务器同步，注册对应的 MsgID 回调到 NetworkDispatcher

### 添加新消息处理器

1. 在 Protobuf 定义中添加新的消息类型和 MsgID
2. 在 NetworkDispatcher 的映射表中注册 MsgID -> 回调函数
3. 实现回调函数处理消息逻辑（注意：回调在主循环线程执行，可直接修改游戏状态）
4. 如需发送新消息，通过发送队列提交 Protobuf 序列化后的消息

## 相关 Skill

- [[scene-system]] — 场景系统详细设计
- [[item-interaction]] — 物品交互系统
