/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 *
 * Generic ESP32-S3 board - placeholder for future custom hardware.
 * Minimal pin definitions for a basic ESP32-S3 + mic + speaker setup.
 */

#pragma once

#include "driver/gpio.h"
#include "hal/adc_types.h"

#define BOARD_NAME "Generic ESP32-S3"
#define BOARD_MCU  "ESP32-S3"

// TODO: Define pins for generic ESP32-S3 board when hardware is designed.
// This file serves as a template for adding new board targets.

#warning "Generic ESP32-S3 board is not yet implemented. Use SenseCAP Watcher."

#define BOARD_RGB_GPIO          GPIO_NUM_48  // Common default on devkits
#define BOARD_RGB_LED_COUNT     1

#define BOARD_I2S_NUM           0
#define BOARD_I2S_MCLK          GPIO_NUM_NC
#define BOARD_I2S_SCLK          GPIO_NUM_NC
#define BOARD_I2S_LRCK          GPIO_NUM_NC
#define BOARD_I2S_DSIN          GPIO_NUM_NC
#define BOARD_I2S_DOUT          GPIO_NUM_NC
#define BOARD_AUDIO_SAMPLE_RATE 16000
#define BOARD_AUDIO_SAMPLE_BITS 16
