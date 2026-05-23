#!/usr/bin/env python3
"""
Gate Server Functional Test Script
===================================
Tests:
  TC-001: Server startup and port listening
  TC-002: TCP connection establishment
  TC-003: Heartbeat message send/receive
  TC-004: Login message send/receive
  TC-005: Sticky packet handling (multiple messages in one send)
  TC-006: Packet splitting (one message sent in parts)
"""

import socket
import struct
import time
import sys
import os

# Add proto generated path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'common', 'proto', 'generated'))
sys.path.insert(0, 'D:/mb_workspace/farm_demo/scripts/common/proto/generated')

import base_pb2

# ─── Constants ────────────────────────────────────────────────────────────────
SERVER_HOST = 'localhost'
SERVER_PORT = 8080
TIMEOUT = 5  # seconds

MSG_ID_HEARTBEAT = 1001
MSG_ID_HEARTBEAT_RESP = 1002
MSG_ID_LOGIN_REQ = 2001
MSG_ID_LOGIN_RESP = 2002

# ─── Protocol Helpers ─────────────────────────────────────────────────────────

def pack_message(msg_id: int, payload: bytes) -> bytes:
    """
    Pack a message into the wire format:
    [4B length (network order)] [4B msg_id (network order)] [payload]
    length = 4 (msg_id) + len(payload)
    """
    body_len = 4 + len(payload)
    return struct.pack('!II', body_len, msg_id) + payload


def recv_exact(sock: socket.socket, n: int) -> bytes:
    """Receive exactly n bytes from socket."""
    data = b''
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError('Connection closed while receiving data')
        data += chunk
    return data


def recv_message(sock: socket.socket) -> tuple:
    """
    Receive one message from socket.
    Returns (msg_id, payload_bytes).
    """
    header = recv_exact(sock, 8)
    body_len, msg_id = struct.unpack('!II', header)
    payload_len = body_len - 4
    if payload_len > 0:
        payload = recv_exact(sock, payload_len)
    else:
        payload = b''
    return msg_id, payload


# ─── Test Results ─────────────────────────────────────────────────────────────

class TestResult:
    def __init__(self, test_id: str, name: str):
        self.test_id = test_id
        self.name = name
        self.status = 'PENDING'
        self.detail = ''

    def pass_(self, detail: str = ''):
        self.status = 'PASS'
        self.detail = detail

    def fail_(self, detail: str = ''):
        self.status = 'FAIL'
        self.detail = detail

    def skip(self, detail: str = ''):
        self.status = 'SKIP'
        self.detail = detail

    def __str__(self):
        return f'[{self.status}] {self.test_id}: {self.name} - {self.detail}'


results: list[TestResult] = []


# ─── Test Cases ───────────────────────────────────────────────────────────────

def test_001_server_startup():
    """TC-001: Verify server is listening on port 8080."""
    t = TestResult('TC-001', 'Server startup and port listening')
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((SERVER_HOST, SERVER_PORT))
        t.pass_(f'Successfully connected to {SERVER_HOST}:{SERVER_PORT}')
        s.close()
    except ConnectionRefusedError:
        t.fail_(f'Connection refused on {SERVER_HOST}:{SERVER_PORT}')
    except socket.timeout:
        t.fail_(f'Connection timed out on {SERVER_HOST}:{SERVER_PORT}')
    except Exception as e:
        t.fail_(f'Unexpected error: {e}')
    results.append(t)
    return t


def test_002_tcp_connection():
    """TC-002: Verify TCP connection can be established and closed cleanly."""
    t = TestResult('TC-002', 'TCP connection establishment')
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((SERVER_HOST, SERVER_PORT))
        t.pass_(f'TCP connection established to {SERVER_HOST}:{SERVER_PORT}')
        s.close()
    except Exception as e:
        t.fail_(f'Failed to establish TCP connection: {e}')
    results.append(t)
    return t


def test_003_heartbeat():
    """TC-003: Verify heartbeat message send and receive."""
    t = TestResult('TC-003', 'Heartbeat message send/receive')
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((SERVER_HOST, SERVER_PORT))

        # Build heartbeat payload
        hb = base_pb2.Heartbeat()
        hb.timestamp = int(time.time() * 1000)
        payload = hb.SerializeToString()

        # Send heartbeat request (MsgID=1001)
        msg = pack_message(MSG_ID_HEARTBEAT, payload)
        s.sendall(msg)

        # Receive response
        resp_id, resp_payload = recv_message(s)

        if resp_id != MSG_ID_HEARTBEAT_RESP:
            t.fail_(f'Expected MsgID {MSG_ID_HEARTBEAT_RESP}, got {resp_id}')
        else:
            resp_hb = base_pb2.Heartbeat()
            resp_hb.ParseFromString(resp_payload)
            t.pass_(f'Heartbeat response received. MsgID={resp_id}, timestamp={resp_hb.timestamp}')

        s.close()
    except socket.timeout:
        t.fail_('Timeout waiting for heartbeat response')
    except Exception as e:
        t.fail_(f'Error: {e}')
    results.append(t)
    return t


def test_004_login():
    """TC-004: Verify login message send and receive."""
    t = TestResult('TC-004', 'Login message send/receive')
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((SERVER_HOST, SERVER_PORT))

        # Build login request
        login_req = base_pb2.LoginReq()
        login_req.token = 'test_token_12345'
        payload = login_req.SerializeToString()

        # Send login request (MsgID=2001)
        msg = pack_message(MSG_ID_LOGIN_REQ, payload)
        s.sendall(msg)

        # Receive response
        resp_id, resp_payload = recv_message(s)

        if resp_id != MSG_ID_LOGIN_RESP:
            t.fail_(f'Expected MsgID {MSG_ID_LOGIN_RESP}, got {resp_id}')
        else:
            login_resp = base_pb2.LoginResp()
            login_resp.ParseFromString(resp_payload)
            if login_resp.code == 0 and login_resp.msg == 'login success':
                t.pass_(f'Login response: code={login_resp.code}, msg="{login_resp.msg}"')
            else:
                t.fail_(f'Unexpected login response: code={login_resp.code}, msg="{login_resp.msg}"')

        s.close()
    except socket.timeout:
        t.fail_('Timeout waiting for login response')
    except Exception as e:
        t.fail_(f'Error: {e}')
    results.append(t)
    return t


def test_005_sticky_packet():
    """TC-005: Verify sticky packet handling (send multiple messages at once)."""
    t = TestResult('TC-005', 'Sticky packet handling (multiple messages in one send)')
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((SERVER_HOST, SERVER_PORT))

        # Build two heartbeat messages
        hb1 = base_pb2.Heartbeat()
        hb1.timestamp = int(time.time() * 1000)
        payload1 = hb1.SerializeToString()

        hb2 = base_pb2.Heartbeat()
        hb2.timestamp = int(time.time() * 1000) + 1000
        payload2 = hb2.SerializeToString()

        # Concatenate two messages and send at once (sticky packet)
        msg1 = pack_message(MSG_ID_HEARTBEAT, payload1)
        msg2 = pack_message(MSG_ID_HEARTBEAT, payload2)
        s.sendall(msg1 + msg2)

        # Receive two responses
        responses = []
        for i in range(2):
            resp_id, resp_payload = recv_message(s)
            responses.append((resp_id, resp_payload))

        # Validate responses
        all_ok = True
        errors = []
        for i, (rid, rp) in enumerate(responses):
            if rid != MSG_ID_HEARTBEAT_RESP:
                all_ok = False
                errors.append(f'Response {i+1}: expected MsgID {MSG_ID_HEARTBEAT_RESP}, got {rid}')

        if all_ok:
            t.pass_(f'Sent 2 messages in one send, received 2 responses correctly. MsgIDs={[r[0] for r in responses]}')
        else:
            t.fail_(f'Sticky packet test failed: {"; ".join(errors)}')

        s.close()
    except socket.timeout:
        t.fail_('Timeout waiting for responses')
    except Exception as e:
        t.fail_(f'Error: {e}')
    results.append(t)
    return t


def test_006_split_packet():
    """TC-006: Verify split packet handling (send one message in parts)."""
    t = TestResult('TC-006', 'Split packet handling (message sent in parts)')
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(TIMEOUT)
        s.connect((SERVER_HOST, SERVER_PORT))

        # Build a login request message
        login_req = base_pb2.LoginReq()
        login_req.token = 'split_test_token'
        payload = login_req.SerializeToString()
        full_msg = pack_message(MSG_ID_LOGIN_REQ, payload)

        # Split the message into 3 parts
        part1 = full_msg[:4]      # First 4 bytes (partial length header)
        part2 = full_msg[4:8]     # Remaining length header + partial msg_id
        part3 = full_msg[8:]      # Rest of the message

        # Send each part with a small delay
        s.sendall(part1)
        time.sleep(0.05)
        s.sendall(part2)
        time.sleep(0.05)
        s.sendall(part3)

        # Receive response
        resp_id, resp_payload = recv_message(s)

        if resp_id != MSG_ID_LOGIN_RESP:
            t.fail_(f'Expected MsgID {MSG_ID_LOGIN_RESP}, got {resp_id}')
        else:
            login_resp = base_pb2.LoginResp()
            login_resp.ParseFromString(resp_payload)
            if login_resp.code == 0 and login_resp.msg == 'login success':
                t.pass_(f'Split packet handled correctly. Response: code={login_resp.code}, msg="{login_resp.msg}"')
            else:
                t.fail_(f'Unexpected login response: code={login_resp.code}, msg="{login_resp.msg}"')

        s.close()
    except socket.timeout:
        t.fail_('Timeout waiting for response after split packet send')
    except Exception as e:
        t.fail_(f'Error: {e}')
    results.append(t)
    return t


# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    print('=' * 70)
    print('Gate Server Functional Test')
    print('=' * 70)
    print()

    # Run tests in order
    test_cases = [
        test_001_server_startup,
        test_002_tcp_connection,
        test_003_heartbeat,
        test_004_login,
        test_005_sticky_packet,
        test_006_split_packet,
    ]

    for test_func in test_cases:
        result = test_func()
        print(f'  {result}')
        # If server not running, skip remaining tests
        if result.status == 'FAIL' and result.test_id == 'TC-001':
            print('\n  Server not running, skipping remaining tests.')
            for remaining in test_cases[len(results):]:
                r = remaining.__func__() if hasattr(remaining, '__func__') else None
                if r:
                    r.skip('Server not available')
                    print(f'  {r}')
            break

    # Print summary
    print()
    print('=' * 70)
    print('Test Summary')
    print('=' * 70)
    total = len(results)
    passed = sum(1 for r in results if r.status == 'PASS')
    failed = sum(1 for r in results if r.status == 'FAIL')
    skipped = sum(1 for r in results if r.status == 'SKIP')

    for r in results:
        print(f'  {r}')

    print()
    print(f'Total: {total} | Passed: {passed} | Failed: {failed} | Skipped: {skipped}')
    overall = 'PASS' if failed == 0 else 'FAIL'
    print(f'Overall: {overall}')
    print('=' * 70)

    # Return results for report generation
    return results, overall


if __name__ == '__main__':
    results, overall = main()
    # Exit with code 0 if all passed, 1 if any failed
    sys.exit(0 if overall == 'PASS' else 1)
