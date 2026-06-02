"""
客户端连接模块测试

测试用例：
TC-001: 连接到服务器
TC-002: 连接失败处理
TC-003: 连接超时处理
TC-004: 发送和接收消息
TC-005: 粘包处理
TC-006: 拆包处理
TC-007: 心跳维持
TC-008: 心跳超时检测
TC-009: 连接状态查询
TC-010: 主动断开连接
"""
import socket
import struct
import threading
import time
import sys
import os

# 添加 protobuf 生成目录到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'common', 'proto', 'generated'))

import base_pb2
from connection import GateConnection, ConnectionState


class MockServer:
    """模拟服务器，用于测试"""

    def __init__(self, host='127.0.0.1', port=0, respond=True):
        self.host = host
        self.port = port
        self.server_socket = None
        self.client_socket = None
        self.running = False
        self.thread = None
        self.received_messages = []
        self.respond = respond  # 是否发送响应

    def start(self):
        """启动模拟服务器"""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(1)
        self.port = self.server_socket.getsockname()[1]  # 获取实际端口
        self.running = True
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def stop(self):
        """停止模拟服务器"""
        self.running = False
        if self.client_socket:
            try:
                self.client_socket.close()
            except:
                pass
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
        if self.thread:
            self.thread.join(timeout=2.0)

    def _run(self):
        """服务器主循环"""
        try:
            # 等待连接
            self.server_socket.settimeout(5.0)
            client, addr = self.server_socket.accept()
            self.client_socket = client
            client.settimeout(0.5)  # 设置客户端 socket 超时

            # 处理消息
            buffer = b''
            while self.running:
                try:
                    data = client.recv(4096)
                    if not data:
                        break

                    buffer += data
                    while len(buffer) >= 8:
                        # 解析长度
                        msg_length = struct.unpack('!I', buffer[:4])[0]
                        if len(buffer) < msg_length + 4:
                            break

                        # 提取消息
                        msg_data = buffer[4:msg_length + 4]
                        buffer = buffer[msg_length + 4:]

                        # 解析 MsgID
                        msg_id = struct.unpack('!I', msg_data[:4])[0]
                        payload = msg_data[4:]

                        # 存储接收到的消息
                        self.received_messages.append((msg_id, payload))

                        # 发送响应
                        if self.respond:
                            response = struct.pack('!II', len(payload) + 4, msg_id) + payload
                            client.sendall(response)

                except socket.timeout:
                    continue
                except Exception as e:
                    if self.running:
                        print(f"Server error: {e}")
                    break
        except Exception as e:
            if self.running:
                print(f"Server accept error: {e}")

    def send_to_client(self, msg_id, payload):
        """向客户端发送消息"""
        if self.client_socket:
            msg = struct.pack('!II', len(payload) + 4, msg_id) + payload
            try:
                self.client_socket.sendall(msg)
            except:
                pass


def pack_message(msg_id: int, payload: bytes) -> bytes:
    """构造消息: [4字节长度][4字节MsgID][Payload]"""
    return struct.pack('!II', len(payload) + 4, msg_id) + payload


def test_connect_to_server():
    """TC-001: 连接到服务器"""
    print("TC-001: 连接到服务器 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        assert conn.connect() == True, "连接应该成功"
        assert conn.is_connected == True, "应该处于已连接状态"
        assert conn.state == ConnectionState.CONNECTED, "状态应该是 CONNECTED"

        conn.disconnect()
        assert conn.is_connected == False, "断开后应该不是已连接状态"
        print("PASS")
    finally:
        server.stop()


def test_connection_failure():
    """TC-002: 连接失败处理"""
    print("TC-002: 连接失败处理 ... ", end="")
    # 使用一个不存在的端口
    conn = GateConnection('127.0.0.1', 19999, timeout=1.0)

    try:
        conn.connect()
        assert False, "应该抛出异常"
    except (ConnectionError, TimeoutError) as e:
        assert conn.state == ConnectionState.DISCONNECTED, "状态应该是 DISCONNECTED"
        print("PASS")
    except Exception as e:
        assert False, f"应该抛出 ConnectionError 或 TimeoutError，但抛出了 {type(e)}: {e}"


def test_connection_timeout():
    """TC-003: 连接超时处理"""
    print("TC-003: 连接超时处理 ... ", end="")
    # 使用一个不可达的地址
    conn = GateConnection('192.0.2.1', 8888, timeout=0.5)  # 使用不可达的 IP

    try:
        conn.connect()
        assert False, "应该抛出异常"
    except TimeoutError as e:
        assert conn.state == ConnectionState.DISCONNECTED, "状态应该是 DISCONNECTED"
        print("PASS")
    except Exception as e:
        # 某些系统可能抛出其他异常
        assert "超时" in str(e) or "timed out" in str(e).lower(), f"错误信息不正确: {e}"
        print("PASS")


def test_send_receive_message():
    """TC-004: 发送和接收消息"""
    print("TC-004: 发送和接收消息 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        conn.connect()

        # 发送心跳消息
        heartbeat = base_pb2.Heartbeat()
        heartbeat.timestamp = int(time.time())
        payload = heartbeat.SerializeToString()
        assert conn.send_message(1, payload) == True, "发送应该成功"

        # 等待消息处理和响应
        time.sleep(2.0)

        # 检查服务器是否收到消息
        assert len(server.received_messages) > 0, "服务器应该收到消息"
        msg_id, received_payload = server.received_messages[0]
        assert msg_id == 1, f"消息 ID 应该是 1，实际是 {msg_id}"

        # 接收响应
        messages = conn.recv_all_messages()
        assert len(messages) > 0, "应该收到响应消息"

        conn.disconnect()
        print("PASS")
    finally:
        server.stop()


def test_sticky_packet():
    """TC-005: 粘包处理"""
    print("TC-005: 粘包处理 ... ", end="")

    # 使用原始 socket 测试粘包
    server = MockServer()
    server.start()

    try:
        # 直接使用 socket 连接
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect(('127.0.0.1', server.port))

        # 准备多个消息并一次性发送（模拟粘包）
        messages = []
        for i in range(3):
            heartbeat = base_pb2.Heartbeat()
            heartbeat.timestamp = int(time.time()) + i
            payload = heartbeat.SerializeToString()
            messages.append(pack_message(1, payload))

        # 一次性发送所有消息
        sock.sendall(b''.join(messages))

        # 等待消息处理
        time.sleep(1.0)

        # 应该收到3个消息
        assert len(server.received_messages) == 3, f"应该收到3个消息，实际收到 {len(server.received_messages)}"

        sock.close()
        print("PASS")
    finally:
        server.stop()


def test_split_packet():
    """TC-006: 拆包处理"""
    print("TC-006: 拆包处理 ... ", end="")

    # 使用原始 socket 测试拆包
    server = MockServer()
    server.start()

    try:
        # 直接使用 socket 连接
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect(('127.0.0.1', server.port))

        # 准备一个消息
        heartbeat = base_pb2.Heartbeat()
        heartbeat.timestamp = int(time.time())
        payload = heartbeat.SerializeToString()
        msg = pack_message(1, payload)

        # 分片发送（模拟拆包）
        part1 = msg[:4]  # 长度头
        part2 = msg[4:6]  # 部分 MsgID
        part3 = msg[6:]  # 剩余部分

        sock.sendall(part1)
        time.sleep(0.1)
        sock.sendall(part2)
        time.sleep(0.1)
        sock.sendall(part3)

        # 等待消息处理
        time.sleep(1.0)

        # 应该收到1个完整消息
        assert len(server.received_messages) == 1, f"应该收到1个消息，实际收到 {len(server.received_messages)}"

        sock.close()
        print("PASS")
    finally:
        server.stop()


def test_heartbeat():
    """TC-007: 心跳维持"""
    print("TC-007: 心跳维持 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        conn.connect()

        # 等待心跳发送（心跳间隔是5秒）
        time.sleep(7)

        # 检查是否有心跳消息
        heartbeat_count = 0
        for msg_id, _ in server.received_messages:
            if msg_id == 1:  # 心跳消息 ID
                heartbeat_count += 1

        assert heartbeat_count >= 1, f"应该至少收到1个心跳消息，实际收到 {heartbeat_count}"

        conn.disconnect()
        print("PASS")
    finally:
        server.stop()


def test_heartbeat_timeout():
    """TC-008: 心跳超时检测"""
    print("TC-008: 心跳超时检测 ... ", end="")

    # 创建一个不会发送响应的服务器
    server = MockServer(respond=False)
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        disconnect_reason = []
        conn.set_on_disconnect(lambda reason: disconnect_reason.append(reason))

        conn.connect()

        # 等待心跳超时（15秒）
        time.sleep(17)

        # 应该已经断开连接
        assert conn.is_connected == False, "应该已经断开连接"
        assert len(disconnect_reason) > 0, "应该收到断开原因"
        assert "超时" in disconnect_reason[0] or "timed out" in disconnect_reason[0].lower(), f"断开原因不正确: {disconnect_reason[0]}"

        conn.disconnect()
        print("PASS")
    finally:
        server.stop()


def test_connection_state():
    """TC-009: 连接状态查询"""
    print("TC-009: 连接状态查询 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)

        # 初始状态
        assert conn.state == ConnectionState.DISCONNECTED, "初始状态应该是 DISCONNECTED"
        assert conn.is_connected == False, "初始应该未连接"

        # 连接后状态
        conn.connect()
        assert conn.state == ConnectionState.CONNECTED, "连接后状态应该是 CONNECTED"
        assert conn.is_connected == True, "连接后应该已连接"

        # 断开后状态
        conn.disconnect()
        assert conn.state == ConnectionState.DISCONNECTED, "断开后状态应该是 DISCONNECTED"
        assert conn.is_connected == False, "断开后应该未连接"

        print("PASS")
    finally:
        server.stop()


def test_disconnect():
    """TC-010: 主动断开连接"""
    print("TC-010: 主动断开连接 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        disconnect_reason = []
        conn.set_on_disconnect(lambda reason: disconnect_reason.append(reason))

        conn.connect()
        assert conn.is_connected == True, "应该已连接"

        # 主动断开
        conn.disconnect()
        assert conn.is_connected == False, "应该已断开"
        assert conn.state == ConnectionState.DISCONNECTED, "状态应该是 DISCONNECTED"

        # 主动断开不应该触发回调
        assert len(disconnect_reason) == 0, "主动断开不应该触发回调"

        print("PASS")
    finally:
        server.stop()


def main():
    print("=" * 60)
    print("客户端连接模块测试")
    print("=" * 60)

    tests = [
        test_connect_to_server,
        test_connection_failure,
        test_connection_timeout,
        test_send_receive_message,
        test_sticky_packet,
        test_split_packet,
        test_heartbeat,
        test_heartbeat_timeout,
        test_connection_state,
        test_disconnect,
    ]

    passed = 0
    failed = 0
    errors = []

    for test_fn in tests:
        try:
            test_fn()
            passed += 1
        except Exception as e:
            failed += 1
            errors.append((test_fn.__doc__, str(e)))
            print(f"FAIL: {e}")

    print("=" * 60)
    print(f"测试结果: {passed} 通过, {failed} 失败，共 {len(tests)} 个测试")
    if errors:
        print("\n失败的测试:")
        for name, err in errors:
            print(f"  {name}: {err}")
    print("=" * 60)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
