"""
Gate Server integration test.

Tests:
TC-001: Server startup and port listening
TC-002: Basic connection and disconnection
TC-003: Heartbeat message round-trip
TC-004: Login flow
TC-005: Sticky packet handling (multiple messages in one send)
TC-006: Split packet handling (one message across multiple sends)
"""

import socket
import struct
import subprocess
import sys
import os
import time
import signal

# Add protobuf generated path
PROTO_GEN_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "..", "common", "proto", "generated")
sys.path.insert(0, os.path.abspath(PROTO_GEN_DIR))

import base_pb2

# Constants
MSG_ID_HEARTBEAT = 1001
MSG_ID_HEARTBEAT_RESP = 1002
MSG_ID_LOGIN_REQ = 2001
MSG_ID_LOGIN_RESP = 2002

SERVER_EXE = os.path.join(os.path.dirname(__file__), "..", "Release", "Release", "gate_server.exe")
TEST_PORT = 18080
TEST_IP = "127.0.0.1"


def pack_message(msg_id: int, payload: bytes) -> bytes:
    """Pack a message in the wire format: [4B length][4B MsgID][Payload]"""
    body_len = 4 + len(payload)  # MsgID + Payload
    return struct.pack("!I", body_len) + struct.pack("!I", msg_id) + payload


def recv_exact(sock: socket.socket, n: int) -> bytes:
    """Receive exactly n bytes from socket."""
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("Connection closed")
        data += chunk
    return data


def recv_message(sock: socket.socket) -> tuple:
    """Receive one message from socket. Returns (msg_id, payload)."""
    header = recv_exact(sock, 8)
    body_len = struct.unpack("!I", header[:4])[0]
    msg_id = struct.unpack("!I", header[4:8])[0]
    payload_len = body_len - 4
    payload = recv_exact(sock, payload_len) if payload_len > 0 else b""
    return msg_id, payload


def start_server(port: int) -> subprocess.Popen:
    """Start the gate server process."""
    if not os.path.exists(SERVER_EXE):
        raise FileNotFoundError(f"Server executable not found: {SERVER_EXE}")

    proc = subprocess.Popen(
        [SERVER_EXE, str(port)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    # Wait for server to start
    time.sleep(1.0)
    if proc.poll() is not None:
        raise RuntimeError(f"Server exited immediately with code {proc.returncode}")
    return proc


def stop_server(proc: subprocess.Popen):
    """Stop the server process."""
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


def connect_client(port: int) -> socket.socket:
    """Connect a TCP client to the server."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    sock.connect((TEST_IP, port))
    return sock


def wait_for_port(port: int, timeout: float = 5.0) -> bool:
    """Wait until a port becomes available."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(0.5)
            sock.connect((TEST_IP, port))
            sock.close()
            return True
        except (ConnectionRefusedError, OSError):
            time.sleep(0.1)
    return False


def test_server_startup():
    """TC-001: Server startup and port listening."""
    print("TC-001: Server startup and port listening ... ", end="")
    proc = start_server(TEST_PORT)
    try:
        assert wait_for_port(TEST_PORT), "Server port not reachable after startup"
        sock = connect_client(TEST_PORT)
        sock.close()
        print("PASS")
    finally:
        stop_server(proc)


def test_basic_connection():
    """TC-002: Basic connection and disconnection."""
    print("TC-002: Basic connection and disconnection ... ", end="")
    proc = start_server(TEST_PORT)
    try:
        assert wait_for_port(TEST_PORT), "Server port not reachable"
        sock = connect_client(TEST_PORT)
        time.sleep(0.2)
        sock.close()
        time.sleep(0.2)
        # Server should still be running after client disconnect
        assert proc.poll() is None, "Server crashed after client disconnect"
        print("PASS")
    finally:
        stop_server(proc)


def test_heartbeat():
    """TC-003: Heartbeat message round-trip."""
    print("TC-003: Heartbeat message round-trip ... ", end="")
    proc = start_server(TEST_PORT)
    try:
        assert wait_for_port(TEST_PORT), "Server port not reachable"
        sock = connect_client(TEST_PORT)

        # Send heartbeat
        hb = base_pb2.Heartbeat()
        hb.timestamp = int(time.time())
        payload = hb.SerializeToString()
        sock.sendall(pack_message(MSG_ID_HEARTBEAT, payload))

        # Receive heartbeat response
        msg_id, resp_payload = recv_message(sock)
        assert msg_id == MSG_ID_HEARTBEAT_RESP, f"Expected heartbeat resp ({MSG_ID_HEARTBEAT_RESP}), got {msg_id}"

        hb_resp = base_pb2.Heartbeat()
        hb_resp.ParseFromString(resp_payload)
        assert hb_resp.timestamp > 0, "Heartbeat response timestamp should be > 0"

        sock.close()
        print("PASS")
    finally:
        stop_server(proc)


def test_login():
    """TC-004: Login flow."""
    print("TC-004: Login flow ... ", end="")
    proc = start_server(TEST_PORT)
    try:
        assert wait_for_port(TEST_PORT), "Server port not reachable"
        sock = connect_client(TEST_PORT)

        # Send login request
        login_req = base_pb2.LoginReq()
        login_req.token = "test_token_123"
        payload = login_req.SerializeToString()
        sock.sendall(pack_message(MSG_ID_LOGIN_REQ, payload))

        # Receive login response
        msg_id, resp_payload = recv_message(sock)
        assert msg_id == MSG_ID_LOGIN_RESP, f"Expected login resp ({MSG_ID_LOGIN_RESP}), got {msg_id}"

        login_resp = base_pb2.LoginResp()
        login_resp.ParseFromString(resp_payload)
        assert login_resp.code == 0, f"Login failed with code {login_resp.code}: {login_resp.msg}"
        assert login_resp.msg == "login success", f"Unexpected login msg: {login_resp.msg}"

        sock.close()
        print("PASS")
    finally:
        stop_server(proc)


def test_sticky_packet():
    """TC-005: Sticky packet handling (multiple messages in one send)."""
    print("TC-005: Sticky packet handling ... ", end="")
    proc = start_server(TEST_PORT)
    try:
        assert wait_for_port(TEST_PORT), "Server port not reachable"
        sock = connect_client(TEST_PORT)

        # Prepare 3 heartbeat messages concatenated
        messages = []
        for i in range(3):
            hb = base_pb2.Heartbeat()
            hb.timestamp = int(time.time()) + i
            payload = hb.SerializeToString()
            messages.append(pack_message(MSG_ID_HEARTBEAT, payload))

        # Send all at once (sticky packet)
        sock.sendall(b"".join(messages))

        # Should receive 3 heartbeat responses
        responses = []
        sock.settimeout(3.0)
        for _ in range(3):
            msg_id, resp_payload = recv_message(sock)
            responses.append((msg_id, resp_payload))

        for i, (msg_id, _) in enumerate(responses):
            assert msg_id == MSG_ID_HEARTBEAT_RESP, f"Response {i}: expected {MSG_ID_HEARTBEAT_RESP}, got {msg_id}"

        sock.close()
        print("PASS")
    finally:
        stop_server(proc)


def test_split_packet():
    """TC-006: Split packet handling (one message across multiple sends)."""
    print("TC-006: Split packet handling ... ", end="")
    proc = start_server(TEST_PORT)
    try:
        assert wait_for_port(TEST_PORT), "Server port not reachable"
        sock = connect_client(TEST_PORT)

        # Prepare one heartbeat message
        hb = base_pb2.Heartbeat()
        hb.timestamp = int(time.time())
        payload = hb.SerializeToString()
        msg_bytes = pack_message(MSG_ID_HEARTBEAT, payload)

        # Send in 3 parts with delays (split packet)
        part1 = msg_bytes[:4]      # length header
        part2 = msg_bytes[4:6]     # partial MsgID
        part3 = msg_bytes[6:]      # rest of MsgID + payload

        sock.sendall(part1)
        time.sleep(0.1)
        sock.sendall(part2)
        time.sleep(0.1)
        sock.sendall(part3)

        # Should still receive one heartbeat response
        sock.settimeout(3.0)
        msg_id, resp_payload = recv_message(sock)
        assert msg_id == MSG_ID_HEARTBEAT_RESP, f"Expected {MSG_ID_HEARTBEAT_RESP}, got {msg_id}"

        hb_resp = base_pb2.Heartbeat()
        hb_resp.ParseFromString(resp_payload)
        assert hb_resp.timestamp > 0, "Heartbeat response timestamp should be > 0"

        sock.close()
        print("PASS")
    finally:
        stop_server(proc)


def main():
    print("=" * 60)
    print("Gate Server Integration Tests")
    print("=" * 60)

    # Pre-check: verify exe exists
    if not os.path.exists(SERVER_EXE):
        print(f"FAIL: Server executable not found at {SERVER_EXE}")
        print("Please build the project first: cmake --build . --config Release")
        sys.exit(1)

    tests = [
        test_server_startup,
        test_basic_connection,
        test_heartbeat,
        test_login,
        test_sticky_packet,
        test_split_packet,
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
    print(f"Results: {passed} passed, {failed} failed out of {len(tests)} tests")
    if errors:
        print("\nFailed tests:")
        for name, err in errors:
            print(f"  {name}: {err}")
    print("=" * 60)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
