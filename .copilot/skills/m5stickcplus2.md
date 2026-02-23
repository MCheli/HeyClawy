# M5StickCPlus2 Board Support

## Hardware Overview
- **MCU**: ESP32-PICO-V3-02 (regular ESP32, NOT ESP32-S3)
- **Flash**: 8MB, **PSRAM**: 2MB (much less than S3 boards with 8MB)
- **Display**: ST7789V2 1.14" TFT, 135×240 (landscape: 240×135)
- **Audio output**: Passive buzzer on GPIO2 (PWM-driven only, crude TTS via duty cycle modulation)
- **Audio input**: SPM1423 PDM MEMS mic (CLK=GPIO0, DATA=GPIO34)
- **Buttons**: A=GPIO37 (front), B=GPIO39 (side), C=GPIO35 (power/wake)
- **LED**: Red LED on GPIO19 (shared with IR, not addressable RGB)
- **Power**: GPIO4 HOLD pin (must be HIGH to keep device powered)
- **USB**: CH9102 on COM17

## Key Differences from ESP32-S3 Boards
1. **Target**: `idf.py set-target esp32` (not `esp32s3`)
2. **No wake word** — ESP-SR requires ESP32-S3
3. **No I2S codec** — buzzer only (PWM DAC for audio, not ES8311)
4. **PDM microphone** — I2S PDM RX mode (not standard I2S)
5. **Smaller PSRAM** — 2MB vs 8MB, limits concurrent operations
6. **IRAM constrained** — requires size optimization (`-Os`, WiFi IRAM opt off)

## Build System
- Build script: `build_m5stick.bat`
- ESP32-specific defaults: `sdkconfig.defaults.esp32`
- Conditional deps: `esp_lcd_spd2010`, `esp_io_expander`, `esp_codec_dev`, `esp-sr` are excluded via `idf_component.yml` rules and CMakeLists.txt conditionals
- Board header: `components/board/include/board_m5stickcplus2.h`

## Pin Map
| Function | GPIO | Notes |
|----------|------|-------|
| LCD MOSI | 15 | SPI |
| LCD SCLK | 13 | SPI |
| LCD DC | 14 | |
| LCD RST | 12 | |
| LCD CS | 5 | |
| LCD BL | 27 | LEDC PWM |
| Buzzer | 2 | LEDC PWM |
| PDM CLK | 0 | Shared with BOOT |
| PDM DATA | 34 | Input only |
| Button A | 37 | Active LOW |
| Button B | 39 | Active LOW |
| Button C | 35 | Active LOW, wake from deep sleep |
| LED | 19 | Red, shared with IR |
| HOLD | 4 | Must be HIGH |
| I2C SDA | 21 | IMU/RTC |
| I2C SCL | 22 | IMU/RTC |

## Capability Flags
```c
BOARD_HAS_DISPLAY       1   // Small 135×240 ST7789V2
BOARD_HAS_TOUCH         0
BOARD_HAS_KNOB          0
BOARD_HAS_CAMERA        0
BOARD_HAS_IO_EXPANDER   0
BOARD_HAS_RGB_RING      0
BOARD_HAS_USER_BUTTONS  1   // 3 GPIO buttons
BOARD_HAS_BUZZER        1   // Passive buzzer
BOARD_HAS_SIMPLE_LED    1   // Red LED (not WS2812)
```

## Audio Architecture
- **Recording**: I2S0 PDM RX mode via `i2s_pdm_rx` driver. 16kHz mono. DC offset ~-1890 (calibrated).
- **Playback**: I2S1 standard mode TX with 1-bit delta-sigma modulation on GPIO2.
  - Speaker: PAM8302A class-D amplifier + tiny passive buzzer
  - I2S at 48kHz stereo 16-bit → 1.536 Mbps bitstream → 96× oversampling of 16kHz audio
  - Each PCM sample → 3 × 32 = 96 bits of delta-sigma output
  - 2× gain amplification with clipping
  - DMA-driven (no CPU busy-wait during playback!)
  - ~8-bit effective resolution with noise shaping
  - Based on M5Unified Speaker_Class buzzer mode architecture
  - Approaches tried that **failed**: I2S PDM TX (no sound), Sigma-Delta Modulation driver (no sound), LEDC PWM (worked but CPU-intensive, audible carrier artifacts)
- **Tick sound**: 1kHz sine wave via board_audio_play() (30ms = 480 samples)
- **No hardware volume control**: Gain applied in software (2× default)

## ⚠️ Audio Quality Notes
- The passive buzzer on GPIO2 is fundamentally limited for speech playback
- Delta-sigma I2S approach is the best achievable method (M5Unified uses same technique)
- Quality is acceptable for short TTS responses but will never match codec-based audio
- ESP32's internal DAC (GPIO25/26) would be better but requires hardware modification
- For high-quality audio, use the SPK2 Hat (external I2S DAC/amp)

## Power Management
- **Power on**: Set GPIO4 HIGH immediately on boot
- **Power off**: Set GPIO4 LOW (instant shutdown if not USB-powered)
- **Deep sleep**: Configure GPIO35 (Button C) as EXT0 wake source
- **No PMIC**: Unlike original M5StickC (AXP192), Plus2 uses simple GPIO hold

## Display
- Uses ESP-IDF built-in `esp_lcd_panel_st7789` (no managed component)
- SPI bus on SPI2_HOST at 40MHz
- Color inversion enabled (ST7789 quirk)
- Landscape mode: `swap_xy=true, mirror_x=true`
- LVGL configured for 240×135 with 10-row buffer

## Limitations
- Audio quality is limited by passive buzzer hardware — TTS is intelligible but low-fi
- No wake word detection (ESP-SR S3 only)
- 2MB PSRAM limits concurrent allocations
- IRAM overflow at default optimization — must use `-Os`
- No camera, no knob, no IO expander
- Speaker and mic use separate I2S ports (I2S0=mic RX, I2S1=speaker TX)
