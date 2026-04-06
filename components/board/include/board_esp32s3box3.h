/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 *
 * ESP32-S3-BOX-3 pin definitions and hardware constants.
 *
 * Reference: https://github.com/espressif/esp-box
 *            Schematic: ESP32-S3-BOX-3_V1.2_SCH.pdf
 *
 * Hardware Overview:
 *   MCU:        ESP32-S3 (dual-core, 240MHz) with 16MB Flash + 16MB Octal PSRAM
 *   Display:    ILI9341 320x240 16-bit color, SPI interface
 *   Touch:      GT911 capacitive (I2C 0x5D, INT=GPIO3)
 *   Speaker:    ES8311 DAC codec via I2S
 *   Microphone: ES7210 ADC codec (dual mic array) via I2S (shared bus)
 *   RGB LED:    None (status shown on display)
 *   Buttons:    MUTE (GPIO1), BOOT (GPIO0)
 *   No IO expander, no knob, no camera, no SD card.
 */

#pragma once

#include "driver/gpio.h"
#include "hal/adc_types.h"

// ============================================================================
// Board identification
// ============================================================================
#define BOARD_NAME "ESP32-S3-BOX-3"
#define BOARD_MCU  "ESP32-S3"

// ============================================================================
// Board capability flags
// ============================================================================
#define BOARD_HAS_DISPLAY       1
#define BOARD_HAS_TOUCH         1
#define BOARD_HAS_KNOB          0
#define BOARD_HAS_CAMERA        0
#define BOARD_HAS_IO_EXPANDER   0
#define BOARD_HAS_RGB_RING      0
#define BOARD_HAS_USER_BUTTONS  1   // MUTE + BOOT buttons

// ============================================================================
// General I2C Bus (I2C0 - Audio Codecs, Touch)
// ============================================================================
#define BOARD_I2C_PORT          0
#define BOARD_I2C_SDA           GPIO_NUM_8
#define BOARD_I2C_SCL           GPIO_NUM_18
#define BOARD_I2C_FREQ          400000

// ============================================================================
// Audio I2S (shared bus: speaker ES8311 + mic ES7210)
// ============================================================================
#define BOARD_I2S_NUM           0
#define BOARD_I2S_MCLK          GPIO_NUM_2
#define BOARD_I2S_SCLK          GPIO_NUM_17
#define BOARD_I2S_LRCK          GPIO_NUM_45
#define BOARD_I2S_DSIN          GPIO_NUM_16     // Data IN  (from mic ES7210)
#define BOARD_I2S_DOUT          GPIO_NUM_15     // Data OUT (to speaker ES8311)
#define BOARD_AUDIO_SAMPLE_RATE 16000
#define BOARD_AUDIO_SAMPLE_BITS 16

// Audio codec I2C addresses (on I2C0)
#define BOARD_ES8311_ADDR       0x18    // Speaker DAC (7-bit)
#define BOARD_ES7210_ADDR       0x40    // Microphone ADC (7-bit)
#define BOARD_AUDIO_MIC_GAIN    36.0f

// Speaker PA enable (active high)
#define BOARD_PA_GPIO           GPIO_NUM_46

// ============================================================================
// LCD Display (ILI9341 via SPI)
// ============================================================================
#define BOARD_LCD_SPI_HOST      SPI3_HOST
#define BOARD_LCD_SCLK          GPIO_NUM_7
#define BOARD_LCD_MOSI          GPIO_NUM_6
#define BOARD_LCD_CS            GPIO_NUM_5
#define BOARD_LCD_DC            GPIO_NUM_4
#define BOARD_LCD_RST           GPIO_NUM_48
#define BOARD_LCD_BL            GPIO_NUM_47
#define BOARD_LCD_H_RES         320
#define BOARD_LCD_V_RES         240
#define BOARD_LCD_BPP           16
#define BOARD_LCD_PIXEL_CLK_HZ  (40 * 1000 * 1000)

// ============================================================================
// Touch Panel (GT911 on I2C0)
// ============================================================================
#define BOARD_TOUCH_ADDR        0x5D    // GT911 default I2C address
#define BOARD_TOUCH_INT         GPIO_NUM_3
#define BOARD_TOUCH_RST         GPIO_NUM_NC     // No dedicated reset

// ============================================================================
// Buttons
// ============================================================================
#define BOARD_BOOT_BUTTON       GPIO_NUM_0      // BOOT button (active low)
#define BOARD_MUTE_BUTTON       GPIO_NUM_1      // MUTE button (active low)

// ============================================================================
// Battery ADC (18650 battery dock, voltage divider on GPIO10)
// ============================================================================
#define BOARD_BAT_ADC_GPIO      GPIO_NUM_10
#define BOARD_BAT_ADC_CHANNEL   ADC_CHANNEL_9   // GPIO10 = ADC1_CH9
#define BOARD_BAT_ADC_ATTEN     ADC_ATTEN_DB_12
#define BOARD_BAT_VDIV_MULTIPLY 4.01f            // Voltage divider ratio

// ============================================================================
// RGB LED — BOX-3 has no addressable RGB LED
// Use a dummy GPIO (never initialized) so RGB stubs compile.
// ============================================================================
#define BOARD_RGB_GPIO          GPIO_NUM_NC
#define BOARD_RGB_LED_COUNT     0
