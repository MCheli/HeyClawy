@echo off
REM Build script for M5StickC Plus2 board
REM Target: ESP32 (not S3!)

echo ========================================
echo  Building HeyClawy for M5StickC Plus2
echo ========================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

REM Clean if switching from a different target
REM M5StickCPlus2 uses ESP32 (not S3) — check both target and board
REM Check if we need to switch targets
set NEED_SWITCH=0
if not exist sdkconfig set NEED_SWITCH=1
if exist sdkconfig (
    findstr /C:"CONFIG_IDF_TARGET_ESP32S3=y" sdkconfig >nul 2>&1
    if not errorlevel 1 set NEED_SWITCH=1
)

if "%NEED_SWITCH%"=="1" (
    echo Switching to ESP32 target. Cleaning build...
    rmdir /s /q build 2>nul
    del sdkconfig 2>nul
    REM Create fake cmake build dir so set-target doesn't complain
    mkdir build 2>nul
    echo. > build\CMakeCache.txt
    call idf.py set-target esp32
    if errorlevel 1 (
        echo ERROR: idf.py set-target esp32 failed
        exit /b 1
    )
)

REM Ensure board selection is M5StickCPlus2
findstr /C:"CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2=y" sdkconfig >nul 2>&1
if errorlevel 1 (
    echo Updating board selection to M5StickCPlus2...
    powershell -Command "$c=Get-Content sdkconfig; $c=$c -replace 'CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER=y','# CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER is not set'; $c=$c -replace 'CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO=y','# CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO is not set'; $c=$c -replace '# CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2 is not set','CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2=y'; Set-Content sdkconfig $c"
)

echo Board: M5StickC Plus2 (ESP32, 8MB flash)
echo.

set ESPPORT=COM17
call idf.py build

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo  BUILD SUCCESSFUL
    echo  Flash with: set ESPPORT=COM17 ^& idf.py flash monitor
    echo ========================================
) else (
    echo.
    echo BUILD FAILED
    exit /b 1
)
