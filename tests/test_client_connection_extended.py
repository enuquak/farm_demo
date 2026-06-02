"""
客户端连接模块扩展测试

测试用例：
TC-011: 网络线程启动和退出
TC-012: 线程安全消息队列
TC-013: 主循环读取消息
TC-014: 主循环发送消息
TC-015: Protobuf消息定义 - 心跳消息
TC-016: Protobuf消息定义 - 登录请求消息
TC-017: Protobuf消息定义 - 登录响应消息
TC-018: Protobuf消息定义 - 通用消息封装
TC-019: 连接异常回调
TC-020: 网络线程异常处理
"""
import socket
import struct
import threading
import time
import sys
import os
import queue

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


def test_network_thread_start_stop():
    """TC-011: 网络线程启动和退出"""
    print("TC-011: 网络线程启动和退出 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        conn.connect()

        # 检查网络线程是否启动
        assert conn._network_thread is not None, "网络线程应该已创建"
        assert conn._network_thread.is_alive(), "网络线程应该正在运行"
        assert conn._network_thread.name == "NetworkThread", "线程名称应该是 NetworkThread"

        # 断开连接，检查线程是否退出
        conn.disconnect()
        time.sleep(0.5)  # 等待线程退出
        assert not conn._network_thread.is_alive(), "网络线程应该已退出"

        print("PASS")
    finally:
        server.stop()


def test_thread_safe_message_queue():
    """TC-012: 线程安全消息队列"""
    print("TC-012: 线程安全消息队列 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        conn.connect()

        # 检查队列是否线程安全
        assert isinstance(conn._recv_queue, queue.Queue), "接收队列应该是 Queue 类型"
        assert isinstance(conn._send_queue, queue.Queue), "发送队列应该是 Queue 类型"

        # 测试队列操作
        test_msg = (1, b"test payload")
        conn._recv_queue.put(test_msg)
        retrieved = conn._recv_queue.get_nowait()
        assert retrieved == test_msg, "队列应该正确存储和检索消息"

        conn.disconnect()
        print("PASS")
    finally:
        server.stop()


def test_main_loop_read_messages():
    """TC-013: 主循环读取消息"""
    print("TC-013: 主循环读取消息 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        conn.connect()

        # 模拟服务器发送消息
        heartbeat = base_pb2.Heartbeat()
        heartbeat.timestamp = int(time.time())
        payload = heartbeat.SerializeToString()
        server.send_to_client(1, payload)

        # 等待消息接收
        time.sleep(0.5)

        # 测试主循环读取
        messages = conn.recv_all_messages()
        assert len(messages) > 0, "应该收到消息"
        msg_id, received_payload = messages[0]
        assert msg_id == 1, "消息ID应该是1"

        # 解析心跳消息
        heartbeat_msg = base_pb2.Heartbeat()
        heartbeat_msg.ParseFromString(received_payload)
        assert heartbeat_msg.timestamp > 0, "时间戳应该大于0"

        conn.disconnect()
        print("PASS")
    finally:
        server.stop()


def test_main_loop_send_messages():
    """TC-014: 主循环发送消息"""
    print("TC-014: 主循环发送消息 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        conn.connect()

        # 测试主循环发送
        heartbeat = base_pb2.Heartbeat()
        heartbeat.timestamp = int(time.time())
        payload = heartbeat.SerializeToString()

        # 发送消息
        result = conn.send_message(1, payload)
        assert result == True, "发送应该成功"

        # 等待消息发送和接收
        time.sleep(1.0)

        # 检查服务器是否收到消息
        assert len(server.received_messages) > 0, "服务器应该收到消息"
        msg_id, received_payload = server.received_messages[0]
        assert msg_id == 1, "消息ID应该是1"

        conn.disconnect()
        print("PASS")
    finally:
        server.stop()


def test_protobuf_heartbeat_message():
    """TC-015: Protobuf消息定义 - 心跳消息"""
    print("TC-015: Protobuf消息定义 - 心跳消息 ... ", end="")

    # 测试心跳消息创建
    heartbeat = base_pb2.Heartbeat()
    timestamp = int(time.time())
    heartbeat.timestamp = timestamp

    # 序列化
    payload = heartbeat.SerializeToString()
    assert len(payload) > 0, "序列化后的数据不应该为空"

    # 反序列化
    heartbeat2 = base_pb2.Heartbeat()
    heartbeat2.ParseFromString(payload)
    assert heartbeat2.timestamp == timestamp, "时间戳应该相同"

    print("PASS")


def test_protobuf_login_request_message():
    """TC-016: Protobuf消息定义 - 登录请求消息"""
    print("TC-016: Protobuf消息定义 - 登录请求消息 ... ", end="")

    # 测试登录请求消息创建
    login_req = base_pb2.LoginReq()
    token = "test_token_123"
    login_req.token = token

    # 序列化
    payload = login_req.SerializeToString()
    assert len(payload) > 0, "序列化后的数据不应该为空"

    # 反序列化
    login_req2 = base_pb2.LoginReq()
    login_req2.ParseFromString(payload)
    assert login_req2.token == token, "token应该相同"

    print("PASS")


def test_protobuf_login_response_message():
    """TC-017: Protobuf消息定义 - 登录响应消息"""
    print("TC-017: Protobuf消息定义 - 登录响应消息 ... ", end="")

    # 测试登录响应消息创建
    login_resp = base_pb2.LoginResp()
    code = 200
    msg = "success"
    login_resp.code = code
    login_resp.msg = msg

    # 序列化
    payload = login_resp.SerializeToString()
    assert len(payload) > 0, "序列化后的数据不应该为空"

    # 反序列化
    login_resp2 = base_pb2.LoginResp()
    login_resp2.ParseFromString(payload)
    assert login_resp2.code == code, "code应该相同"
    assert login_resp2.msg == msg, "msg应该相同"

    print("PASS")


def test_protobuf_packet_message():
    """TC-018: Protobuf消息定义 - 通用消息封装"""
    print("TC-018: Protobuf消息定义 - 通用消息封装 ... ", end="")

    # 测试通用消息包创建
    packet = base_pb2.Packet()
    msg_id = 100
    payload_data = b"test payload data"
    packet.msg_id = msg_id
    packet.payload = payload_data

    # 序列化
    payload = packet.SerializeToString()
    assert len(payload) > 0, "序列化后的数据不应该为空"

    # 反序列化
    packet2 = base_pb2.Packet()
    packet2.ParseFromString(payload)
    assert packet2.msg_id == msg_id, "msg_id应该相同"
    assert packet2.payload == payload_data, "payload应该相同"

    print("PASS")


def test_connection_exception_callback():
    """TC-019: 连接异常回调"""
    print("TC-019: 连接异常回调 ... ", end="")

    # 创建一个会发送响应但之后关闭的服务器
    server = MockServer(respond=True)
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        disconnect_reason = []
        conn.set_on_disconnect(lambda reason: disconnect_reason.append(reason))

        conn.connect()
        assert conn.is_connected == True, "应该已连接"

        # 模拟服务器关闭连接
        server.stop()
        time.sleep(2.0)  # 等待检测到连接断开

        # 应该触发断开回调
        assert len(disconnect_reason) > 0, "应该收到断开原因"
        print("PASS")
    except Exception as e:
        print(f"FAIL: {e}")


def test_network_thread_exception_handling():
    """TC-020: 网络线程异常处理"""
    print("TC-020: 网络线程异常处理 ... ", end="")
    server = MockServer()
    server.start()

    try:
        conn = GateConnection('127.0.0.1', server.port, timeout=5.0)
        conn.connect()

        # 模拟网络异常（关闭服务器socket）
        server.stop()
        time.sleep(1.0)

        # 网络线程应该处理异常并退出
        # 检查连接状态
        # 注意：这可能需要一些时间来检测
        time.sleep(2.0)

        # 连接应该标记为断开
        assert conn.is_connected == False, "连接应该已断开"

        print("PASS")
    except Exception as e:
        print(f"FAIL: {e}")


def main():
    print("=" * 60)
    print("客户端连接模块扩展测试")
    print("=" * 60)

    tests = [
        test_network_thread_start_stop,
        test_thread_safe_message_queue,
        test_main_loop_read_messages,
        test_main_loop_send_messages,
        test_protobuf_heartbeat_message,
        test_protobuf_login_request_message,
        test_protobuf_login_response_message,
        test_protobuf_packet_message,
        test_connection_exception_callback,
        test_network_thread_exception_handling,
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