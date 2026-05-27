## Context

Farm Demo 需要游戏内时钟来驱动"一天"的节奏。在客户端-服务器架构下，时钟由服务端权威推进，客户端负责 HUD 展示。

已有基础设施可复用：
- **SceneManager**：支持场景切换 + 过渡动画
- **Energy**：体力系统，已有 current/max/serialize/deserialize
- **House 场景**：已有 BED 家具
- **存档系统**：DBMgr 持久化

## Goals / Non-Goals

**Goals:**
- 建立游戏内时钟，30 分钟粒度跳变（现实 1 分钟 = 游戏 30 分钟）
- 左上角 HUD 显示当前时间和天数
- AM 2:00 到达时强制触发睡觉流程（Iris 黑屏 → 传送 House → 体力恢复 50% → 新一天）
- 背包/弹窗打开时时间暂停
- 存档保存/恢复时钟状态

**Non-Goals:**
- 天色渐变（傍晚变橙、夜晚变暗）— 后续版本
- 主动睡觉（玩家与床交互提前结束一天）— 后续版本
- 熬夜体力加速消耗 — 后续版本
- 时间影响作物生长速度 — 后续版本
- 季节/天气系统 — 后续版本

## Decisions

### D1: 时间模型 — 离散 slot 而非连续计时

**选择**: 用 `time_slot` 整数 (0~39) 表示当天进度，每 slot = 30 分钟游戏时间。

```
slot 0  = AM  6:00    slot 20 = PM  4:00
slot 1  = AM  6:30    slot 24 = PM  6:00 (= 18:00)
slot 12 = PM 12:00    slot 36 = AM  0:00 (= 24:00)
slot 39 = AM  1:30    slot 40 → 触发强制睡觉
```

时间计算公式:
```python
raw_hour = 6 + time_slot // 2
hour = raw_hour % 24
minute = (time_slot % 2) * 30
period = "AM" if hour < 12 else "PM"
display = f"{period} {hour % 12 or 12}:{minute:02d}"
```

**理由**: 30 分钟跳变是需求本身。离散 slot 更简单，避免浮点精度问题，且便于未来按 slot 触发事件。

### D2: GameClock 作为服务端独立模块

**选择**: `scripts/server/game_clock.py` 是纯数据 + 计时器类，通过回调通知外部。

```python
class GameClock:
    def __init__(self):
        self.day = 1
        self.time_slot = 0  # 0~39
        self.elapsed = 0.0  # 距离上次 slot 跳变的秒数
        self.paused = False

    def update(self, dt):
        if self.paused:
            return
        self.elapsed += dt
        if self.elapsed >= 60.0:  # 60 秒 = 30 分钟游戏时间
            self.elapsed -= 60.0
            self.time_slot += 1
            if self.time_slot >= 40:
                self.on_day_end()
            else:
                self.on_new_slot(self.time_slot)

    def on_new_slot(self, slot):
        pass  # 由外部注册回调

    def on_day_end(self):
        pass  # 由外部注册回调

    def pause(self): self.paused = True
    def resume(self): self.paused = False

    @property
    def display_time(self) -> str:
        raw_hour = 6 + self.time_slot // 2
        hour = raw_hour % 24
        minute = (self.time_slot % 2) * 30
        period = "AM" if hour < 12 else "PM"
        return f"{period} {hour % 12 or 12}:{minute:02d}"

    def serialize(self) -> dict:
        return {"day": self.day, "time_slot": self.time_slot, "elapsed": self.elapsed}

    @classmethod
    def deserialize(cls, data: dict) -> 'GameClock':
        c = cls()
        c.day = data.get("day", 1)
        c.time_slot = data.get("time_slot", 0)
        c.elapsed = data.get("elapsed", 0.0)
        return c
```

**理由**: 解耦。GameClock 只管时间推进，强制睡觉的编排由外部完成。

### D3: 时钟同步协议

**选择**: 服务端定时广播时钟状态，客户端只做展示

```protobuf
// 时钟同步（服务器 → 客户端）
message ClockSync {
    int32 day = 1;
    int32 time_slot = 2;
    bool paused = 3;
}

// 每个 slot 跳变时发送一次
```

**理由**: 客户端不需要本地计时，只需根据服务器推送更新 HUD。避免客户端/服务端时钟不一致。

### D4: 强制睡觉流程

```
服务端 GameClock.on_day_end():
  1. 暂停时钟
  2. 发送 ForceSleepNotify 给客户端

客户端收到 ForceSleepNotify:
  3. 启动 Iris 黑屏过渡（复用 SceneTransition）
  4. 过渡完成后发送 ForceSleepReady 给服务器

服务端收到 ForceSleepReady:
  5. 冻结当前场景
  6. 切换到 House 场景，设置玩家位置到床旁
  7. 恢复体力 50%
  8. 推进天数 (day += 1, time_slot = 0)
  9. 发送 SceneChangeResp + ClockSync + EnergySync

客户端收到 SceneChangeResp:
  10. 切换到 House 场景渲染
  11. Iris 展开过渡
  12. 恢复游戏输入
```

### D5: 暂停机制

**选择**: 客户端在背包/弹窗打开时发送 PauseReq 给服务器，服务器暂停时钟。

```python
# 客户端
if inventory_panel.is_open or modal.is_open:
    network.send_pause_clock()
else:
    network.send_resume_clock()
```

**理由**: 服务器权威，暂停状态由服务器管理，避免不一致。

### D6: TimeHUD — 客户端 PyGame 渲染

**选择**: 左上角 PyGame 绘制时间显示

```python
class TimeHUD:
    def __init__(self):
        self.font = pygame.font.Font(None, 24)

    def render(self, surface, day, display_time):
        text = f"{display_time}  Day {day}"
        text_surface = self.font.render(text, True, (255, 255, 255))
        # 半透明背景
        bg = pygame.Surface((text_surface.get_width() + 16, 32), pygame.SRCALPHA)
        bg.fill((0, 0, 0, 128))
        surface.blit(bg, (8, 8))
        surface.blit(text_surface, (16, 14))
```

### D7: 存档格式升级

```json
{
    "clock": {"day": 1, "time_slot": 0, "elapsed": 0.0}
}
```

**向后兼容**: 旧存档无 clock 字段时默认第 1 天 AM 6:00。

### D8: 文件结构

```
scripts/server/
├── game_clock.py        # GameClock 服务端时钟核心

scripts/client/
├── ui/
│   └── time_hud.py      # TimeHUD 客户端时间显示
```

## Risks / Trade-offs

**[Risk] 强制睡觉时玩家在 farm 场景**
→ 需要先切到 house，Iris 过渡已支持跨场景切换，无额外风险。

**[Risk] 强制睡觉中途客户端断线**
→ 服务端已完成场景切换和天数推进，客户端重连后获取最新状态。
→ Mitigation: 重连时服务器下发完整状态（场景 + 时钟 + 能量）。

**[Trade-off] 时间跳变 vs 平滑**
→ 30 分钟跳变可能显得突兀，但符合需求且实现简单。未来可加入过渡动画。

**[Trade-off] 不做天色变化**
→ 玩家感知时间流逝主要靠左上角数字，沉浸感稍弱。但本版本优先功能完整性。
