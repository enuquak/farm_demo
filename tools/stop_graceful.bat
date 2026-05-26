@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Graceful Shutdown
echo ========================================

REM 获取脚本所在目录的父目录（项目根目录）
set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"

echo Project Directory: %PROJECT_DIR%
echo.

REM 检查 gate_server PID 文件
set "GATE_PID_FILE=runtimeData\gate_server.pid"
if not exist "%GATE_PID_FILE%" (
    echo WARNING: gate_server PID file not found: %GATE_PID_FILE%
    echo Trying force shutdown instead...
    call "%~dp0stop_force.bat"
    exit /b 0
)

REM 读取 gate_server PID
set /p GATE_PID=<"%GATE_PID_FILE%"
echo gate_server PID: %GATE_PID%

REM 检查 gate_server 是否在运行
tasklist /FI "PID eq %GATE_PID%" 2>nul | find /i "gate_server.exe" >nul
if errorlevel 1 (
    echo WARNING: gate_server (PID: %GATE_PID%) is not running
    echo Trying force shutdown instead...
    call "%~dp0stop_force.bat"
    exit /b 0
)

echo.
echo Sending shutdown message to gate_server...

REM 使用 PowerShell 发送 TCP 消息
REM MSG_ID_SHUTDOWN = 5001
REM 消息格式: Length(4 bytes) + MsgID(4 bytes) + Payload(JSON)
REM Payload: {"reason":"graceful shutdown","timeout_ms":60000}

set "SHUTDOWN_JSON={\"reason\":\"graceful shutdown\",\"timeout_ms\":60000}"

REM 计算 JSON 长度
set "JSON_LEN=0"
for /f "delims=" %%a in ('powershell -Command "'%SHUTDOWN_JSON%'.Length"') do set "JSON_LEN=%%a"

REM 计算消息总长度 (4 + 4 + JSON_LEN)
set /a "MSG_LEN=4+4+%JSON_LEN%"

echo JSON Payload: %SHUTDOWN_JSON%
echo JSON Length: %JSON_LEN%
echo Message Length: %MSG_LEN%

REM 发送 TCP 消息到 gate_server (默认端口 8080)
echo.
echo Connecting to gate_server at 127.0.0.1:8080...

REM 使用 PowerShell 发送二进制 TCP 消息
powershell -Command "
    try {
        $client = New-Object System.Net.Sockets.TcpClient('127.0.0.1', 8080)
        $stream = $client.GetStream()

        # 构造消息
        $json = '%SHUTDOWN_JSON%'
        $jsonBytes = [System.Text.Encoding]::UTF8.GetBytes($json)
        $msgId = [BitConverter]::GetBytes([uint32]5001)
        $msgLen = [BitConverter]::GetBytes([uint32]($jsonBytes.Length + 4))

        # 反转字节序（网络字节序 = 大端）
        [Array]::Reverse($msgLen)
        [Array]::Reverse($msgId)

        # 发送消息
        $stream.Write($msgLen, 0, 4)
        $stream.Write($msgId, 0, 4)
        $stream.Write($jsonBytes, 0, $jsonBytes.Length)
        $stream.Flush()

        Write-Host 'Shutdown message sent successfully'

        # 等待响应（可选）
        Start-Sleep -Seconds 2

        $client.Close()
    } catch {
        Write-Host \"Error: $_\"
        exit 1
    }
"

if errorlevel 1 (
    echo ERROR: Failed to send shutdown message
    echo.
    echo Falling back to force shutdown...
    call "%~dp0stop_force.bat"
    exit /b 0
)

echo.
echo Waiting for servers to shutdown...
echo (Timeout: 60 seconds)

REM 等待服务器退出
set /a "WAIT_COUNT=0"
set /a "MAX_WAIT=60"

:wait_loop
if %WAIT_COUNT% geq %MAX_WAIT% (
    echo.
    echo TIMEOUT: Servers did not shutdown within %MAX_WAIT% seconds
    echo Forcing shutdown...
    call "%~dp0stop_force.bat"
    exit /b 0
)

REM 检查是否所有服务器都已退出
set "ALL_STOPPED=1"

REM 检查 dbmgr
if exist "runtimeData\dbmgr.pid" (
    set /p DBMGR_PID=<"runtimeData\dbmgr.pid"
    tasklist /FI "PID eq !DBMGR_PID!" 2>nul | find /i "dbmgr.exe" >nul
    if not errorlevel 1 set "ALL_STOPPED=0"
)

REM 检查 game_server
if exist "runtimeData\game_server.pid" (
    set /p GAME_PID=<"runtimeData\game_server.pid"
    tasklist /FI "PID eq !GAME_PID!" 2>nul | find /i "game_server.exe" >nul
    if not errorlevel 1 set "ALL_STOPPED=0"
)

REM 检查 gate_server
if exist "runtimeData\gate_server.pid" (
    set /p GATE_PID=<"runtimeData\gate_server.pid"
    tasklist /FI "PID eq !GATE_PID!" 2>nul | find /i "gate_server.exe" >nul
    if not errorlevel 1 set "ALL_STOPPED=0"
)

if %ALL_STOPPED% equ 1 (
    echo.
    echo All servers stopped successfully
    goto :done
)

set /a "WAIT_COUNT+=1"
echo.
echo Waiting... (%WAIT_COUNT%/%MAX_WAIT%)
timeout /t 1 /nobreak >nul
goto :wait_loop

:done
echo.
echo ========================================
echo Graceful shutdown complete
echo ========================================

endlocal
