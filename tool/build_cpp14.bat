@echo off
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Usage: build_cpp14.bat folder_path
    echo Example: build_cpp14.bat D:\test\my_project
    exit /b 1
)

set "TARGET_DIR=%~1"

echo  ==============================
echo   VS2022 C++ Build Script
echo   Build Dir: %TARGET_DIR%
echo  ==============================

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cd /d "%TARGET_DIR%"

REM Check if CMakeLists.txt exists, use CMake build
if exist "CMakeLists.txt" (
    echo Found CMakeLists.txt, using CMake build...
    if not exist "build" mkdir build
    cd build
    cmake .. -G "Visual Studio 17 2022" -A x64
    if %errorlevel% neq 0 (
        echo CMake configure failed!
        pause
        exit /b 1
    )
    cmake --build . --config Release
    if %errorlevel% equ 0 (
        echo.
        echo Build success!

        REM Copy exe to parent Release directory
        if exist "Release\gate_server.exe" (
            if not exist "..\Release" mkdir "..\Release"
            copy /Y "Release\gate_server.exe" "..\Release\" >nul
            echo Copied gate_server.exe to ..\Release\
        )

        REM Copy libevent DLLs to Release directory
        set "LIBEVENT_ROOT=C:\libevent_install"
        if exist "%LIBEVENT_ROOT%\lib\event.dll" (
            if not exist "..\Release" mkdir "..\Release"
            copy /Y "%LIBEVENT_ROOT%\lib\event.dll" "..\Release\" >nul
            copy /Y "%LIBEVENT_ROOT%\lib\event_core.dll" "..\Release\" >nul
            copy /Y "%LIBEVENT_ROOT%\lib\event_extra.dll" "..\Release\" >nul
            echo Copied libevent DLLs to ..\Release\
        )
    ) else (
        echo.
        echo Build failed!
    )
) else (
    REM Fallback to simple cl compilation
    echo No CMakeLists.txt found, using simple compilation...
    cl /std:c++14 src\main.cpp
    if %errorlevel% equ 0 (
        echo.
        echo Build success! main.exe generated
    ) else (
        echo.
        echo Build failed!
    )
)

pause
