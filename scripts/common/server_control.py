"""
服务器控制规范模块
实现 AI 智能体对 farm_demo 服务器进程的操作规范
"""

import os
import re
import sys
import time
import socket
import struct
import subprocess
import logging
from typing import Optional, Dict, List, Tuple
from dataclasses import dataclass
from enum import Enum

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s][%(levelname)s][%(name)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger('server_control')


# ===========================================
# 常量定义
# ===========================================

# 运行时数据目录
RUNTIME_DATA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'runtimeData')

# PID 文件路径
PID_FILES = {
    'dbmgr_server': os.path.join(RUNTIME_DATA_DIR, 'dbmgr_server.pid'),
    'game_server': os.path.join(RUNTIME_DATA_DIR, 'game_server.pid'),
    'gate_server': os.path.join(RUNTIME_DATA_DIR, 'gate_server.pid'),
}

# 日志文件路径
LOG_FILES = {
    'dbmgr_server': os.path.join(RUNTIME_DATA_DIR, 'logs', 'server', 'dbmgr_server.log'),
    'game_server': os.path.join(RUNTIME_DATA_DIR, 'logs', 'server', 'game_server.log'),
    'gate_server': os.path.join(RUNTIME_DATA_DIR, 'logs', 'server', 'gate_server.log'),
}

# 服务可执行文件路径
SERVICE_EXE_PATHS = {
    'dbmgr_server': os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'scripts', 'server', 'dbmgr_server', 'Release', 'dbmgr_server.exe'),
    'game_server': os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'scripts', 'server', 'game_server', 'Release', 'game_server.exe'),
    'gate_server': os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'scripts', 'server', 'gate_server', 'Release', 'gate_server.exe'),
}

# 服务启动顺序（依赖顺序）
STARTUP_ORDER = ['dbmgr_server', 'game_server', 'gate_server']

# 服务停服顺序（启动顺序的逆序）
SHUTDOWN_ORDER = ['gate_server', 'game_server', 'dbmgr_server']

# 默认端口配置
DEFAULT_PORTS = {
    'gate_server': 8080,
    'game_server': 9090,
}

# MSG_ID_SHUTDOWN 消息 ID
MSG_ID_SHUTDOWN = 5001

# 日志格式正则表达式
LOG_PATTERN = re.compile(
    r'\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\]'  # 时间
    r'\[(\w+)\]'                                            # 级别
    r'\[(\w+):(\d+)\]'                                      # 进程名:PID
    r' \[(\w+)\]'                                           # 模块
    r'(.*)'                                                 # 消息内容
)


# ===========================================
# 数据类定义
# ===========================================

class ServiceStatus(Enum):
    """服务状态枚举"""
    RUNNING = 'running'
    STOPPED = 'stopped'
    UNKNOWN = 'unknown'


@dataclass
class LogEntry:
    """日志条目数据类"""
    timestamp: str
    level: str
    process_name: str
    pid: int
    module: str
    message: str


@dataclass
class ServiceInfo:
    """服务信息数据类"""
    name: str
    pid: Optional[int]
    status: ServiceStatus
    pid_file: str
    log_file: str


# ===========================================
# PID 文件管理功能
# ===========================================

def read_pid_file(service_name: str) -> Optional[int]:
    """
    读取指定服务的 PID 文件

    Args:
        service_name: 服务名称（dbmgr_server, game_server, gate_server）

    Returns:
        int: PID 值，如果文件不存在或读取失败返回 None
    """
    if service_name not in PID_FILES:
        logger.error(f"read_pid_file, 未知的服务名称: {service_name}")
        return None

    pid_file = PID_FILES[service_name]

    if not os.path.exists(pid_file):
        logger.info(f"read_pid_file, PID 文件不存在: {pid_file}")
        return None

    try:
        with open(pid_file, 'r') as f:
            pid_str = f.read().strip()
            if pid_str:
                pid = int(pid_str)
                logger.debug(f"read_pid_file, 读取 PID 成功: {service_name}={pid}")
                return pid
            else:
                logger.warning(f"read_pid_file, PID 文件为空: {pid_file}")
                return None
    except (ValueError, IOError) as e:
        logger.error(f"read_pid_file, 读取 PID 文件失败: {pid_file}, error={str(e)}")
        return None


def check_process_alive(pid: int) -> bool:
    """
    检查进程是否存活

    Args:
        pid: 进程 ID

    Returns:
        bool: 进程是否存活
    """
    if pid is None or pid <= 0:
        return False

    try:
        # Windows: 使用 tasklist 命令检查进程
        if sys.platform == 'win32':
            result = subprocess.run(
                ['tasklist', '/FI', f'PID eq {pid}', '/NH'],
                capture_output=True,
                text=True,
                timeout=5
            )
            # 如果输出中包含 PID，说明进程存在
            return str(pid) in result.stdout
        else:
            # Linux/macOS: 使用 os.kill 发送信号 0
            os.kill(pid, 0)
            return True
    except (subprocess.TimeoutExpired, subprocess.SubprocessError, OSError):
        return False


def get_service_status(service_name: str) -> ServiceStatus:
    """
    获取服务状态

    Args:
        service_name: 服务名称

    Returns:
        ServiceStatus: 服务状态
    """
    pid = read_pid_file(service_name)

    if pid is None:
        return ServiceStatus.STOPPED

    if check_process_alive(pid):
        return ServiceStatus.RUNNING
    else:
        return ServiceStatus.STOPPED


def get_all_service_status() -> Dict[str, ServiceInfo]:
    """
    获取所有服务状态

    Returns:
        Dict[str, ServiceInfo]: 服务信息字典
    """
    services = {}

    for service_name in STARTUP_ORDER:
        pid = read_pid_file(service_name)
        status = get_service_status(service_name)

        services[service_name] = ServiceInfo(
            name=service_name,
            pid=pid,
            status=status,
            pid_file=PID_FILES[service_name],
            log_file=LOG_FILES[service_name],
        )

    return services


# ===========================================
# 日志读取和解析功能
# ===========================================

def parse_log_line(log_line: str) -> Optional[LogEntry]:
    """
    解析日志行

    Args:
        log_line: 日志行字符串

    Returns:
        LogEntry: 解析后的日志条目，解析失败返回 None
    """
    match = LOG_PATTERN.match(log_line.strip())
    if not match:
        return None

    try:
        return LogEntry(
            timestamp=match.group(1),
            level=match.group(2),
            process_name=match.group(3),
            pid=int(match.group(4)),
            module=match.group(5),
            message=match.group(6).strip(),
        )
    except (ValueError, IndexError) as e:
        logger.warning(f"parse_log_line, 解析失败: {str(e)}")
        return None


def read_server_logs(service_name: str, lines: int = 100) -> List[str]:
    """
    读取服务器日志

    Args:
        service_name: 服务名称
        lines: 读取的行数（从末尾开始）

    Returns:
        List[str]: 日志行列表
    """
    if service_name not in LOG_FILES:
        logger.error(f"read_server_logs, 未知的服务名称: {service_name}")
        return []

    log_file = LOG_FILES[service_name]

    if not os.path.exists(log_file):
        logger.info(f"read_server_logs, 日志文件不存在: {log_file}")
        return []

    try:
        with open(log_file, 'r', encoding='utf-8') as f:
            all_lines = f.readlines()
            # 返回最后 N 行
            return all_lines[-lines:] if len(all_lines) > lines else all_lines
    except IOError as e:
        logger.error(f"read_server_logs, 读取日志文件失败: {log_file}, error={str(e)}")
        return []


def filter_logs_by_level(logs: List[str], level: str) -> List[str]:
    """
    按日志级别过滤

    Args:
        logs: 日志行列表
        level: 日志级别（info, warn, error）

    Returns:
        List[str]: 过滤后的日志行列表
    """
    filtered = []
    level_lower = level.lower()

    for log_line in logs:
        entry = parse_log_line(log_line)
        if entry and entry.level.lower() == level_lower:
            filtered.append(log_line)

    return filtered


def filter_logs_by_module(logs: List[str], module: str) -> List[str]:
    """
    按模块过滤

    Args:
        logs: 日志行列表
        module: 模块名称（Main, Gate, Game, DBMgr 等）

    Returns:
        List[str]: 过滤后的日志行列表
    """
    filtered = []

    for log_line in logs:
        entry = parse_log_line(log_line)
        if entry and entry.module.lower() == module.lower():
            filtered.append(log_line)

    return filtered


def check_error_logs(service_name: str, lines: int = 100) -> List[str]:
    """
    检查错误日志

    Args:
        service_name: 服务名称
        lines: 检查的行数

    Returns:
        List[str]: 错误日志行列表
    """
    logs = read_server_logs(service_name, lines)
    return filter_logs_by_level(logs, 'error')


# ===========================================
# 优雅停服功能
# ===========================================

def send_shutdown_message(host: str, port: int, timeout_ms: int = 5000) -> bool:
    """
    发送 MSG_ID_SHUTDOWN 消息

    Args:
        host: 服务器地址
        port: 服务器端口
        timeout_ms: 超时时间（毫秒）

    Returns:
        bool: 是否成功发送并收到响应
    """
    try:
        # 创建 socket 连接
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout_ms / 1000.0)

        # 连接到服务器
        sock.connect((host, port))

        # 构造停服消息
        # 消息格式: [4字节长度][4字节MsgID][Payload]
        # Payload: JSON 格式的停服请求
        import json
        payload = json.dumps({
            'reason': 'AI agent shutdown request',
            'timeout_ms': timeout_ms,
        }).encode('utf-8')

        # 打包消息
        msg_length = len(payload) + 4  # MsgID 占 4 字节
        msg = struct.pack('!II', msg_length, MSG_ID_SHUTDOWN) + payload

        # 发送消息
        sock.sendall(msg)

        # 等待响应
        response_data = sock.recv(4096)

        # 关闭连接
        sock.close()

        if response_data:
            # 解析响应
            resp_length = struct.unpack('!I', response_data[:4])[0]
            resp_msg_id = struct.unpack('!I', response_data[4:8])[0]
            resp_payload = response_data[8:resp_length + 4]

            # 检查是否是停服响应
            if resp_msg_id == 5002:  # MSG_ID_SHUTDOWN_RESP
                resp_json = json.loads(resp_payload.decode('utf-8'))
                if resp_json.get('code') == 0:
                    logger.info(f"send_shutdown_message, 收到停服响应: {host}:{port}")
                    return True
                else:
                    logger.warning(f"send_shutdown_message, 停服响应错误: {resp_json}")
                    return False
            else:
                logger.warning(f"send_shutdown_message, 收到未知响应: msg_id={resp_msg_id}")
                return False
        else:
            logger.warning(f"send_shutdown_message, 未收到响应: {host}:{port}")
            return False

    except socket.timeout:
        logger.error(f"send_shutdown_message, 连接超时: {host}:{port}")
        return False
    except ConnectionRefusedError:
        logger.error(f"send_shutdown_message, 连接被拒绝: {host}:{port}")
        return False
    except Exception as e:
        logger.error(f"send_shutdown_message, 发送失败: {str(e)}")
        return False


def graceful_shutdown_service(service_name: str, timeout_ms: int = 10000) -> bool:
    """
    优雅停服单个服务

    Args:
        service_name: 服务名称
        timeout_ms: 超时时间（毫秒）

    Returns:
        bool: 是否成功停服
    """
    if service_name not in DEFAULT_PORTS:
        logger.error(f"graceful_shutdown_service, 未知的服务名称: {service_name}")
        return False

    # 检查服务是否运行中
    status = get_service_status(service_name)
    if status != ServiceStatus.RUNNING:
        logger.info(f"graceful_shutdown_service, 服务未运行: {service_name}")
        return True

    # 获取服务端口
    port = DEFAULT_PORTS[service_name]

    # 发送停服消息
    if send_shutdown_message('127.0.0.1', port, timeout_ms):
        # 等待进程退出
        pid = read_pid_file(service_name)
        if pid:
            start_time = time.time()
            while time.time() - start_time < timeout_ms / 1000.0:
                if not check_process_alive(pid):
                    logger.info(f"graceful_shutdown_service, 服务已停止: {service_name}")
                    return True
                time.sleep(0.5)

            logger.warning(f"graceful_shutdown_service, 等待超时: {service_name}")
            return False
        else:
            logger.info(f"graceful_shutdown_service, PID 文件不存在: {service_name}")
            return True
    else:
        logger.error(f"graceful_shutdown_service, 发送停服消息失败: {service_name}")
        return False


def graceful_shutdown_all(timeout_ms: int = 10000) -> bool:
    """
    优雅停服所有服务（逆序）

    Args:
        timeout_ms: 超时时间（毫秒）

    Returns:
        bool: 是否全部成功停服
    """
    success = True

    for service_name in SHUTDOWN_ORDER:
        if not graceful_shutdown_service(service_name, timeout_ms):
            logger.error(f"graceful_shutdown_all, 停服失败: {service_name}")
            success = False
            # 继续尝试停服其他服务

    return success


# ===========================================
# 强制停服功能
# ===========================================

def force_kill_process(pid: int) -> bool:
    """
    使用 taskkill 强制终止进程

    Args:
        pid: 进程 ID

    Returns:
        bool: 是否成功终止
    """
    if pid is None or pid <= 0:
        logger.error(f"force_kill_process, 无效的 PID: {pid}")
        return False

    try:
        # Windows: 使用 taskkill /F 命令
        if sys.platform == 'win32':
            result = subprocess.run(
                ['taskkill', '/PID', str(pid), '/F'],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode == 0:
                logger.info(f"force_kill_process, 进程已终止: pid={pid}")
                return True
            else:
                logger.error(f"force_kill_process, 终止失败: pid={pid}, error={result.stderr}")
                return False
        else:
            # Linux/macOS: 使用 kill -9 命令
            os.kill(pid, 9)
            logger.info(f"force_kill_process, 进程已终止: pid={pid}")
            return True
    except subprocess.TimeoutExpired:
        logger.error(f"force_kill_process, 命令超时: pid={pid}")
        return False
    except OSError as e:
        logger.error(f"force_kill_process, 终止失败: pid={pid}, error={str(e)}")
        return False


def force_kill_service(service_name: str) -> bool:
    """
    强制停服单个服务

    Args:
        service_name: 服务名称

    Returns:
        bool: 是否成功停服
    """
    pid = read_pid_file(service_name)

    if pid is None:
        logger.info(f"force_kill_service, PID 文件不存在: {service_name}")
        return True

    if not check_process_alive(pid):
        logger.info(f"force_kill_service, 进程已不存在: {service_name}, pid={pid}")
        cleanup_pid_file(service_name)
        return True

    if force_kill_process(pid):
        cleanup_pid_file(service_name)
        return True
    else:
        return False


def cleanup_pid_file(service_name: str) -> bool:
    """
    清理 PID 文件

    Args:
        service_name: 服务名称

    Returns:
        bool: 是否成功清理
    """
    if service_name not in PID_FILES:
        logger.error(f"cleanup_pid_file, 未知的服务名称: {service_name}")
        return False

    pid_file = PID_FILES[service_name]

    if not os.path.exists(pid_file):
        logger.info(f"cleanup_pid_file, PID 文件不存在: {pid_file}")
        return True

    try:
        os.remove(pid_file)
        logger.info(f"cleanup_pid_file, PID 文件已删除: {pid_file}")
        return True
    except OSError as e:
        logger.error(f"cleanup_pid_file, 删除失败: {pid_file}, error={str(e)}")
        return False


# ===========================================
# 启动功能
# ===========================================

def start_service(service_name: str, exe_path: Optional[str] = None, delay_after: int = 2) -> bool:
    """
    启动单个服务

    Args:
        service_name: 服务名称
        exe_path: 可执行文件路径（如果为 None，使用默认路径）
        delay_after: 启动后等待时间（秒）

    Returns:
        bool: 是否成功启动
    """
    if exe_path is None:
        if service_name not in SERVICE_EXE_PATHS:
            logger.error(f"start_service, 未知的服务名称: {service_name}")
            return False
        exe_path = SERVICE_EXE_PATHS[service_name]

    # 检查可执行文件是否存在
    if not os.path.exists(exe_path):
        logger.error(f"start_service, 可执行文件不存在: {exe_path}")
        return False

    # 检查服务是否已在运行
    status = get_service_status(service_name)
    if status == ServiceStatus.RUNNING:
        logger.info(f"start_service, 服务已在运行: {service_name}")
        return True

    try:
        # 启动服务进程
        logger.info(f"start_service, 启动服务: {service_name}, exe={exe_path}")

        # 使用 subprocess.Popen 启动后台进程
        process = subprocess.Popen(
            [exe_path],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0,
        )

        # 等待一段时间，让服务启动
        time.sleep(delay_after)

        # 检查进程是否存活
        if process.poll() is None:
            logger.info(f"start_service, 服务启动成功: {service_name}, pid={process.pid}")
            return True
        else:
            logger.error(f"start_service, 服务启动失败: {service_name}, exit_code={process.returncode}")
            return False

    except Exception as e:
        logger.error(f"start_service, 启动失败: {service_name}, error={str(e)}")
        return False


def start_all_services() -> bool:
    """
    按顺序启动所有服务（dbmgr_server -> game_server -> gate_server）

    Returns:
        bool: 是否全部成功启动
    """
    success = True

    for service_name in STARTUP_ORDER:
        if not start_service(service_name):
            logger.error(f"start_all_services, 启动失败: {service_name}")
            success = False
            break  # 启动失败时停止后续服务启动

        # 等待服务启动完成
        if service_name != STARTUP_ORDER[-1]:
            logger.info(f"start_all_services, 等待服务启动: {service_name}")
            time.sleep(2)

    return success


# ===========================================
# 重启功能
# ===========================================

def restart_all(timeout_ms: int = 10000) -> bool:
    """
    重启所有服务（先停后起）

    Args:
        timeout_ms: 停服超时时间（毫秒）

    Returns:
        bool: 是否全部成功重启
    """
    logger.info("restart_all, 开始重启所有服务")

    # 第一步: 优雅停服
    if not graceful_shutdown_all(timeout_ms):
        logger.warning("restart_all, 优雅停服失败，尝试强制停服")

        # 强制停服
        for service_name in SHUTDOWN_ORDER:
            status = get_service_status(service_name)
            if status == ServiceStatus.RUNNING:
                if not force_kill_service(service_name):
                    logger.error(f"restart_all, 强制停服失败: {service_name}")
                    return False

    # 第二步: 验证停服
    if not verify_shutdown():
        logger.error("restart_all, 停服验证失败")
        return False

    # 第三步: 启动服务
    if not start_all_services():
        logger.error("restart_all, 启动服务失败")
        return False

    # 第四步: 验证启动
    if not verify_startup():
        logger.error("restart_all, 启动验证失败")
        return False

    logger.info("restart_all, 所有服务重启成功")
    return True


# ===========================================
# 操作验证功能
# ===========================================

def verify_startup() -> bool:
    """
    验证启动操作

    Returns:
        bool: 是否验证通过
    """
    logger.info("verify_startup, 开始验证启动操作")

    for service_name in STARTUP_ORDER:
        # 检查 PID 文件
        pid = read_pid_file(service_name)
        if pid is None:
            logger.error(f"verify_startup, PID 文件不存在: {service_name}")
            return False

        # 检查进程存活
        if not check_process_alive(pid):
            logger.error(f"verify_startup, 进程未存活: {service_name}, pid={pid}")
            return False

        # 检查错误日志
        error_logs = check_error_logs(service_name, lines=50)
        if error_logs:
            logger.warning(f"verify_startup, 发现错误日志: {service_name}, count={len(error_logs)}")
            # 不返回 False，只警告

    logger.info("verify_startup, 启动验证通过")
    return True


def verify_shutdown() -> bool:
    """
    验证停服操作

    Returns:
        bool: 是否验证通过
    """
    logger.info("verify_shutdown, 开始验证停服操作")

    for service_name in SHUTDOWN_ORDER:
        # 检查进程是否已退出
        pid = read_pid_file(service_name)
        if pid is not None and check_process_alive(pid):
            logger.error(f"verify_shutdown, 进程仍在运行: {service_name}, pid={pid}")
            return False

        # 检查 PID 文件是否已清理
        if service_name in PID_FILES and os.path.exists(PID_FILES[service_name]):
            logger.warning(f"verify_shutdown, PID 文件仍存在: {service_name}")
            # 优雅停服时由服务自行清理，如果文件存在可能是强制停服

    logger.info("verify_shutdown, 停服验证通过")
    return True


# ===========================================
# 主函数（用于测试）
# ===========================================

if __name__ == '__main__':
    # 测试代码
    print("=== 服务器控制规范模块测试 ===")

    # 显示所有服务状态
    print("\n1. 服务状态:")
    services = get_all_service_status()
    for name, info in services.items():
        print(f"  {name}: {info.status.value} (PID: {info.pid})")

    # 测试日志读取
    print("\n2. 日志读取:")
    for service_name in STARTUP_ORDER:
        logs = read_server_logs(service_name, lines=5)
        print(f"  {service_name}: {len(logs)} 行日志")

    print("\n=== 测试完成 ===")
