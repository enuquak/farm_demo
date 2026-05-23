#!/usr/bin/env python3
"""
Wrapper to start gate_server.exe with WSAStartup initialized.
On Windows, WSAStartup must be called before any Winsock functions.
This script initializes Winsock via ctypes, then starts the server.
"""

import subprocess
import sys
import time
import ctypes
import os

# Initialize Winsock via ctypes
if sys.platform == 'win32':
    # Call WSAStartup
    wsa_data = ctypes.create_string_buffer(408)
    ret = ctypes.windll.ws2_32.WSAStartup(0x0202, wsa_data)
    if ret != 0:
        print(f'WSAStartup failed with error: {ret}')
        sys.exit(1)
    print(f'WSAStartup initialized successfully')

# Server executable path
server_exe = r'D:\mb_workspace\farm_demo\scripts\server\gate_server\Release\Release\gate_server.exe'
output_file = r'D:\mb_workspace\farm_demo\agent_workspace_data\server_output.txt'

# Start the server
print(f'Starting server: {server_exe} 8080')
with open(output_file, 'w') as f:
    proc = subprocess.Popen(
        [server_exe, '8080'],
        stdout=f,
        stderr=subprocess.STDOUT,
        cwd=os.path.dirname(server_exe)
    )

print(f'Server process started, PID: {proc.pid}')

# Wait for server to initialize
time.sleep(3)

# Check if process is still alive
if proc.poll() is not None:
    print(f'Server process exited with code: {proc.returncode}')
    with open(output_file, 'r') as f:
        print(f.read())
    sys.exit(1)
else:
    print(f'Server process is running (PID: {proc.pid})')
    print(f'Output file: {output_file}')
