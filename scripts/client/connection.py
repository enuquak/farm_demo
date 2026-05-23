"""
Gate 服务器连接模块
实现与 Gate 服务器的 TCP 长连接，支持 Protobuf 消息收发、心跳维持和网络线程
"""
import socket
import threading
import queue
import time
import struct
from enum import Enum
from typing import Optional, Callable, Any

# 添加 protobuf 生成目录到路径
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'common', 'proto', 'generated'))

import base_pb2


class ConnectionState(Enum):
    """连接状态枚举"""
    DISCONNECTED = 0    # 已断开
    CONNECTING = 1      # 连接中
    CONNECTED = 2       # 已连接


class GateConnection:
    """
    Gate 服务器连接类
    管理与 Gate 服务器的 TCP 连接，提供消息收发和心跳功能
    """

    def __init__(self, host: str = '127.0.0.1', port: int = 8888, timeout: float = 5.0):
        """
        初始化连接

        Args:
            host: 服务器地址
            port: 服务器端口
            timeout: 连接超时时间（秒）
        """
        self.host = host
        self.port = port
        self.timeout = timeout

        # 连接状态
        self.state = ConnectionState.DISCONNECTED
        self._state_lock = threading.Lock()

        # Socket 对象
        self._socket: Optional[socket.socket] = None

        # 消息队列
        self._recv_queue: queue.Queue = queue.Queue()  # 接收队列（网络线程 -> 主线程）
        self._send_queue: queue.Queue = queue.Queue()  # 发送队列（主线程 -> 网络线程）

        # 网络线程
        self._network_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()

        # 心跳管理
        self._heartbeat_manager: Optional['HeartbeatManager'] = None

        # 回调函数
        self._on_disconnect: Optional[Callable[[str], None]] = None

        # 接收缓冲区
        self._recv_buffer = b''

    @property
    def is_connected(self) -> bool:
        """检查是否已连接"""
        with self._state_lock:
            return self.state == ConnectionState.CONNECTED

    def set_on_disconnect(self, callback: Callable[[str], None]):
        """设置断开连接回调"""
        self._on_disconnect = callback

    def connect(self) -> bool:
        """
        连接到 Gate 服务器

        Returns:
            bool: 连接是否成功

        Raises:
            ConnectionError: 连接失败
            TimeoutError: 连接超时
        """
        with self._state_lock:
            if self.state != ConnectionState.DISCONNECTED:
                return False
            self.state = ConnectionState.CONNECTING

        try:
            # 创建 socket
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._socket.settimeout(self.timeout)

            # 尝试连接
            self._socket.connect((self.host, self.port))

            # 连接成功，移除超时设置，使用非阻塞模式
            self._socket.settimeout(None)
            self._socket.setblocking(False)

            # 连接成功
            with self._state_lock:
                self.state = ConnectionState.CONNECTED

            # 启动网络线程
            self._start_network_thread()

            # 启动心跳
            self._start_heartbeat()

            return True

        except socket.timeout:
            with self._state_lock:
                self.state = ConnectionState.DISCONNECTED
            raise TimeoutError(f"连接超时: {self.host}:{self.port}")

        except ConnectionRefusedError:
            with self._state_lock:
                self.state = ConnectionState.DISCONNECTED
            raise ConnectionError(f"连接被拒绝: {self.host}:{self.port}")

        except Exception as e:
            with self._state_lock:
                self.state = ConnectionState.DISCONNECTED
            raise ConnectionError(f"连接失败: {str(e)}")

    def disconnect(self):
        """断开连接"""
        with self._state_lock:
            if self.state == ConnectionState.DISCONNECTED:
                return
            self.state = ConnectionState.DISCONNECTED

        # 停止心跳
        self._stop_heartbeat()

        # 停止网络线程
        self._stop_event.set()
        if self._network_thread and self._network_thread.is_alive():
            self._network_thread.join(timeout=2.0)

        # 关闭 socket
        if self._socket:
            try:
                self._socket.close()
            except:
                pass
            self._socket = None

        # 清空队列
        self._clear_queues()

    def send_message(self, msg_id: int, payload: bytes) -> bool:
        """
        发送消息

        Args:
            msg_id: 消息 ID
            payload: 消息负载（Protobuf 序列化后的字节）

        Returns:
            bool: 是否成功放入发送队列
        """
        if not self.is_connected:
            return False

        # 构造消息: [4字节长度][4字节MsgID][Payload]
        msg = struct.pack('!II', len(payload) + 4, msg_id) + payload
        self._send_queue.put(msg)
        return True

    def send_heartbeat(self):
        """发送心跳消息"""
        heartbeat = base_pb2.Heartbeat()
        heartbeat.timestamp = int(time.time())
        payload = heartbeat.SerializeToString()
        self.send_message(1, payload)  # MsgID 1 = 心跳

    def send_login_request(self, token: str):
        """发送登录请求"""
        login_req = base_pb2.LoginReq()
        login_req.token = token
        payload = login_req.SerializeToString()
        self.send_message(2, payload)  # MsgID 2 = 登录请求

    def recv_message(self) -> Optional[tuple]:
        """
        从接收队列获取消息（非阻塞）

        Returns:
            tuple: (msg_id, payload) 或 None
        """
        try:
            return self._recv_queue.get_nowait()
        except queue.Empty:
            return None

    def recv_all_messages(self) -> list:
        """
        获取所有待处理的消息

        Returns:
            list: 消息列表 [(msg_id, payload), ...]
        """
        messages = []
        while True:
            msg = self.recv_message()
            if msg is None:
                break
            messages.append(msg)
        return messages

    def _start_network_thread(self):
        """启动网络线程"""
        self._stop_event.clear()
        self._network_thread = threading.Thread(
            target=self._network_loop,
            name="NetworkThread",
            daemon=True
        )
        self._network_thread.start()

    def _network_loop(self):
        """网络线程主循环"""
        while not self._stop_event.is_set():
            try:
                # 发送消息
                self._send_pending_messages()

                # 接收消息
                self._receive_messages()

                # 短暂休眠，避免 CPU 占用过高
                time.sleep(0.01)

            except Exception as e:
                # 网络异常，断开连接
                self._handle_disconnect(f"网络异常: {str(e)}")
                break

    def _send_pending_messages(self):
        """发送待处理的消息"""
        while not self._send_queue.empty():
            try:
                msg = self._send_queue.get_nowait()
                self._socket.sendall(msg)
            except queue.Empty:
                break
            except Exception as e:
                raise Exception(f"发送消息失败: {str(e)}")

    def _receive_messages(self):
        """接收消息"""
        try:
            # 使用 select 检查是否有数据可读
            import select
            readable, _, _ = select.select([self._socket], [], [], 0.1)
            if not readable:
                return

            data = self._socket.recv(4096)
            if not data:
                # 连接被关闭
                raise Exception("连接被服务器关闭")

            self._recv_buffer += data
            self._process_recv_buffer()

        except socket.timeout:
            # 超时，继续循环
            pass
        except BlockingIOError:
            # 非阻塞模式下没有数据
            pass

    def _process_recv_buffer(self):
        """处理接收缓冲区"""
        while len(self._recv_buffer) >= 8:  # 至少需要 8 字节（长度 + MsgID）
            # 解析长度（前4字节）
            msg_length = struct.unpack('!I', self._recv_buffer[:4])[0]

            # 检查是否收到完整消息
            if len(self._recv_buffer) < msg_length + 4:
                break  # 等待更多数据

            # 提取完整消息
            msg_data = self._recv_buffer[4:msg_length + 4]
            self._recv_buffer = self._recv_buffer[msg_length + 4:]

            # 解析 MsgID
            msg_id = struct.unpack('!I', msg_data[:4])[0]
            payload = msg_data[4:]

            # 放入接收队列
            self._recv_queue.put((msg_id, payload))

    def _handle_disconnect(self, reason: str):
        """处理断开连接"""
        with self._state_lock:
            if self.state == ConnectionState.DISCONNECTED:
                return
            self.state = ConnectionState.DISCONNECTED

        # 停止心跳
        self._stop_heartbeat()

        # 关闭 socket
        if self._socket:
            try:
                self._socket.close()
            except:
                pass
            self._socket = None

        # 清空队列
        self._clear_queues()

        # 通知上层
        if self._on_disconnect:
            self._on_disconnect(reason)

    def _clear_queues(self):
        """清空消息队列"""
        while not self._recv_queue.empty():
            try:
                self._recv_queue.get_nowait()
            except:
                break

        while not self._send_queue.empty():
            try:
                self._send_queue.get_nowait()
            except:
                break

    def _start_heartbeat(self):
        """启动心跳"""
        try:
            from .heartbeat import HeartbeatManager
        except ImportError:
            from heartbeat import HeartbeatManager
        self._heartbeat_manager = HeartbeatManager(self)
        self._heartbeat_manager.start()

    def _stop_heartbeat(self):
        """停止心跳"""
        if self._heartbeat_manager:
            self._heartbeat_manager.stop()
            self._heartbeat_manager = None
