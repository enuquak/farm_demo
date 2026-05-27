# -*- coding: utf-8 -*-
"""服务器进程管理模块。

提供 PID 文件读取、进程存活检测、启动/停止/重启服务等功能。
"""

import logging
import os
import subprocess
import time

from .constants import (
    PID_FILE_TEMPLATE,
    PROJECT_ROOT,
    SERVICE_NAMES,
    STARTUP_VERIFY_TIMEOUT_SEC,
    START_ARGS_TEMPLATE,
    START_CMD_TEMPLATE,
    STEP_INTERVAL_SEC,
    STOP_ORDER,
)

logger = logging.getLogger(__name__)


def read_pid(name: str) -> int | None:
    """读取服务的 PID 文件，返回 PID 整数或 None。

    Args:
        name: 服务名称（如 dbmgr, game_server, gate_server）

    Returns:
        PID 整数，文件不存在或读取失败返回 None
    """
    pid_file = PID_FILE_TEMPLATE.format(name)
    if not os.path.exists(pid_file):
        return None

    try:
        with open(pid_file, "r") as f:
            pid_str = f.read().strip()
        return int(pid_str)
    except (ValueError, OSError) as e:
        logger.error(f"read_pid, failed to read PID file, {name=}, {pid_file=}, {e}")
        return None


def is_process_alive(pid: int) -> bool:
    """通过 PID 检查进程是否存活（Windows 专用）。

    Args:
        pid: 进程 ID

    Returns:
        进程存活返回 True，否则返回 False
    """
    try:
        result = subprocess.run(
            ["tasklist", "/FI", f"PID eq {pid}", "/NH"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        # tasklist 输出中包含进程名则存活，否则输出 "No tasks"
        return str(pid) in result.stdout
    except (subprocess.TimeoutExpired, OSError) as e:
        logger.error(f"is_process_alive, check failed, {pid=}, {e}")
        return False


def get_service_status(name: str) -> bool:
    """获取服务运行状态。

    Args:
        name: 服务名称

    Returns:
        服务运行中返回 True，否则返回 False
    """
    pid = read_pid(name)
    if pid is None:
        return False
    return is_process_alive(pid)


def force_stop_service(name: str) -> bool:
    """强制终止单个服务进程。

    Args:
        name: 服务名称

    Returns:
        成功终止或服务未运行返回 True，终止失败返回 False
    """
    pid = read_pid(name)
    if pid is None:
        logger.info(f"force_stop_service, PID file not found, {name=}, skipping")
        return True

    if not is_process_alive(pid):
        logger.info(f"force_stop_service, process not alive, {name=}, {pid=}")
        return True

    try:
        logger.info(f"force_stop_service, killing process, {name=}, {pid=}")
        subprocess.run(
            ["taskkill", "/PID", str(pid), "/F"],
            capture_output=True,
            timeout=10,
        )
        return True
    except (subprocess.TimeoutExpired, OSError) as e:
        logger.error(f"force_stop_service, kill failed, {name=}, {pid=}, {e}")
        return False


def _cleanup_pid_file(name: str) -> None:
    """清理 PID 文件。"""
    pid_file = PID_FILE_TEMPLATE.format(name)
    try:
        if os.path.exists(pid_file):
            os.remove(pid_file)
            logger.info(f"_cleanup_pid_file, removed, {name=}")
    except OSError as e:
        logger.error(f"_cleanup_pid_file, failed, {name=}, {e}")


def force_stop_all() -> None:
    """强制终止所有服务并清理 PID 文件。按逆序停止：gate_server -> game_server -> dbmgr。"""
    for name in STOP_ORDER:
        force_stop_service(name)
        _cleanup_pid_file(name)

    logger.info("force_stop_all, all services stopped")


def start_service(name: str) -> bool:
    """启动单个服务进程。

    Args:
        name: 服务名称

    Returns:
        启动成功（PID 文件已写入）返回 True，否则返回 False
    """
    exe_path = START_CMD_TEMPLATE.format(name)
    args_str = START_ARGS_TEMPLATE.format(name)
    cmd = f'"{exe_path}" {args_str}'

    logger.info(f"start_service, starting, {name=}, {cmd=}")

    try:
        subprocess.Popen(
            cmd,
            cwd=PROJECT_ROOT,
            shell=True,
            # stdout/stderr 不重定向，由服务自身管理日志
        )
    except OSError as e:
        logger.error(f"start_service, launch failed, {name=}, {e}")
        return False

    # 等待 PID 文件写入（最多 5 秒）
    pid_file = PID_FILE_TEMPLATE.format(name)
    for _ in range(STARTUP_VERIFY_TIMEOUT_SEC * 10):
        if os.path.exists(pid_file):
            pid = read_pid(name)
            if pid is not None and is_process_alive(pid):
                logger.info(f"start_service, started successfully, {name=}, {pid=}")
                return True
        time.sleep(0.1)

    logger.error(f"start_service, startup verification timeout, {name=}")
    return False


def restart_all() -> bool:
    """重启所有服务：逆序停止 + 正序启动，每步间隔 2 秒。

    Returns:
        全部重启成功返回 True，中间有失败返回 False
    """
    logger.info("restart_all, starting restart sequence")

    # 逆序停止
    for name in STOP_ORDER:
        logger.info(f"restart_all, stopping, {name=}")
        force_stop_service(name)
        _cleanup_pid_file(name)
        time.sleep(STEP_INTERVAL_SEC)

    # 正序启动
    for name in SERVICE_NAMES:
        logger.info(f"restart_all, starting, {name=}")
        if not start_service(name):
            logger.error(f"restart_all, failed to start {name}, aborting remaining starts")
            return False
        time.sleep(STEP_INTERVAL_SEC)

    logger.info("restart_all, restart complete")
    return True
