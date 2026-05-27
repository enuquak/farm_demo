"""
游戏内时钟核心模块
服务端以 30 分钟为粒度推进游戏时间（现实 60 秒 = 游戏 30 分钟）
"""
import logging
from typing import Callable, Optional, Dict, Any

logger = logging.getLogger("server.game_clock")


class GameClock:
    """
    游戏内时钟

    时间模型:
    - time_slot: 0~39 表示当天进度，每 slot = 30 分钟游戏时间
    - 60 秒现实时间推进 1 个 slot
    - slot 0 = AM 6:00, slot 12 = PM 12:00, slot 36 = AM 0:00
    - slot 40 触发 on_day_end 回调
    """

    # 每个 slot 对应的现实秒数
    SLOT_DURATION = 60.0

    # 一天的 slot 总数
    SLOTS_PER_DAY = 40

    def __init__(self):
        """初始化时钟（第 1 天 AM 6:00）"""
        self.day: int = 1
        self.time_slot: int = 0  # 0~39
        self.elapsed: float = 0.0  # 距离上次 slot 跳变的秒数
        self.paused: bool = False

        # 回调函数
        self._on_new_slot: Optional[Callable[[int], None]] = None
        self._on_day_end: Optional[Callable[[], None]] = None

        # 标记 on_day_end 是否已触发（避免重复触发）
        self._day_end_triggered: bool = False

    def set_on_new_slot(self, callback: Callable[[int], None]):
        """注册新 slot 回调"""
        self._on_new_slot = callback

    def set_on_day_end(self, callback: Callable[[], None]):
        """注册一天结束回调"""
        self._on_day_end = callback

    def update(self, dt: float):
        """
        更新时钟（每帧调用）

        Args:
            dt: 距离上一帧的时间（秒）
        """
        if self.paused:
            return

        self.elapsed += dt

        # 检查是否推进一个 slot
        while self.elapsed >= self.SLOT_DURATION:
            self.elapsed -= self.SLOT_DURATION
            self.time_slot += 1

            if self.time_slot >= self.SLOTS_PER_DAY:
                # 一天结束
                if not self._day_end_triggered:
                    self._day_end_triggered = True
                    logger.info(f"[GameClock]Day {self.day} ended (slot={self.time_slot})")
                    if self._on_day_end:
                        self._on_day_end()
                # 不再继续推进，等待外部处理
                self.time_slot = self.SLOTS_PER_DAY
                self.elapsed = 0.0
                return
            else:
                logger.debug(f"[GameClock]New slot: day={self.day}, slot={self.time_slot}, "
                           f"time={self.display_time}")
                if self._on_new_slot:
                    self._on_new_slot(self.time_slot)

    @property
    def display_time(self) -> str:
        """
        返回格式化的时间字符串

        Returns:
            如 "AM 6:00", "PM 12:00", "AM 0:00"
        """
        raw_hour = 6 + self.time_slot // 2
        hour = raw_hour % 24
        minute = (self.time_slot % 2) * 30
        period = "AM" if hour < 12 else "PM"
        display_hour = hour if hour in (0, 12) else hour % 12
        return f"{period} {display_hour}:{minute:02d}"

    def pause(self):
        """暂停时钟"""
        if not self.paused:
            self.paused = True
            logger.info(f"[GameClock]Paused at day={self.day}, slot={self.time_slot}")

    def resume(self):
        """恢复时钟"""
        if self.paused:
            self.paused = False
            logger.info(f"[GameClock]Resumed at day={self.day}, slot={self.time_slot}")

    def advance_day(self):
        """
        推进到新的一天（由外部调用，在强制睡觉流程完成后）
        """
        self.day += 1
        self.time_slot = 0
        self.elapsed = 0.0
        self._day_end_triggered = False
        logger.info(f"[GameClock]Advanced to day {self.day}")

    def serialize(self) -> Dict[str, Any]:
        """
        序列化时钟状态

        Returns:
            包含 day, time_slot, elapsed 的字典
        """
        return {
            "day": self.day,
            "time_slot": self.time_slot,
            "elapsed": self.elapsed,
        }

    @classmethod
    def deserialize(cls, data: Dict[str, Any]) -> 'GameClock':
        """
        从字典反序列化时钟状态

        Args:
            data: 包含 day, time_slot, elapsed 的字典

        Returns:
            GameClock 实例
        """
        clock = cls()
        clock.day = data.get("day", 1)
        clock.time_slot = data.get("time_slot", 0)
        clock.elapsed = data.get("elapsed", 0.0)
        return clock

    def to_sync_proto(self) -> Dict[str, int]:
        """
        返回用于 ClockSync 消息的数据

        Returns:
            包含 day, time_slot, paused 的字典
        """
        return {
            "day": self.day,
            "time_slot": self.time_slot,
            "paused": self.paused,
        }
