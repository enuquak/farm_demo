"""
统一日志初始化模块 (Python 客户端)
提供与 C++ 服务器一致的日志格式: [时间][级别][进程名:PID][模块][内容]
"""

import logging
import os
import sys
from datetime import datetime
from logging.handlers import RotatingFileHandler


class UnifiedFormatter(logging.Formatter):
    """
    统一日志格式器
    格式: [时间][级别][进程名:PID][模块][内容]
    时间格式: YYYY-MM-DD HH:MM:SS.mmm
    级别: 右对齐大写英文 (DEBUG/INFO/ERROR/CRITICAL)
    """

    def __init__(self, process_name="client"):
        super().__init__()
        self.process_name = process_name
        self.pid = os.getpid()

    def format(self, record):
        # 时间
        now = datetime.fromtimestamp(record.created)
        time_str = now.strftime("%Y-%m-%d %H:%M:%S.") + f"{int(now.microsecond / 1000):03d}"

        # 级别 (右对齐，5字符宽度)
        level_str = record.levelname.ljust(8)

        # 进程标识
        process_id = f"{self.process_name}:{self.pid}"

        # 模块 (从 record.name 获取)
        module = record.name.ljust(12)

        # 内容
        message = record.getMessage()

        return f"[{time_str}][{level_str}][{process_id}][{module}] {message}"


def init_logging(
    process_name="client",
    log_dir="./runtimeData/logs/client",
    level="info",
    max_file_size_mb=20,
    max_files=7,
    player_id=None,
):
    """
    初始化统一日志系统

    Args:
        process_name: 进程名称 (默认 "client")
        log_dir: 日志目录 (默认 "./runtimeData/logs/client")
        level: 最低日志级别 ("debug", "info", "error", "critical")
        max_file_size_mb: 单文件最大大小 (MB)
        max_files: 历史文件保留数量
        player_id: 玩家 ID (用于日志文件名)
    """
    # 确保日志目录存在
    os.makedirs(log_dir, exist_ok=True)

    # 构建日志文件名
    if player_id:
        log_file = os.path.join(log_dir, f"client_{player_id}.log")
    else:
        log_file = os.path.join(log_dir, f"{process_name}.log")

    # 解析日志级别
    level_map = {
        "debug": logging.DEBUG,
        "info": logging.INFO,
        "error": logging.ERROR,
        "critical": logging.CRITICAL,
    }
    log_level = level_map.get(level.lower(), logging.INFO)

    # 创建 logger
    logger = logging.getLogger()
    logger.setLevel(log_level)

    # 清除已有的 handler (避免重复添加)
    logger.handlers.clear()

    # 创建格式器
    formatter = UnifiedFormatter(process_name)

    # 文件 handler (按大小轮转)
    max_bytes = max_file_size_mb * 1024 * 1024
    file_handler = RotatingFileHandler(
        log_file,
        maxBytes=max_bytes,
        backupCount=max_files,
        encoding="utf-8",
    )
    file_handler.setLevel(log_level)
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)

    return logger


def shutdown_logging():
    """关闭日志系统"""
    logging.shutdown()
