/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 *
 * App state helpers — LED mapping, state transitions, shared globals
 */

#include "app_state.h"
#include "board.h"
#include "ui.h"
#include "openclaw_client.h"
#include "settings.h"
#include "webserver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "app_state";

/* Globals */
EventGroupHandle_t g_app_events = NULL;
bool g_recording = false;
int64_t g_response_shown_at = 0;
uint8_t *g_pending_jpeg = NULL;
size_t   g_pending_jpeg_size = 0;
bool g_continue_listening = false;
volatile bool g_tts_pending = false;

/* ── LED helper ──────────────────────────────────────────────────────── */

/* Map startup_pattern setting (0-4) to RGB mode enum */
static board_rgb_mode_t startup_mode(void)
{
    uint8_t pat = settings_get()->startup_pattern;
    switch (pat) {
    case 1:  return RGB_MODE_AURORA;
    case 2:  return RGB_MODE_STARFIELD;
    case 3:  return RGB_MODE_FIRE;
    case 4:  return RGB_MODE_OCEAN;
    default: return RGB_MODE_RAINBOW_SPIN;
    }
}

void app_led_for_state(ui_state_t st)
{
#if BOARD_RGB_LED_COUNT > 1
    /* Multi-LED ring: rich animations */
    switch (st) {
    case UI_STATE_BOOT:
    case UI_STATE_CONNECTING:
        board_rgb_animate(startup_mode(), 0, 0, 0);
        break;
    case UI_STATE_IDLE:
        board_rgb_animate(RGB_MODE_SOLID, 0, 4, 0);
        break;
    case UI_STATE_LISTENING:
        board_rgb_animate(RGB_MODE_PULSE_WAVE, 8, 40, 16);
        break;
    case UI_STATE_SENDING:
        board_rgb_animate(RGB_MODE_CHASE, 0, 16, 40);
        break;
    case UI_STATE_THINKING:
        board_rgb_animate(RGB_MODE_CHASE, 40, 24, 0);
        break;
    case UI_STATE_STREAMING:
        board_rgb_animate(RGB_MODE_SPARKLE, 32, 8, 48);
        break;
    case UI_STATE_RESPONSE:
        board_rgb_animate(RGB_MODE_SOLID, 0, 8, 0);
        break;
    case UI_STATE_TTS_LOADING:
        board_rgb_animate(RGB_MODE_CHASE, 0, 24, 24);
        break;
    case UI_STATE_TTS_PLAYING:
        board_rgb_animate(RGB_MODE_BREATHE, 0, 32, 16);
        break;
    case UI_STATE_ERROR:
        board_rgb_animate(RGB_MODE_BLINK, 32, 0, 0);
        break;
    default:
        board_rgb_animate(RGB_MODE_SOLID, 16, 16, 0);
        break;
    }
#else
    /* Single LED: simple modes */
    switch (st) {
    case UI_STATE_IDLE:
        board_rgb_animate(RGB_MODE_SOLID, 0, 4, 0);
        break;
    case UI_STATE_LISTENING:
        board_rgb_animate(RGB_MODE_SOLID, 32, 0, 0);
        break;
    case UI_STATE_SENDING:
        board_rgb_animate(RGB_MODE_SOLID, 0, 0, 32);
        break;
    case UI_STATE_THINKING:
        board_rgb_animate(RGB_MODE_BREATHE, 40, 24, 0);
        break;
    case UI_STATE_STREAMING:
        board_rgb_animate(RGB_MODE_BREATHE, 24, 0, 40);
        break;
    case UI_STATE_RESPONSE:
        board_rgb_animate(RGB_MODE_SOLID, 0, 6, 0);
        break;
    case UI_STATE_TTS_LOADING:
        board_rgb_animate(RGB_MODE_BLINK, 0, 24, 24);
        break;
    case UI_STATE_TTS_PLAYING:
        board_rgb_animate(RGB_MODE_BREATHE, 0, 32, 16);
        break;
    case UI_STATE_ERROR:
        board_rgb_animate(RGB_MODE_BLINK, 32, 0, 0);
        break;
    default:
        board_rgb_animate(RGB_MODE_SOLID, 16, 16, 0);
        break;
    }
#endif
}

void app_set_state(ui_state_t st)
{
    ui_set_state(st);
    app_led_for_state(st);
}

/* ── Device command parser ───────────────────────────────────────────── */
/* Parses and executes [DEVICE:key=value] commands from OpenClaw response.
 * Strips processed command lines from buf in-place.
 * Returns number of commands executed. */
static int parse_device_commands(char *buf)
{
    int count = 0;
    char *pos = buf;

    while ((pos = strstr(pos, "[DEVICE:")) != NULL) {
        char *start = pos;
        char *end = strchr(pos, ']');
        if (!end) break;

        /* Extract key=value from [DEVICE:key=value] */
        const char *kv = pos + 8;  /* skip "[DEVICE:" */
        size_t kv_len = end - kv;
        char cmd[64];
        if (kv_len >= sizeof(cmd)) { pos = end + 1; continue; }
        memcpy(cmd, kv, kv_len);
        cmd[kv_len] = '\0';

        /* Split key=value */
        char *eq = strchr(cmd, '=');
        const char *key = cmd;
        const char *val = "";
        if (eq) {
            *eq = '\0';
            val = eq + 1;
        }

        ESP_LOGI(TAG, "Device cmd: %s=%s", key, val);
        settings_t *cfg = settings_get_mutable();

        if (strcmp(key, "volume") == 0) {
            int v = atoi(val);
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            cfg->volume = (uint8_t)v;
            board_audio_set_volume(v);
            settings_save();
            ESP_LOGI(TAG, "Volume set to %d%%", v);
            count++;
        } else if (strcmp(key, "brightness") == 0) {
            int v = atoi(val);
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            cfg->brightness = (uint8_t)v;
            board_display_set_brightness(v);
            settings_save();
            ESP_LOGI(TAG, "Brightness set to %d%%", v);
            count++;
        } else if (strcmp(key, "rgb") == 0) {
            if (strcmp(val, "off") == 0) {
                cfg->rgb_enabled = false;
                board_rgb_set(0, 0, 0);
                settings_save();
            } else if (strcmp(val, "on") == 0) {
                cfg->rgb_enabled = true;
                settings_save();
                app_led_for_state(ui_get_state());
            } else if (strcmp(val, "rainbow") == 0) {
                board_rgb_animate(RGB_MODE_RAINBOW_SPIN, 0, 0, 0);
            } else if (strcmp(val, "aurora") == 0) {
                board_rgb_animate(RGB_MODE_AURORA, 0, 0, 0);
            } else if (strcmp(val, "starfield") == 0) {
                board_rgb_animate(RGB_MODE_STARFIELD, 0, 0, 0);
            } else if (strcmp(val, "fire") == 0) {
                board_rgb_animate(RGB_MODE_FIRE, 0, 0, 0);
            } else if (strcmp(val, "ocean") == 0) {
                board_rgb_animate(RGB_MODE_OCEAN, 0, 0, 0);
            } else {
                /* Try as R,G,B values e.g. "255,0,128" */
                int r = 0, g = 0, b = 0;
                if (sscanf(val, "%d,%d,%d", &r, &g, &b) == 3) {
                    board_rgb_set((uint8_t)r, (uint8_t)g, (uint8_t)b);
                }
            }
            ESP_LOGI(TAG, "RGB: %s", val);
            count++;
        } else if (strcmp(key, "sleep") == 0) {
            int mins = atoi(val);
            cfg->sleep_timeout_ms = (uint32_t)(mins * 60000);
            settings_save();
            ESP_LOGI(TAG, "Sleep timeout: %d min", mins);
            count++;
        } else if (strcmp(key, "reboot") == 0) {
            ESP_LOGW(TAG, "Reboot requested via voice");
            count++;
            /* Strip command, then reboot after short delay */
            memmove(start, end + 1, strlen(end + 1) + 1);
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        } else if (strcmp(key, "webserver") == 0) {
            if (strcmp(val, "on") == 0 && !webserver_is_running()) {
                xEventGroupSetBits(g_app_events, WEBSERVER_TOGGLE_BIT);
            } else if (strcmp(val, "off") == 0 && webserver_is_running()) {
                xEventGroupSetBits(g_app_events, WEBSERVER_TOGGLE_BIT);
            }
            count++;
        } else if (strcmp(key, "auto_read") == 0) {
            cfg->auto_read_response = (strcmp(val, "on") == 0 || strcmp(val, "true") == 0);
            settings_save();
            count++;
        } else {
            ESP_LOGW(TAG, "Unknown device cmd: %s", key);
        }

        /* Strip the [DEVICE:...] tag from buffer (including trailing newline) */
        char *after = end + 1;
        if (*after == '\n') after++;
        memmove(start, after, strlen(after) + 1);
        pos = start;  /* Continue scanning from same position */
    }

    return count;
}
void app_on_chat_response(const char *text, bool is_final)
{
    if (is_final) {
        ESP_LOGI(TAG, "Response (%lu ms): %.100s%s",
                 (unsigned long)openclaw_get_thinking_time_ms(),
                 text, strlen(text) > 100 ? "..." : "");

        /* Check for [LISTEN] / [END] tag at end of response */
        g_continue_listening = false;
        const char *listen_tag = strstr(text, "[LISTEN]");
        if (listen_tag) {
            g_continue_listening = true;
            ESP_LOGI(TAG, "Conversational: will auto-listen after TTS");
        }

        /* Work on a mutable copy so we can strip the tag */
        static char resp_buf[2048];
        strncpy(resp_buf, text, sizeof(resp_buf) - 1);
        resp_buf[sizeof(resp_buf) - 1] = '\0';

        /* Strip [LISTEN] or [END] tag from response text */
        char *tag = strstr(resp_buf, "[LISTEN]");
        if (tag) {
            /* Trim trailing whitespace/newlines before tag */
            char *p = tag;
            while (p > resp_buf && (*(p-1) == '\n' || *(p-1) == '\r' || *(p-1) == ' ')) p--;
            *p = '\0';
        }
        tag = strstr(resp_buf, "[END]");
        if (tag) {
            char *p = tag;
            while (p > resp_buf && (*(p-1) == '\n' || *(p-1) == '\r' || *(p-1) == ' ')) p--;
            *p = '\0';
        }

        /* Parse and execute device commands ([DEVICE:key=value]) */
        int dev_cmds = parse_device_commands(resp_buf);
        if (dev_cmds > 0) {
            ESP_LOGI(TAG, "Executed %d device command(s)", dev_cmds);
        }

        /* Parse dual response: first line = short label, rest = spoken response */
        static char short_buf[128];
        const char *long_text = resp_buf;
        const char *nl = strchr(resp_buf, '\n');
        if (nl && (nl - resp_buf) < (int)sizeof(short_buf) - 1) {
            size_t short_len = nl - resp_buf;
            memcpy(short_buf, resp_buf, short_len);
            short_buf[short_len] = '\0';
            long_text = nl + 1;
            while (*long_text == '\n' || *long_text == '\r') long_text++;
            if (*long_text == '\0') long_text = short_buf;
            ESP_LOGI(TAG, "Short: '%s' | Long: '%.80s%s'", short_buf, long_text,
                     strlen(long_text) > 80 ? "..." : "");
        } else {
            strncpy(short_buf, resp_buf, sizeof(short_buf) - 1);
            short_buf[sizeof(short_buf) - 1] = '\0';
        }

        app_set_state(UI_STATE_RESPONSE);
        g_response_shown_at = esp_timer_get_time();
        ui_set_response(short_buf, long_text);

        /* Auto-TTS: always on screenless boards, otherwise respect setting */
#if BOARD_HAS_DISPLAY
        const settings_t *cfg = settings_get();
        if (cfg->auto_read_response) {
            g_tts_pending = true;
            xEventGroupSetBits(g_app_events, TTS_PLAY_BIT);
        }
#else
        /* No display — always speak the response */
        g_tts_pending = true;
        xEventGroupSetBits(g_app_events, TTS_PLAY_BIT);
#endif
    }
}
