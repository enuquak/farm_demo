# Task: forced-sleep

## 1. 客户端强制睡觉流程
- [已完成] 1.1 在 game_scene.py 中处理 ForceSleepNotify 消息（启动 Iris 收缩过渡）
- [已完成] 1.2 实现 Iris 收缩完成后发送 ForceSleepReady 给服务器
- [已完成] 1.3 处理强制睡觉的 SceneChangeResp（切换到 house 场景 + Iris 展开）

## 2. 服务端强制睡觉流程
- [已完成] 2.1 在 game_server.cpp 中实现 on_day_end 回调逻辑
- [已完成] 2.2 实现 ForceSleepReady handler（场景切换 + 体力恢复50% + 天数推进）
- [已完成] 2.3 发送 SceneChangeResp + ClockSync + EnergySync 给客户端
- [已完成] 2.4 超时处理：10秒未收到 ForceSleepReady 则强制完成
