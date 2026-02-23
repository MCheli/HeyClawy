@echo off
REM Build HeyClawy for SenseCAP Watcher
REM Target: ESP32-S3

echo ============================================
echo  HeyClawy - SenseCAP Watcher
echo ============================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

REM Clean if switching from a different target/board
REM Check if we need to switch targets
set NEED_SWITCH=0
if not exist sdkconfig set NEED_SWITCH=1
if exist sdkconfig (
    findstr /C:"CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER=y" sdkconfig >nul 2>&1
    if errorlevel 1 set NEED_SWITCH=1
)

if "%NEED_SWITCH%"=="1" (
    echo Switching to ESP32-S3 / SenseCAP target. Cleaning build...
    rmdir /s /q build 2>nul
    del sdkconfig 2>nul
    REM Create fake cmake build dir so set-target doesn't complain
    mkdir build 2>nul
    echo. > build\CMakeCache.txt
    call idf.py set-target esp32s3
    if errorlevel 1 (
        echo ERROR: idf.py set-target esp32s3 failed
        exit /b 1
    )
)

REM Ensure board selection is SenseCAP Watcher
findstr /C:"CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER=y" sdkconfig >nul 2>&1
if errorlevel 1 (
    echo Updating board selection to SenseCAP Watcher...
    powershell -Command "$c=Get-Content sdkconfig; $c=$c -replace 'CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO=y','# CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO is not set'; $c=$c -replace 'CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2=y','# CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2 is not set'; $c=$c -replace '# CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER is not set','CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER=y'; Set-Content sdkconfig $c"
)

REM Fix console and flash for SenseCAP
powershell -Command "$c=Get-Content sdkconfig; $c=$c -replace '^CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y$','CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y'; $c=$c -replace '^CONFIG_ESPTOOLPY_FLASHSIZE=\"16MB\"$','CONFIG_ESPTOOLPY_FLASHSIZE=\"8MB\"'; $c=$c -replace '^# CONFIG_ESP_CONSOLE_UART_DEFAULT is not set$','CONFIG_ESP_CONSOLE_UART_DEFAULT=y'; $c=$c -replace '^CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y$','# CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG is not set'; Set-Content sdkconfig $c"

echo Board: SenseCAP Watcher (ESP32-S3, 8MB flash, UART console)
echo.

set ESPPORT=COM3
idf.py build

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================
    echo  BUILD SUCCESSFUL
    echo  Flash with: set ESPPORT=COM3 ^& idf.py flash monitor
    echo ============================================
) else (
    echo.
    echo BUILD FAILED
    exit /b 1
)
