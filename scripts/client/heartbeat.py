"""
心跳管理模块
定期发送心跳包，检测连接活性
"""
import threading
import time
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .connection import GateConnection


class HeartbeatManager:
    """
    心跳管理器
    定期发送心跳包，监控连接状态
    """

    # 心跳间隔（秒）
    HEARTBEAT_INTERVAL = 5.0
    # 心跳超时（秒）
    HEARTBEAT_TIMEOUT = 15.0

    def __init__(self, connection: 'GateConnection'):
        """
        初始化心跳管理器

        Args:
            connection: GateConnection 实例
        """
        self._connection = connection
        self._thread: threading.Thread = None
        self._stop_event = threading.Event()
        self._last_heartbeat_response = time.time()
        self._lock = threading.Lock()

    def start(self):
        """启动心跳定时器"""
        if self._thread and self._thread.is_alive():
            return

        self._stop_event.clear()
        self._last_heartbeat_response = time.time()

        self._thread = threading.Thread(
            target=self._heartbeat_loop,
            name="HeartbeatThread",
            daemon=True
        )
        self._thread.start()

    def stop(self):
        """停止心跳定时器"""
        self._stop_event.set()
        if self._thread and self._thread.is_alive() and self._thread is not threading.current_thread():
            self._thread.join(timeout=2.0)
        self._thread = None

    def update_response_time(self):
        """更新最后心跳响应时间"""
        with self._lock:
            self._last_heartbeat_response = time.time()

    def get_last_response_time(self) -> float:
        """获取最后心跳响应时间"""
        with self._lock:
            return self._last_heartbeat_response

    def _heartbeat_loop(self):
        """心跳主循环"""
        last_send_time = 0

        while not self._stop_event.is_set():
            current_time = time.time()

            # 检查是否需要发送心跳
            if current_time - last_send_time >= self.HEARTBEAT_INTERVAL:
                try:
                    self._connection.send_heartbeat()
                    last_send_time = current_time
                except Exception as e:
                    # 发送失败，连接可能已断开
                    break

            # 检查心跳超时
            time_since_last_response = current_time - self.get_last_response_time()
            if time_since_last_response > self.HEARTBEAT_TIMEOUT:
                # 心跳超时，标记连接断开
                self._connection._handle_disconnect(
                    f"心跳超时: {time_since_last_response:.1f}秒未收到响应"
                )
                break

            # 短暂休眠
            time.sleep(1.0)
