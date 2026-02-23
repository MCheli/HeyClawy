# Skill: SenseCAP Watcher Hardware

## Overview
The SenseCAP Watcher is an ESP32-S3 based device by Seeed Studio with AI capabilities. It includes:
- ESP32-S3 MCU (dual-core 240MHz, 8MB Flash, Octal PSRAM)
- Himax HX6538 AI co-processor (Cortex-M55 + Ethos-U55)
- SPD2010 round display (412x412, 16-bit, QSPI + capacitive touch)
- ES8311 speaker DAC + ES7243/ES7243E microphone ADC
- WS2812 RGB LED (single)
- Rotary encoder with push button
- MicroSD card slot
- PCF8563 RTC
- PCA9535 16-bit IO expander (power management, buttons, detect pins)

## Key Gotchas

### IO Expander is Critical
Most power control is via the PCA9535 IO expander on I2C0 (addr 0x21). Without initializing it and setting the correct power pins, the LCD, SD card, audio codec, and AI chip won't power on.

**Power-on sequence:**
1. Init IO expander
2. Set all outputs low
3. Set PWR_SYSTEM (P1.2) high, wait 100ms
4. Set PWR_SDCARD(P1.0), PWR_LCD(P1.1), PWR_AI(P1.3), PWR_PA(P1.4), PWR_GROVE(P1.6), PWR_BAT_ADC(P1.7) high
5. Wait 50ms

### SPI Bus Sharing
SPI2 is shared between the AI camera (SSCMA client) and SD card. You MUST pull the unused CS pin HIGH before using the other device.
- AI Camera CS: GPIO21
- SD Card CS: GPIO46

### Display Driver
The SPD2010 is a specialized display that requires:
- QSPI mode (4 data lines)
- Custom driver: `esp_lcd_spd2010` (available as ESP-IDF managed component)
- Touch is also SPD2010-based, using I2C1 (SDA=GPIO39, SCL=GPIO38)

### Audio Codec Variants
Some units have ES7243 (addr 0x13), others have ES7243E (addr 0x14). The firmware probes both addresses at runtime.

### Knob Button
The push button of the rotary encoder is NOT a direct GPIO — it's on IO expander pin P0.3. You need the IO expander driver to read it.

## Pinout Reference
See `docs/hardware/sensecap-watcher-pinout.md` for the complete pin mapping.

## Firmware Reference
Official firmware source: https://github.com/Seeed-Studio/SenseCAP-Watcher-Firmware
Key files:
- `components/sensecap-watcher/include/sensecap-watcher.h` — All pin definitions
- `components/sensecap-watcher/sensecap-watcher.c` — BSP initialization code
- `components/esp_lcd_touch_chsc6x/` — Touch driver
- `components/esp_lvgl_port/` — LVGL display port
- `components/knob/` — Rotary encoder driver
