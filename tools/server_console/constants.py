# -*- coding: utf-8 -*-
"""服务器控制台常量定义。"""

import os

# 项目根目录（tools/server_console/ 的上两级）
PROJECT_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))

# 服务列表（按启动顺序排列）
SERVICE_NAMES = ["dbmgr", "game_server", "gate_server"]

# 停服顺序（启动顺序的逆序）
STOP_ORDER = list(reversed(SERVICE_NAMES))

# PID 文件路径模板
PID_FILE_TEMPLATE = os.path.join(PROJECT_ROOT, "runtimeData", "{}.pid")

# 日志文件路径模板
LOG_FILE_TEMPLATE = os.path.join(PROJECT_ROOT, "runtimeData", "logs", "server", "{}.log")

# 启动命令模板
START_CMD_TEMPLATE = os.path.join(PROJECT_ROOT, "bin", "{}.exe")
START_ARGS_TEMPLATE = "--config config/{}.json"

# 状态检测间隔（毫秒）
STATUS_CHECK_INTERVAL_MS = 2000

# 日志重试间隔（秒）
LOG_RETRY_INTERVAL_SEC = 1

# 启动验证超时（秒）
STARTUP_VERIFY_TIMEOUT_SEC = 5

# 操作步骤间隔（秒）
STEP_INTERVAL_SEC = 2

# 日志缓存最大行数
LOG_CACHE_MAX_LINES = 10000
