# Task: scene-transition

## 1. 过渡状态机
- [已完成] 状态定义: IDLE, IRIS_CLOSE, SWITCHING, IRIS_OPEN
  - 文件: `scripts/client/scene/scene_transition.py` TransitionState 类
- [已完成] 初始状态 IDLE
  - SceneTransition.__init__ 设置 _state = TransitionState.IDLE
- [已完成] start() 切换到 IRIS_CLOSE
  - 记录圆心位置和最大半径
- [已完成] 状态自动推进 update(dt)
  - IDLE -> IRIS_CLOSE -> SWITCHING -> IRIS_OPEN -> IDLE

## 2. Iris 收缩阶段 (300ms)
- [已完成] IRIS_DURATION = 0.3 (300ms)
- [已完成] 每帧 radius 减少 max_radius * dt / 0.3
  - 文件: `scripts/client/scene/scene_transition.py` update() IRIS_CLOSE 分支
- [已完成] radius <= 0 时切换到 SWITCHING

## 3. 场景切换阶段 (1帧)
- [已完成] SWITCHING 状态执行 switch_callback
  - SceneManager._execute_switch 作为回调
  - 替换 TileMap、更新场景 ID
- [已完成] 切换期间 radius=0，遮罩完全覆盖（全黑）

## 4. Iris 展开阶段 (300ms)
- [已完成] 每帧 radius 增加 max_radius * dt / 0.3
  - 文件: `scripts/client/scene/scene_transition.py` update() IRIS_OPEN 分支
- [已完成] radius >= max_radius 时切换到 IDLE

## 5. 圆形遮罩渲染
- [已完成] SRCALPHA Surface 创建
  - 文件: `scripts/client/scene/scene_transition.py` apply() 方法
  - 创建带 alpha 通道的 Surface，填充 (0,0,0,255)
- [已完成] 透明圆形绘制
  - pygame.draw.circle(mask, (0,0,0,0), center, radius)
- [已完成] 圆心固定在玩家启动时的屏幕位置
  - start() 记录 player_screen_pos，后续不随玩家移动
- [已完成] max_radius = sqrt(width^2 + height^2) / 2

## 6. 过渡期间输入屏蔽
- [已完成] 移动输入屏蔽
  - 文件: `scripts/client/game_scene.py` _handle_input() 检查 is_input_blocked()
- [已完成] 鼠标点击交互屏蔽
  - 文件: `scripts/client/game_scene.py` _handle_mouse_click()
  - 在方法开头增加 is_input_blocked() 检查，过渡期间跳过鼠标交互处理
