# Task: time-hud

## 1. TimeHUD 客户端 UI
- [已完成] 1.1 创建 `scripts/client/ui/time_hud.py`，实现 TimeHUD 类
- [已完成] 1.2 实现渲染逻辑：左上角 (8,8) 白色 24px 字体 + 半透明黑色背景
- [已完成] 1.3 实现 update(day, time_slot) 方法
- [已完成] 1.4 在 game_scene.py 中集成 TimeHUD（创建、渲染、ClockSync 消息处理）
- [已完成] 1.5 处理 ClockSync 消息（解析 proto，更新 TimeHUD 显示）
