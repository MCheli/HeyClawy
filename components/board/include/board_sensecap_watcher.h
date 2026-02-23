/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 *
 * SenseCAP Watcher board pin definitions and hardware constants.
 *
 * Reference: https://github.com/Seeed-Studio/SenseCAP-Watcher-Firmware
 *            components/sensecap-watcher/include/sensecap-watcher.h
 *
 * Hardware Overview:
 *   MCU:        ESP32-S3 (dual-core, 240MHz) with 8MB Flash + Octal PSRAM
 *   Display:    SPD2010 412x412 16-bit color, QSPI interface
 *   Touch:      SPD2010 integrated touch, I2C interface
 *   Speaker:    ES8311 DAC codec via I2S
 *   Microphone: ES7243 / ES7243E ADC codec via I2S (shared bus with speaker)
 *   RGB LED:    WS2812 single LED
 *   Knob:       Rotary encoder + push button (button via IO expander)
 *   SD Card:    MicroSD via SPI2
 *   RTC:        PCF8563 via I2C0
 *   AI Camera:  Himax HX6538 via SPI2 + UART
 *   IO Expand:  PCA9535 16-bit I2C GPIO expander (power control, button, detect pins)
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "hal/adc_types.h"

// ============================================================================
// Board identification
// ============================================================================
#define BOARD_NAME "SenseCAP Watcher"
#define BOARD_MCU  "ESP32-S3"

// ============================================================================
// Board capability flags
// ============================================================================
#define BOARD_HAS_DISPLAY       1
#define BOARD_HAS_TOUCH         1
#define BOARD_HAS_KNOB          1
#define BOARD_HAS_CAMERA        1
#define BOARD_HAS_IO_EXPANDER   1
#define BOARD_HAS_RGB_RING      0   // Single LED, not a ring
#define BOARD_HAS_USER_BUTTONS  0   // Knob button only (via IO expander)

// ============================================================================
// SPI2 Bus (shared: SSCMA AI Camera + SD Card)
// ============================================================================
#define BOARD_SPI2_SCLK         GPIO_NUM_4
#define BOARD_SPI2_MOSI         GPIO_NUM_5
#define BOARD_SPI2_MISO         GPIO_NUM_6

// ============================================================================
// SPI3 / QSPI Bus (LCD display)
// ============================================================================
#define BOARD_QSPI_PCLK         GPIO_NUM_7
#define BOARD_QSPI_DATA0        GPIO_NUM_9
#define BOARD_QSPI_DATA1        GPIO_NUM_1
#define BOARD_QSPI_DATA2        GPIO_NUM_14
#define BOARD_QSPI_DATA3        GPIO_NUM_13

// ============================================================================
// LCD Display (SPD2010 via QSPI/SPI3)
// ============================================================================
#define BOARD_LCD_SPI_HOST      SPI3_HOST
#define BOARD_LCD_CS            GPIO_NUM_45
#define BOARD_LCD_RST           GPIO_NUM_NC     // No dedicated reset (shared)
#define BOARD_LCD_BL            GPIO_NUM_8      // Backlight (PWM via LEDC)
#define BOARD_LCD_H_RES         412
#define BOARD_LCD_V_RES         412
#define BOARD_LCD_BPP           16
#define BOARD_LCD_PIXEL_CLK_HZ  (40 * 1000 * 1000)
#define BOARD_LCD_CMD_BITS      32
#define BOARD_LCD_PARAM_BITS    8

// ============================================================================
// Touch Panel (SPD2010 integrated, I2C1)
// ============================================================================
#define BOARD_TOUCH_I2C_PORT    1
#define BOARD_TOUCH_SDA         GPIO_NUM_39
#define BOARD_TOUCH_SCL         GPIO_NUM_38
#define BOARD_TOUCH_I2C_FREQ    400000

// ============================================================================
// General I2C Bus (I2C0 - IO Expander, RTC, Audio Codecs)
// ============================================================================
#define BOARD_I2C_PORT          0
#define BOARD_I2C_SDA           GPIO_NUM_47
#define BOARD_I2C_SCL           GPIO_NUM_48
#define BOARD_I2C_FREQ          400000

// ============================================================================
// IO Expander (PCA9535 on I2C0)
// ============================================================================
#define BOARD_IO_EXP_INT        GPIO_NUM_2
#define BOARD_IO_EXP_ADDR       0x21    // PCA9535 address 001

// ============================================================================
// Audio I2S (shared bus: speaker ES8311 + mic ES7243/ES7243E)
// ============================================================================
#define BOARD_I2S_NUM           0
#define BOARD_I2S_MCLK          GPIO_NUM_10
#define BOARD_I2S_SCLK          GPIO_NUM_11
#define BOARD_I2S_LRCK          GPIO_NUM_12
#define BOARD_I2S_DSIN          GPIO_NUM_15     // Data IN  (from mic)
#define BOARD_I2S_DOUT          GPIO_NUM_16     // Data OUT (to speaker)
#define BOARD_AUDIO_SAMPLE_RATE 16000
#define BOARD_AUDIO_SAMPLE_BITS 16

// Audio codec I2C addresses (on I2C0)
#define BOARD_ES8311_ADDR       0x18    // Speaker DAC (7-bit) - 0x30 >> 1
#define BOARD_ES7243_ADDR       0x13    // Microphone ADC
#define BOARD_ES7243E_ADDR      0x14    // Microphone ADC (alternate)
#define BOARD_AUDIO_MIC_GAIN    27.0f

// ============================================================================
// RGB LED (WS2812, single LED)
// ============================================================================
#define BOARD_RGB_GPIO          GPIO_NUM_40
#define BOARD_RGB_LED_COUNT     1

// ============================================================================
// Rotary Encoder / Knob
// ============================================================================
#define BOARD_KNOB_A            GPIO_NUM_41
#define BOARD_KNOB_B            GPIO_NUM_42
// Knob button is on IO expander pin 3

// ============================================================================
// Battery ADC
// ============================================================================
#define BOARD_BAT_ADC_CHANNEL   ADC_CHANNEL_2   // GPIO3
#define BOARD_BAT_ADC_ATTEN     ADC_ATTEN_DB_12

// ============================================================================
// SD Card (SPI2, shared bus with SSCMA)
// ============================================================================
#define BOARD_SD_SPI_HOST       SPI2_HOST
#define BOARD_SD_CS             GPIO_NUM_46
// SD detect is on IO expander pin 4

// ============================================================================
// SSCMA AI Camera (Himax HX6538, SPI2 + IO expander)
// ============================================================================
#define BOARD_SSCMA_SPI_HOST    SPI2_HOST
#define BOARD_SSCMA_SPI_CS      GPIO_NUM_21
#define BOARD_SSCMA_SPI_CLK_HZ  (12 * 1000 * 1000)

// ============================================================================
// UART (Himax AI chip flasher)
// ============================================================================
#define BOARD_HIMAX_UART_NUM    UART_NUM_1
#define BOARD_HIMAX_UART_TX     GPIO_NUM_17
#define BOARD_HIMAX_UART_RX     GPIO_NUM_18
#define BOARD_HIMAX_UART_BAUD   921600

// ============================================================================
// RTC (PCF8563 on I2C0)
// ============================================================================
#define BOARD_RTC_I2C_ADDR      0x51

// ============================================================================
// IO Expander Pin Assignments (PCA9535)
//   P0.0-P0.7 = pins 0-7  (Port 0)
//   P1.0-P1.7 = pins 8-15 (Port 1)
// ============================================================================
// Inputs (accent: detect pins)
#define BOARD_IOEXP_CHRG_DET    0   // P0.0 - Charge detect
#define BOARD_IOEXP_STDBY_DET   1   // P0.1 - Standby detect
#define BOARD_IOEXP_VBUS_DET    2   // P0.2 - VBUS detect
#define BOARD_IOEXP_KNOB_BTN    3   // P0.3 - Knob button
#define BOARD_IOEXP_SD_DET      4   // P0.4 - SD card detect
#define BOARD_IOEXP_TOUCH_INT   5   // P0.5 - Touch interrupt
#define BOARD_IOEXP_SSCMA_SYNC  6   // P0.6 - SSCMA sync
#define BOARD_IOEXP_SSCMA_RST   7   // P0.7 - SSCMA reset

// Outputs (power control)
#define BOARD_IOEXP_PWR_SDCARD  8   // P1.0 - SD card power
#define BOARD_IOEXP_PWR_LCD     9   // P1.1 - LCD power
#define BOARD_IOEXP_PWR_SYSTEM  10  // P1.2 - System power
#define BOARD_IOEXP_PWR_AI      11  // P1.3 - AI chip power
#define BOARD_IOEXP_PWR_PA      12  // P1.4 - Codec PA power
#define BOARD_IOEXP_PWR_BAT_DET 13  // P1.5 - Battery detect
#define BOARD_IOEXP_PWR_GROVE   14  // P1.6 - Grove connector power
#define BOARD_IOEXP_PWR_BAT_ADC 15  // P1.7 - Battery ADC power
