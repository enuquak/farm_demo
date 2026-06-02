@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Starting All Servers
echo ========================================

REM 获取脚本所在目录的父目录（项目根目录）
set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"

echo Project Directory: %PROJECT_DIR%
echo.

REM 启动 dbmgr_server
echo [1/3] Starting dbmgr_server...
start "dbmgr_server" /D "%PROJECT_DIR%" bin\dbmgr_server.exe --config config\dbmgr.json
if errorlevel 1 (
    echo ERROR: Failed to start dbmgr_server
    exit /b 1
)
echo dbmgr_server started, waiting 2 seconds...
timeout /t 2 /nobreak >nul

REM 启动 game_server
echo [2/3] Starting game_server...
start "game_server" /D "%PROJECT_DIR%" bin\game_server.exe --config config\game_server.json
if errorlevel 1 (
    echo ERROR: Failed to start game_server
    exit /b 1
)
echo game_server started, waiting 2 seconds...
timeout /t 2 /nobreak >nul

REM 启动 gate_server
echo [3/3] Starting gate_server...
start "gate_server" /D "%PROJECT_DIR%" bin\gate_server.exe --config config\gate_server.json
if errorlevel 1 (
    echo ERROR: Failed to start gate_server
    exit /b 1
)
echo gate_server started

echo.
echo ========================================
echo All servers started
echo ========================================
echo.
echo Services:
echo   - dbmgr_server: bin\dbmgr_server.exe --config config\dbmgr.json
echo   - game_server: bin\game_server.exe --config config\game_server.json
echo   - gate_server: bin\gate_server.exe --config config\gate_server.json
echo.
echo PID files:
echo   - runtimeData\dbmgr_server.pid
echo   - runtimeData\game_server.pid
echo   - runtimeData\gate_server.pid
echo.
echo Use stop_graceful.bat or stop_force.bat to stop servers.
echo ========================================

endlocal
