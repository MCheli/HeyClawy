/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 *
 * M5StickC Plus2 pin definitions and hardware constants.
 *
 * Reference: https://docs.m5stack.com/en/core/M5StickC%20PLUS2
 *            Schematic: Sch_M5StickC_Plus2_v0.5.pdf
 *
 * Hardware Overview:
 *   MCU:        ESP32-PICO-V3-02 (dual-core 240MHz, 8MB Flash, 2MB PSRAM)
 *   Display:    ST7789V2 1.14" TFT 135×240, SPI interface
 *   Speaker:    Passive buzzer on GPIO2 (PWM-driven)
 *   Microphone: SPM1423 MEMS PDM mic (CLK=GPIO0, DATA=GPIO34)
 *   Buttons:    3 buttons (A=GPIO37, B=GPIO39, C/Power=GPIO35)
 *   LED:        Red LED on GPIO19 (shared with IR emitter)
 *   IMU:        MPU6886 (I2C, optional)
 *   RTC:        BM8563 (I2C)
 *   Power:      GPIO4 HOLD pin (must be HIGH to keep power on)
 *   No IO expander, no knob, no camera, no RGB strip.
 */

#pragma once

#include "driver/gpio.h"
#include "hal/adc_types.h"

// ============================================================================
// Board identification
// ============================================================================
#define BOARD_NAME "M5StickC Plus2"
#define BOARD_MCU  "ESP32"

// ============================================================================
// Board capability flags
// ============================================================================
#define BOARD_HAS_DISPLAY       1
#define BOARD_HAS_TOUCH         0
#define BOARD_HAS_KNOB          0
#define BOARD_HAS_CAMERA        0
#define BOARD_HAS_IO_EXPANDER   0
#define BOARD_HAS_RGB_RING      0   // Single red LED, not WS2812
#define BOARD_HAS_USER_BUTTONS  1   // 3 GPIO buttons
#define BOARD_HAS_BUZZER        1   // Passive buzzer on GPIO2
#define BOARD_HAS_SIMPLE_LED    1   // GPIO-driven single LED (not addressable)

// ============================================================================
// Power management
// ============================================================================
#define BOARD_HOLD_PIN          GPIO_NUM_4   // Must be HIGH to keep device powered
#define BOARD_POWER_BTN         GPIO_NUM_35  // Button C = power/wake

// ============================================================================
// Display (ST7789V2 via SPI)
// ============================================================================
#define BOARD_LCD_SPI_HOST      SPI2_HOST
#define BOARD_LCD_MOSI          GPIO_NUM_15
#define BOARD_LCD_SCLK          GPIO_NUM_13
#define BOARD_LCD_DC            GPIO_NUM_14
#define BOARD_LCD_RST           GPIO_NUM_12
#define BOARD_LCD_CS            GPIO_NUM_5
#define BOARD_LCD_BL            GPIO_NUM_27  // Backlight (LEDC PWM)
#define BOARD_LCD_H_RES         135
#define BOARD_LCD_V_RES         240
#define BOARD_LCD_BPP           16
#define BOARD_LCD_PIXEL_CLK_HZ  (40 * 1000 * 1000)
#define BOARD_LCD_CMD_BITS      8
#define BOARD_LCD_PARAM_BITS    8
#define BOARD_LCD_ROTATION      1    // Landscape mode (240×135)

// ============================================================================
// Audio — Passive Buzzer (PWM output)
// ============================================================================
#define BOARD_BUZZER_GPIO       GPIO_NUM_2
#define BOARD_BUZZER_LEDC_TIMER LEDC_TIMER_1
#define BOARD_BUZZER_LEDC_CH    LEDC_CHANNEL_1
#define BOARD_AUDIO_SAMPLE_RATE 16000
#define BOARD_AUDIO_SAMPLE_BITS 16

// ============================================================================
// Audio — PDM Microphone (SPM1423)
// ============================================================================
#define BOARD_I2S_NUM           0
#define BOARD_PDM_CLK           GPIO_NUM_0   // PDM clock
#define BOARD_PDM_DATA          GPIO_NUM_34  // PDM data in

// Dummy I2S pins (not used on M5Stick but referenced by common code)
#define BOARD_I2S_MCLK          GPIO_NUM_NC
#define BOARD_I2S_SCLK          GPIO_NUM_NC
#define BOARD_I2S_LRCK          GPIO_NUM_NC
#define BOARD_I2S_DSIN          GPIO_NUM_NC
#define BOARD_I2S_DOUT          GPIO_NUM_NC

// ============================================================================
// I2C Bus (IMU, RTC)
// ============================================================================
#define BOARD_I2C_PORT          0
#define BOARD_I2C_SDA           GPIO_NUM_21
#define BOARD_I2C_SCL           GPIO_NUM_22
#define BOARD_I2C_FREQ          400000

// ============================================================================
// Buttons (active low, directly on GPIO)
// ============================================================================
#define BOARD_BTN_A             GPIO_NUM_37  // Front button
#define BOARD_BTN_B             GPIO_NUM_39  // Side button
#define BOARD_BTN_C             GPIO_NUM_35  // Power/wake button
#define BOARD_BOOT_BUTTON       GPIO_NUM_0   // Also PDM CLK!

// ============================================================================
// LED (Red, shared with IR emitter)
// ============================================================================
#define BOARD_LED_GPIO          GPIO_NUM_19
#define BOARD_RGB_GPIO          GPIO_NUM_19  // Alias for common code
#define BOARD_RGB_LED_COUNT     1

// ============================================================================
// Battery ADC
// ============================================================================
#define BOARD_BAT_ADC_CHANNEL   ADC_CHANNEL_2   // GPIO38
#define BOARD_BAT_ADC_ATTEN     ADC_ATTEN_DB_12

// ============================================================================
// RTC (BM8563 on I2C)
// ============================================================================
#define BOARD_RTC_I2C_ADDR      0x51

// ============================================================================
// Expansion Port (HY2.0-4P)
// ============================================================================
#define BOARD_EXT_GPIO1         GPIO_NUM_32
#define BOARD_EXT_GPIO2         GPIO_NUM_33
