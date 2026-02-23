@echo off
REM Build HeyClawy for Waveshare ESP32-S3-AUDIO-Board
REM Target: ESP32-S3, 16MB flash, USB JTAG console

setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ============================================
echo  HeyClawy - Waveshare ESP32-S3-AUDIO-Board
echo ============================================

REM Check if we need to switch to ESP32-S3 or switch board
set NEED_SWITCH=0
if not exist sdkconfig set NEED_SWITCH=1
if exist sdkconfig (
    findstr /C:"CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO=y" sdkconfig >nul 2>&1
    if errorlevel 1 set NEED_SWITCH=1
)

if "%NEED_SWITCH%"=="1" (
    echo Switching to ESP32-S3 / Waveshare Audio target. Cleaning build...
    rmdir /s /q build 2>nul
    del sdkconfig 2>nul
    mkdir build 2>nul
    echo. > build\CMakeCache.txt
    call idf.py set-target esp32s3
    if errorlevel 1 (
        echo ERROR: idf.py set-target esp32s3 failed
        exit /b 1
    )
)

REM Ensure board selection is Waveshare Audio
findstr /C:"CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO=y" sdkconfig >nul 2>&1
if errorlevel 1 (
    echo Updating board selection to Waveshare Audio...
    powershell -Command "$c=Get-Content sdkconfig; $c=$c -replace 'CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER=y','# CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER is not set'; $c=$c -replace 'CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2=y','# CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2 is not set'; $c=$c -replace '# CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO is not set','CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO=y'; Set-Content sdkconfig $c"
)

REM Fix flash size and console for Waveshare Audio Board
powershell -Command "$c=Get-Content sdkconfig; $c=$c -replace 'CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y','CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y'; $c=$c -replace 'CONFIG_ESPTOOLPY_FLASHSIZE=""8MB""','CONFIG_ESPTOOLPY_FLASHSIZE=""16MB""'; Set-Content sdkconfig $c"

echo Board: Waveshare ESP32-S3-AUDIO-Board (ESP32-S3, 16MB flash)
echo.

set ESPPORT=COM16
idf.py build

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================
    echo  BUILD SUCCESSFUL
    echo  Flash with: set ESPPORT=COM16 ^& idf.py flash monitor
    echo ============================================
) else (
    echo.
    echo BUILD FAILED
    exit /b 1
)
