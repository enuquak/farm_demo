@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Force Shutdown
echo ========================================

REM 获取脚本所在目录的父目录（项目根目录）
set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"

echo Project Directory: %PROJECT_DIR%
echo.

REM 定义服务列表
set "SERVICES=dbmgr_server game_server gate_server"

REM 遍历每个服务
for %%s in (%SERVICES%) do (
    set "PID_FILE=runtimeData\%%s.pid"

    echo Processing %%s...

    if not exist "!PID_FILE!" (
        echo   WARNING: PID file not found: !PID_FILE!
        echo   Service %%s may not be running
    ) else (
        REM 读取 PID
        set /p PID=<"!PID_FILE!"
        echo   PID: !PID!

        REM 检查进程是否存在
        tasklist /FI "PID eq !PID!" 2>nul | find /i "%%s.exe" >nul
        if errorlevel 1 (
            echo   Process !PID! is not running
        ) else (
            echo   Force killing process !PID!...
            taskkill /PID !PID! /F >nul 2>&1
            if errorlevel 1 (
                echo   ERROR: Failed to kill process !PID!
            ) else (
                echo   Process !PID! killed successfully
            )
        )
    )
    echo.
)

REM 清理 PID 文件
echo Cleaning up PID files...
for %%s in (%SERVICES%) do (
    set "PID_FILE=runtimeData\%%s.pid"
    if exist "!PID_FILE!" (
        del /f "!PID_FILE!" >nul 2>&1
        if errorlevel 1 (
            echo   WARNING: Failed to delete !PID_FILE!
        ) else (
            echo   Deleted !PID_FILE!
        )
    )
)

echo.
echo ========================================
echo Force shutdown complete
echo ========================================

endlocal
