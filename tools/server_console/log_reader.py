# -*- coding: utf-8 -*-
"""日志文件实时读取模块。

后台线程持续 tail 服务日志文件，支持文件轮转检测和缓存。
"""

import logging
import os
import threading
import time
from collections import deque
from typing import Callable, Optional

from .constants import LOG_CACHE_MAX_LINES, LOG_FILE_TEMPLATE, LOG_RETRY_INTERVAL_SEC

logger = logging.getLogger(__name__)


class LogReader:
    """日志文件实时读取器。

    后台线程持续读取指定日志文件的新行，写入 deque 缓存，
    并通过回调通知 GUI 更新。

    Attributes:
        name: 服务名称
        lines: 日志行缓存（deque，最大 10000 行）
    """

    def __init__(self, name: str, on_new_line: Optional[Callable[[str], None]] = None) -> None:
        """初始化日志读取器。

        Args:
            name: 服务名称（如 dbmgr, game_server, gate_server）
            on_new_line: 新行到达时的回调函数（GUI 线程中调用）
        """
        self.name = name
        self.lines: deque[str] = deque(maxlen=LOG_CACHE_MAX_LINES)
        self._on_new_line = on_new_line
        self._log_file = LOG_FILE_TEMPLATE.format(name)
        self._stop_event = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._file_pos: int = 0
        self._current_inode: Optional[int] = None

    def start(self) -> None:
        """启动后台读取线程。"""
        if self._thread is not None and self._thread.is_alive():
            return

        self._stop_event.clear()
        self._thread = threading.Thread(
            target=self._read_loop,
            name=f"LogReader-{self.name}",
            daemon=True,
        )
        self._thread.start()
        logger.info(f"LogReader.start, thread started, {self.name=}")

    def stop(self) -> None:
        """停止后台读取线程。"""
        self._stop_event.set()
        if self._thread is not None:
            self._thread.join(timeout=3)
            self._thread = None
        logger.info(f"LogReader.stop, thread stopped, {self.name=}")

    def get_all_lines(self) -> list[str]:
        """获取缓存中的所有日志行。

        Returns:
            日志行列表
        """
        return list(self.lines)

    def _get_file_inode(self, filepath: str) -> Optional[int]:
        """获取文件的 inode（用于轮转检测）。

        Windows 上使用文件索引号，Linux/Mac 使用 st_ino。
        """
        try:
            stat = os.stat(filepath)
            # Windows: 使用 file index 作为 inode 替代
            return stat.st_ino if hasattr(stat, "st_ino") else stat.st_dev
        except OSError:
            return None

    def _read_loop(self) -> None:
        """后台读取主循环。"""
        while not self._stop_event.is_set():
            if not os.path.exists(self._log_file):
                # 文件不存在，等待重试
                time.sleep(LOG_RETRY_INTERVAL_SEC)
                continue

            try:
                self._tail_file()
            except Exception as e:
                logger.error(f"LogReader._read_loop, error, {self.name=}, {e}")
                time.sleep(LOG_RETRY_INTERVAL_SEC)

    def _tail_file(self) -> None:
        """持续读取文件新增内容。"""
        current_inode = self._get_file_inode(self._log_file)

        # 检测文件轮转：inode 变化或文件大小变小
        if self._current_inode is not None and current_inode != self._current_inode:
            logger.info(f"LogReader._tail_file, file rotated, {self.name=}")
            self._file_pos = 0
        elif os.path.getsize(self._log_file) < self._file_pos:
            logger.info(f"LogReader._tail_file, file truncated, {self.name=}")
            self._file_pos = 0

        self._current_inode = current_inode

        with open(self._log_file, "r", encoding="utf-8", errors="replace") as f:
            f.seek(self._file_pos)
            while not self._stop_event.is_set():
                line = f.readline()
                if line:
                    line = line.rstrip("\n\r")
                    self.lines.append(line)
                    if self._on_new_line is not None:
                        self._on_new_line(line)
                    self._file_pos = f.tell()
                else:
                    # 没有新行，短暂等待
                    time.sleep(0.2)
                    # 检查文件是否被轮转
                    new_inode = self._get_file_inode(self._log_file)
                    if new_inode != current_inode:
                        break
