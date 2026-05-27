# Task: game-clock

## 1. Proto 定义与消息 ID
- [已完成] 1.1 在 `scripts/common/proto/player.proto` 中添加 ClockSync、ForceSleepNotify、ForceSleepReady 消息定义
- [已完成] 1.2 在 `scripts/client/msg_ids.py` 中添加 MSG_ID_CLOCK_SYNC、MSG_ID_FORCE_SLEEP_NOTIFY、MSG_ID_FORCE_SLEEP_READY
- [已完成] 1.3 在 `scripts/common/proto/msg_ids.h` 中添加对应 C++ 消息 ID 常量
- [已完成] 1.4 重新生成 protobuf Python 代码

## 2. GameClock 服务端核心模块
- [已完成] 2.1 创建 `scripts/server/game_clock.py`，实现 GameClock 类基础结构（day, time_slot, elapsed, paused）
- [已完成] 2.2 实现 display_time 属性（时间格式化：AM/PM + hour:minute）
- [已完成] 2.3 实现 update(dt) 方法（帧率无关的时间推进，60秒=1个slot）
- [已完成] 2.4 实现 pause()/resume() 方法
- [已完成] 2.5 实现 on_new_slot(slot) 和 on_day_end() 回调机制
- [已完成] 2.6 实现 serialize()/deserialize() 方法（含旧存档兼容）

## 3. Game Server 集成
- [已完成] 3.1 在 game_server.cpp 的 update_game_logic() 中调用 GameClock.update()
- [已完成] 3.2 注册 ClockSync 广播逻辑（slot 跳变时发送给所有在线玩家）
- [已完成] 3.3 注册 ForceSleepReady 消息 handler
- [已完成] 3.4 实现 on_day_end 回调（暂停时钟 + 发送 ForceSleepNotify）
- [已完成] 3.5 实现 ForceSleepReady 处理（场景切换 + 体力恢复 + 天数推进 + 恢复时钟）
- [已完成] 3.6 在 EnterGameResp 中携带 ClockSync 数据
- [已完成] 3.7 在玩家数据持久化中保存/加载时钟状态

## 4. 缺陷修复（测试后）
- [已完成] 4.1 修复 BUG-001: display_time 午夜(hour=0)返回"AM 12:00"而非"AM 0:00"（game_clock.py + time_hud.py）
- [已完成] 4.2 修复 OBS-001: 时钟数据持久化到 DBMgr（save_clock_data/load_clock_data，在 shutdown 时保存，启动时加载）
- [已完成] 4.3 修复 OBS-002: 强制睡觉体力恢复使用 max_energy/2 替代硬编码 50
