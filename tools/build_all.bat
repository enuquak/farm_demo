@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Build All Services
echo ========================================

REM 获取脚本所在目录的父目录（项目根目录）
set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"

echo Project Directory: %PROJECT_DIR%
echo.

REM 生成共享常量代码
echo ========================================
echo Generating shared constants...
echo ========================================
python tools\generate_constants.py
if errorlevel 1 (
    echo ERROR: Failed to generate shared constants
    exit /b 1
)
echo.

REM 定义服务列表
set "SERVICES=dbmgr game_server gate_server"

REM 创建 bin 目录（如果不存在）
if not exist "bin" (
    echo Creating bin directory...
    mkdir bin
)

REM 遍历每个服务
set "BUILD_SUCCESS=0"
set "BUILD_FAILED=0"

for %%s in (%SERVICES%) do (
    echo.
    echo ========================================
    echo Building %%s...
    echo ========================================

    REM 检查服务目录是否存在
    if not exist "scripts\server\%%s" (
        echo ERROR: Service directory not found: scripts\server\%%s
        set /a "BUILD_FAILED+=1"
        goto :next_service
    )

    REM 检查 main.cpp 是否存在
    if not exist "scripts\server\%%s\src\main.cpp" (
        echo ERROR: main.cpp not found: scripts\server\%%s\src\main.cpp
        set /a "BUILD_FAILED+=1"
        goto :next_service
    )

    REM 调用构建脚本
    echo Calling build_cpp14.bat for %%s...
    cmd /c "D:\mb_workspace\farm_demo\tool\build_cpp14.bat" "D:\mb_workspace\farm_demo\scripts\server\%%s"

    if errorlevel 1 (
        echo ERROR: Build failed for %%s
        set /a "BUILD_FAILED+=1"
    ) else (
        echo Build successful for %%s

        REM 复制 exe 到 bin 目录
        if exist "scripts\server\%%s\Release\%%s.exe" (
            echo Copying %%s.exe to bin\...
            copy /y "scripts\server\%%s\Release\%%s.exe" "bin\%%s.exe" >nul
            if errorlevel 1 (
                echo WARNING: Failed to copy %%s.exe to bin\
            ) else (
                echo Copied %%s.exe to bin\
            )
        ) else (
            echo WARNING: %%s.exe not found in Release directory
        )

        set /a "BUILD_SUCCESS+=1"
    )

    :next_service
)

echo.
echo ========================================
echo Build Summary
echo ========================================
echo Successful: %BUILD_SUCCESS%
echo Failed: %BUILD_FAILED%
echo.

REM 检查 bin 目录内容
echo Contents of bin\:
dir /b bin\ 2>nul
echo.

REM 检查 DLL 依赖
echo Checking DLL dependencies...
set "MISSING_DLL=0"

REM 检查 libevent DLL
if not exist "bin\event.dll" (
    echo WARNING: Missing event.dll
    set /a "MISSING_DLL+=1"
)
if not exist "bin\event_core.dll" (
    echo WARNING: Missing event_core.dll
    set /a "MISSING_DLL+=1"
)
if not exist "bin\event_extra.dll" (
    echo WARNING: Missing event_extra.dll
    set /a "MISSING_DLL+=1"
)

if %MISSING_DLL% gtr 0 (
    echo.
    echo WARNING: %MISSING_DLL% DLL(s) missing from bin\ directory
    echo Please copy DLLs from C:\libevent_install\lib\ to bin\
    echo.
    echo Required DLLs:
    echo   - event.dll
    echo   - event_core.dll
    echo   - event_extra.dll
) else (
    echo All DLL dependencies found
)

echo.
if %BUILD_FAILED% equ 0 (
    echo ========================================
    echo All services built successfully
    echo ========================================
) else (
    echo ========================================
    echo Build completed with %BUILD_FAILED% failure(s)
    echo ========================================
)

endlocal
