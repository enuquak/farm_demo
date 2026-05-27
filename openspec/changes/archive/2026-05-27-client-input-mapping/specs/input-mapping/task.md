# Input Mapping Task List

## 1. InputManager Action Map 抽象层
- [已完成 ✅] 1.1 创建 `input_manager.py` 模块，定义 InputManager 类
- [已完成 ✅] 1.2 实现硬编码 Action Map，将物理键映射到语义动作 (move_up, move_down, move_left, move_right, interact, open_inventory, hotbar_1~hotbar_0)
- [已完成 ✅] 1.3 实现 `is_action_pressed(action_name)` 方法，查询动作是否被按下
- [已完成 ✅] 1.4 在 GameScene 中集成 InputManager，替换原始 key.get_pressed() 逻辑

## 2. 鼠标状态追踪
- [已完成 ✅] 2.1 InputManager 追踪鼠标位置 `mouse_pos`
- [已完成 ✅] 2.2 InputManager 追踪鼠标按键状态 `mouse_pressed` 和 `mouse_just_pressed`
- [已完成 ✅] 2.3 InputManager.update() 方法每帧更新鼠标状态

## 3. 鼠标点击交互
- [已完成 ✅] 3.1 实现鼠标左键点击 tile 的交互逻辑（屏幕坐标转 tile 坐标）
- [已完成 ✅] 3.2 实现切比雪夫距离校验（interactRange）
- [已完成 ✅] 3.3 实现 interactRange 默认值（ITEM_EFFECTS 中未声明时默认为 1，-1 表示无限制）
- [已完成 ✅] 3.4 在 GameScene 中集成鼠标点击交互，发送 ItemUseReq

## 4. UI 层屏蔽
- [已完成 ✅] 4.1 实现 UI 层屏蔽机制，背包面板或弹窗打开时不触发游戏交互
