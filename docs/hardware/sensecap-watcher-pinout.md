# SenseCAP Watcher Hardware Pinout Reference

**Source**: [SenseCAP-Watcher-Firmware](https://github.com/Seeed-Studio/SenseCAP-Watcher-Firmware/blob/main/components/sensecap-watcher/include/sensecap-watcher.h)

## MCU
- ESP32-S3 (dual-core Xtensa LX7, 240MHz)
- 8MB Flash (QIO, 80MHz)
- Octal PSRAM

## Pin Map

### SPI2 Bus (shared: AI Camera + SD Card)
| Signal | GPIO |
|--------|------|
| SCLK   | 4    |
| MOSI   | 5    |
| MISO   | 6    |

### QSPI / SPI3 Bus (LCD Display)
| Signal | GPIO |
|--------|------|
| PCLK   | 7    |
| DATA0  | 9    |
| DATA1  | 1    |
| DATA2  | 14   |
| DATA3  | 13   |

### LCD Display (SPD2010)
| Signal     | GPIO/Value       |
|------------|------------------|
| SPI Host   | SPI3_HOST        |
| CS         | 45               |
| RST        | NC (shared)      |
| Backlight  | 8 (PWM/LEDC)     |
| Resolution | 412 x 412        |
| Color      | 16-bit RGB565    |
| Pixel CLK  | 40 MHz           |

### Touch Panel (SPD2010 integrated)
| Signal   | GPIO/Value |
|----------|------------|
| I2C Port | I2C1       |
| SDA      | 39         |
| SCL      | 38         |
| INT      | IO Exp P0.5|
| Freq     | 400 kHz    |

### General I2C Bus (I2C0)
| Signal   | GPIO |
|----------|------|
| SDA      | 47   |
| SCL      | 48   |
| Freq     | 400 kHz |

Devices on I2C0:
- PCA9535 IO Expander (addr 0x21)
- PCF8563 RTC (addr 0x51)
- ES8311 Speaker DAC (addr 0x18)
- ES7243 Mic ADC (addr 0x13) or ES7243E (addr 0x14)

### Audio I2S
| Signal | GPIO |
|--------|------|
| MCLK   | 10   |
| SCLK   | 11   |
| LRCK   | 12   |
| DIN    | 15   |
| DOUT   | 16   |

- Sample Rate: 16 kHz
- Bit Width: 16-bit
- Speaker Codec: ES8311 (DAC)
- Mic Codec: ES7243 or ES7243E (ADC)
- Mic Gain: 27.0 dB

### RGB LED (WS2812)
| Signal | GPIO |
|--------|------|
| DATA   | 40   |
| Count  | 1    |

### Rotary Encoder (Knob)
| Signal | GPIO/IO Exp |
|--------|-------------|
| A      | 41          |
| B      | 42          |
| Button | IO Exp P0.3 |

### Battery ADC
| Signal  | Value         |
|---------|---------------|
| Channel | ADC_CHANNEL_2 |
| GPIO    | 3             |
| Atten   | 2.5 dB (0-1100mV) |
| Ratio   | (62+20)/20    |

### SD Card (MicroSD via SPI2)
| Signal | GPIO/IO Exp |
|--------|-------------|
| SPI    | SPI2_HOST   |
| CS     | 46          |
| Detect | IO Exp P0.4 |

### Himax AI Camera (HX6538)
| Signal      | GPIO/IO Exp |
|-------------|-------------|
| SPI Bus     | SPI2_HOST   |
| SPI CS      | 21          |
| SPI CLK     | 12 MHz      |
| SYNC        | IO Exp P0.6 |
| RST         | IO Exp P0.7 |
| UART TX     | 17          |
| UART RX     | 18          |
| UART Baud   | 921600      |

### IO Expander (PCA9535, I2C addr 0x21)
| Pin  | Name        | Dir    | Function              |
|------|-------------|--------|-----------------------|
| P0.0 | CHRG_DET    | Input  | Charge detect         |
| P0.1 | STDBY_DET   | Input  | Standby detect        |
| P0.2 | VBUS_DET    | Input  | VBUS detect           |
| P0.3 | KNOB_BTN    | Input  | Knob push button      |
| P0.4 | SD_DET      | Input  | SD card detect        |
| P0.5 | TOUCH_INT   | Input  | Touch interrupt       |
| P0.6 | SSCMA_SYNC  | Input  | AI camera sync        |
| P0.7 | SSCMA_RST   | Output | AI camera reset       |
| P1.0 | PWR_SDCARD  | Output | SD card power         |
| P1.1 | PWR_LCD     | Output | LCD power             |
| P1.2 | PWR_SYSTEM  | Output | System power latch    |
| P1.3 | PWR_AI      | Output | AI chip power         |
| P1.4 | PWR_PA      | Output | Codec PA power        |
| P1.5 | PWR_BAT_DET | Output | Battery detect enable |
| P1.6 | PWR_GROVE   | Output | Grove connector power |
| P1.7 | PWR_BAT_ADC | Output | Battery ADC enable    |

INT pin: GPIO 2

### RTC (PCF8563)
- I2C address: 0x51
- On I2C0 bus

## Power Startup Sequence
1. Set IO expander outputs all low
2. Set PWR_SYSTEM high
3. Wait 100ms
4. Set PWR_SDCARD, PWR_LCD, PWR_AI, PWR_PA, PWR_GROVE, PWR_BAT_ADC high
5. Wait 50ms
