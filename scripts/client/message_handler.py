"""
消息处理模块
处理从服务器接收到的消息，分发给相应的处理器
"""
from typing import Callable, Dict, Any, Optional
import sys
import os

# 添加 protobuf 生成目录到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'common', 'proto', 'generated'))

import base_pb2


class MessageHandler:
    """
    消息处理器
    管理消息处理回调，分发接收到的消息
    """

    # 消息 ID 定义
    MSG_HEARTBEAT = 1
    MSG_LOGIN_REQ = 2
    MSG_LOGIN_RESP = 3
    MSG_PACKET = 4

    def __init__(self):
        """初始化消息处理器"""
        self._handlers: Dict[int, Callable[[int, bytes], None]] = {}
        self._default_handler: Optional[Callable[[int, bytes], None]] = None

    def register_handler(self, msg_id: int, handler: Callable[[int, bytes], None]):
        """
        注册消息处理器

        Args:
            msg_id: 消息 ID
            handler: 处理函数，接收 (msg_id, payload) 参数
        """
        self._handlers[msg_id] = handler

    def set_default_handler(self, handler: Callable[[int, bytes], None]):
        """
        设置默认处理器

        Args:
            handler: 处理函数，接收 (msg_id, payload) 参数
        """
        self._default_handler = handler

    def handle_message(self, msg_id: int, payload: bytes):
        """
        处理消息

        Args:
            msg_id: 消息 ID
            payload: 消息负载
        """
        handler = self._handlers.get(msg_id)
        if handler:
            handler(msg_id, payload)
        elif self._default_handler:
            self._default_handler(msg_id, payload)

    def handle_heartbeat(self, payload: bytes) -> base_pb2.Heartbeat:
        """
        处理心跳消息

        Args:
            payload: 消息负载

        Returns:
            Heartbeat 消息对象
        """
        heartbeat = base_pb2.Heartbeat()
        heartbeat.ParseFromString(payload)
        return heartbeat

    def handle_login_response(self, payload: bytes) -> base_pb2.LoginResp:
        """
        处理登录响应

        Args:
            payload: 消息负载

        Returns:
            LoginResp 消息对象
        """
        login_resp = base_pb2.LoginResp()
        login_resp.ParseFromString(payload)
        return login_resp

    def handle_packet(self, payload: bytes) -> base_pb2.Packet:
        """
        处理通用消息包

        Args:
            payload: 消息负载

        Returns:
            Packet 消息对象
        """
        packet = base_pb2.Packet()
        packet.ParseFromString(payload)
        return packet

    def create_heartbeat(self) -> bytes:
        """
        创建心跳消息

        Returns:
            序列化后的心跳消息
        """
        import time
        heartbeat = base_pb2.Heartbeat()
        heartbeat.timestamp = int(time.time())
        return heartbeat.SerializeToString()

    def create_login_request(self, token: str) -> bytes:
        """
        创建登录请求

        Args:
            token: 登录令牌

        Returns:
            序列化后的登录请求
        """
        login_req = base_pb2.LoginReq()
        login_req.token = token
        return login_req.SerializeToString()

    def create_packet(self, msg_id: int, payload: bytes) -> bytes:
        """
        创建通用消息包

        Args:
            msg_id: 业务消息 ID
            payload: 业务消息负载

        Returns:
            序列化后的消息包
        """
        packet = base_pb2.Packet()
        packet.msg_id = msg_id
        packet.payload = payload
        return packet.SerializeToString()
