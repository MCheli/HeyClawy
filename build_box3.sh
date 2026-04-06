#!/usr/bin/env bash
# Build HeyClawy for ESP32-S3-BOX-3
# Target: ESP32-S3, 16MB flash, 16MB PSRAM, USB JTAG console

set -e
cd "$(dirname "$0")"

echo "============================================"
echo " HeyClawy - ESP32-S3-BOX-3"
echo "============================================"

# Check if we need to switch to ESP32-S3 or switch board
NEED_SWITCH=0
if [ ! -f sdkconfig ]; then
    NEED_SWITCH=1
elif ! grep -q "CONFIG_HEYCLAWY_BOARD_ESP32S3BOX3=y" sdkconfig 2>/dev/null; then
    NEED_SWITCH=1
fi

if [ "$NEED_SWITCH" = "1" ]; then
    echo "Switching to ESP32-S3 / BOX-3 target. Cleaning build..."
    rm -rf build sdkconfig
    idf.py set-target esp32s3
fi

# Ensure board selection is ESP32-S3-BOX-3
if ! grep -q "CONFIG_HEYCLAWY_BOARD_ESP32S3BOX3=y" sdkconfig 2>/dev/null; then
    echo "Updating board selection to ESP32-S3-BOX-3..."
    sed -i '' \
        -e 's/CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER=y/# CONFIG_HEYCLAWY_BOARD_SENSECAP_WATCHER is not set/' \
        -e 's/CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO=y/# CONFIG_HEYCLAWY_BOARD_WAVESHARE_AUDIO is not set/' \
        -e 's/CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2=y/# CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2 is not set/' \
        -e 's/CONFIG_HEYCLAWY_BOARD_GENERIC_ESP32S3=y/# CONFIG_HEYCLAWY_BOARD_GENERIC_ESP32S3 is not set/' \
        -e 's/# CONFIG_HEYCLAWY_BOARD_ESP32S3BOX3 is not set/CONFIG_HEYCLAWY_BOARD_ESP32S3BOX3=y/' \
        sdkconfig
fi

# Fix flash size for BOX-3 (16MB)
sed -i '' \
    -e 's/CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y/CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y/' \
    -e 's/CONFIG_ESPTOOLPY_FLASHSIZE="8MB"/CONFIG_ESPTOOLPY_FLASHSIZE="16MB"/' \
    sdkconfig

echo "Board: ESP32-S3-BOX-3 (ESP32-S3, 16MB flash, 16MB PSRAM)"
echo ""

idf.py build

echo ""
echo "============================================"
echo " BUILD SUCCESSFUL"
echo " Flash with: idf.py -p /dev/ttyACM0 flash monitor"
echo "============================================"
