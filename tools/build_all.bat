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

REM 创建 build 目录（如果不存在）
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

REM 使用根 CMakeLists.txt 构建所有服务
echo ========================================
echo Building all services with CMake...
echo ========================================
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    cd "%PROJECT_DIR%"
    exit /b 1
)
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed
    cd "%PROJECT_DIR%"
    exit /b 1
)
cd "%PROJECT_DIR%"

echo.
echo ========================================
echo Build successful
echo ========================================
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
echo ========================================
echo All services built successfully
echo ========================================

endlocal
