# Task: player-movement

## 1. 添加 PositionUpdate / PositionCorrect Protobuf 消息定义
- [x] 1.1 在 `scripts/common/proto/player.proto` 中添加 `PositionUpdate` 和 `PositionCorrect` 消息 [已完成 ✅]
- [x] 1.2 重新编译生成 Python/C++ protobuf 代码 [已完成 ✅]
- [x] 1.3 在 `scripts/client/msg_ids.py` 中添加对应消息 ID 常量 [已完成 ✅]

## 2. 实现 Direction 方向系统
- [x] 2.1 在 `scripts/client/constants.py` 中添加 Direction 枚举 (up/down/left/right) [已完成 ✅]

## 3. 重构 GameScene 玩家移动（帧率无关 + 方向追踪 + 地图边界 + 水面碰撞）
- [x] 3.1 重构 `_handle_input()`：使用 deltaTime 实现帧率无关移动 [已完成 ✅]
- [x] 3.2 添加方向状态维护（根据移动方向自动更新 facing_direction） [已完成 ✅]
- [x] 3.3 添加水面 tile 不可通行碰撞检测 [已完成 ✅]
- [x] 3.4 确保地图边界碰撞正确（使用 TILE_SIZE 作为 playerWidth） [已完成 ✅]

## 4. 改进玩家渲染（朝向指示器）
- [x] 4.1 重写 `_render_player()`：绘制 TILE_SIZE x TILE_SIZE 纯色方块 + 朝向三角箭头 [已完成 ✅]

## 5. 实现客户端位置预测与定期发送
- [x] 5.1 实现 PositionUpdate 消息构建与发送（每 100ms 发送一次，静止时不发） [已完成 ✅]
- [x] 5.2 在主循环中集成位置更新发送逻辑 [已完成 ✅]

## 6. 实现服务器位置纠正处理
- [x] 6.1 实现 PositionCorrect 消息接收与解析 [已完成 ✅]
- [x] 6.2 实现平滑插值纠正（200ms lerp），纠正期间输入仍生效 [已完成 ✅]
