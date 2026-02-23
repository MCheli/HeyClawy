/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 *
 * TTS client — EdgeTTS (OpenAI-compatible) text-to-speech via HTTP
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *host;       // TTS server host (see secrets.h)
    uint16_t port;          // TTS server port (e.g., 5050)
    const char *api_key;    // API key (can be dummy for EdgeTTS)
    const char *voice;      // Voice name (e.g., "alloy", "echo", "nova")
    const char *model;      // Model name (e.g., "tts-1")
} tts_config_t;

// Callback for audio chunks during streaming playback
typedef void (*tts_audio_cb_t)(const int16_t *samples, size_t count);

// Initialize the TTS client
esp_err_t tts_init(const tts_config_t *config);

// Speak text: fetches audio from TTS server and plays through speaker.
// This is a blocking call — it streams and plays audio in chunks.
// Returns ESP_OK when playback is complete.
esp_err_t tts_speak(const char *text);

// Stop any current playback
void tts_stop(void);

// Check if TTS is currently playing
bool tts_is_playing(void);

#ifdef __cplusplus
}
#endif
