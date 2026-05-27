"""
服务器控制规范模块测试
"""

import os
import sys
import unittest
from unittest.mock import patch, MagicMock

# 添加 scripts 目录到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from common.server_control import (
    read_pid_file,
    check_process_alive,
    get_service_status,
    parse_log_line,
    read_server_logs,
    filter_logs_by_level,
    filter_logs_by_module,
    check_error_logs,
    ServiceStatus,
    LogEntry,
)


class TestPidFileManagement(unittest.TestCase):
    """PID 文件管理测试"""

    @patch('common.server_control.os.path.exists')
    @patch('common.server_control.open', create=True)
    def test_read_pid_file_success(self, mock_open, mock_exists):
        """测试成功读取 PID 文件"""
        mock_exists.return_value = True
        mock_open.return_value.__enter__ = lambda s: s
        mock_open.return_value.__exit__ = MagicMock(return_value=False)
        mock_open.return_value.read.return_value = '12345'

        result = read_pid_file('gate_server')
        self.assertEqual(result, 12345)

    @patch('common.server_control.os.path.exists')
    def test_read_pid_file_not_exists(self, mock_exists):
        """测试 PID 文件不存在"""
        mock_exists.return_value = False

        result = read_pid_file('gate_server')
        self.assertIsNone(result)

    @patch('common.server_control.os.path.exists')
    @patch('common.server_control.open', create=True)
    def test_read_pid_file_empty(self, mock_open, mock_exists):
        """测试 PID 文件为空"""
        mock_exists.return_value = True
        mock_open.return_value.__enter__ = lambda s: s
        mock_open.return_value.__exit__ = MagicMock(return_value=False)
        mock_open.return_value.read.return_value = ''

        result = read_pid_file('gate_server')
        self.assertIsNone(result)

    def test_read_pid_file_invalid_service(self):
        """测试无效的服务名称"""
        result = read_pid_file('invalid_service')
        self.assertIsNone(result)


class TestProcessAlive(unittest.TestCase):
    """进程存活检测测试"""

    @patch('common.server_control.sys.platform', 'win32')
    @patch('common.server_control.subprocess.run')
    def test_check_process_alive_win32(self, mock_run):
        """测试 Windows 平台进程存活检测"""
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout='12345'
        )

        result = check_process_alive(12345)
        self.assertTrue(result)

    @patch('common.server_control.sys.platform', 'win32')
    @patch('common.server_control.subprocess.run')
    def test_check_process_alive_not_found(self, mock_run):
        """测试进程不存在"""
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout='INFO: No tasks running.'
        )

        result = check_process_alive(12345)
        self.assertFalse(result)

    def test_check_process_alive_invalid_pid(self):
        """测试无效的 PID"""
        result = check_process_alive(None)
        self.assertFalse(result)

        result = check_process_alive(0)
        self.assertFalse(result)

        result = check_process_alive(-1)
        self.assertFalse(result)


class TestServiceStatus(unittest.TestCase):
    """服务状态测试"""

    @patch('common.server_control.read_pid_file')
    @patch('common.server_control.check_process_alive')
    def test_get_service_status_running(self, mock_alive, mock_pid):
        """测试服务运行中"""
        mock_pid.return_value = 12345
        mock_alive.return_value = True

        result = get_service_status('gate_server')
        self.assertEqual(result, ServiceStatus.RUNNING)

    @patch('common.server_control.read_pid_file')
    def test_get_service_status_stopped_no_pid(self, mock_pid):
        """测试服务停止（无 PID 文件）"""
        mock_pid.return_value = None

        result = get_service_status('gate_server')
        self.assertEqual(result, ServiceStatus.STOPPED)

    @patch('common.server_control.read_pid_file')
    @patch('common.server_control.check_process_alive')
    def test_get_service_status_stopped_process_dead(self, mock_alive, mock_pid):
        """测试服务停止（进程已死）"""
        mock_pid.return_value = 12345
        mock_alive.return_value = False

        result = get_service_status('gate_server')
        self.assertEqual(result, ServiceStatus.STOPPED)


class TestLogParsing(unittest.TestCase):
    """日志解析测试"""

    def test_parse_log_line_success(self):
        """测试成功解析日志行"""
        log_line = '[2026-05-27 03:21:36.444][info][gate_server:32608] [Main]PID file written: ./runtimeData/gate_server.pid'
        result = parse_log_line(log_line)

        self.assertIsNotNone(result)
        self.assertEqual(result.timestamp, '2026-05-27 03:21:36.444')
        self.assertEqual(result.level, 'info')
        self.assertEqual(result.process_name, 'gate_server')
        self.assertEqual(result.pid, 32608)
        self.assertEqual(result.module, 'Main')
        self.assertIn('PID file written', result.message)

    def test_parse_log_line_invalid(self):
        """测试解析无效日志行"""
        log_line = 'This is not a valid log line'
        result = parse_log_line(log_line)
        self.assertIsNone(result)

    def test_parse_log_line_error_level(self):
        """测试解析错误级别日志"""
        log_line = '[2026-05-27 03:21:36.444][error][gate_server:32608] [Gate]Connection failed'
        result = parse_log_line(log_line)

        self.assertIsNotNone(result)
        self.assertEqual(result.level, 'error')
        self.assertEqual(result.module, 'Gate')


class TestLogFiltering(unittest.TestCase):
    """日志过滤测试"""

    def setUp(self):
        """设置测试数据"""
        self.test_logs = [
            '[2026-05-27 03:21:36.444][info][gate_server:32608] [Main]PID file written\n',
            '[2026-05-27 03:21:36.444][error][gate_server:32608] [Gate]Connection failed\n',
            '[2026-05-27 03:21:36.444][info][gate_server:32608] [Gate]Listening on 0.0.0.0:8080\n',
            '[2026-05-27 03:21:36.444][warn][gate_server:32608] [GameConnection]Disconnected\n',
        ]

    def test_filter_logs_by_level(self):
        """测试按级别过滤日志"""
        result = filter_logs_by_level(self.test_logs, 'error')
        self.assertEqual(len(result), 1)
        self.assertIn('Connection failed', result[0])

    def test_filter_logs_by_level_case_insensitive(self):
        """测试级别过滤不区分大小写"""
        result = filter_logs_by_level(self.test_logs, 'ERROR')
        self.assertEqual(len(result), 1)

    def test_filter_logs_by_module(self):
        """测试按模块过滤日志"""
        result = filter_logs_by_module(self.test_logs, 'Gate')
        self.assertEqual(len(result), 2)

    def test_filter_logs_by_module_case_insensitive(self):
        """测试模块过滤不区分大小写"""
        result = filter_logs_by_module(self.test_logs, 'gate')
        self.assertEqual(len(result), 2)


class TestErrorLogCheck(unittest.TestCase):
    """错误日志检查测试"""

    @patch('common.server_control.read_server_logs')
    @patch('common.server_control.filter_logs_by_level')
    def test_check_error_logs(self, mock_filter, mock_read):
        """测试检查错误日志"""
        mock_read.return_value = ['log1', 'log2']
        mock_filter.return_value = ['error_log']

        result = check_error_logs('gate_server', lines=50)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], 'error_log')

        mock_read.assert_called_once_with('gate_server', 50)
        mock_filter.assert_called_once_with(['log1', 'log2'], 'error')


if __name__ == '__main__':
    unittest.main()
