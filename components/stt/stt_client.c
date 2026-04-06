/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 *
 * STT client — sends WAV audio to faster-whisper HTTP server for transcription.
 */

#include "stt_client.h"
#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "cJSON.h"

static const char *TAG = "stt";

static char s_host[64] = "";
static int  s_port = 5051;

void stt_init(const char *host, int port)
{
    strncpy(s_host, host, sizeof(s_host) - 1);
    s_port = port;
    ESP_LOGI(TAG, "STT init: %s:%d", s_host, s_port);
}

/* Build a WAV file in PSRAM from 16-bit mono PCM */
static uint8_t *build_wav(const int16_t *pcm, size_t num_samples, int sample_rate, size_t *out_len)
{
    uint32_t data_size = (uint32_t)(num_samples * 2);
    uint32_t file_size = 44 + data_size;
    uint8_t *buf = heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) return NULL;

    memcpy(buf, "RIFF", 4);
    uint32_t chunk_size = file_size - 8;
    memcpy(buf + 4, &chunk_size, 4);
    memcpy(buf + 8, "WAVE", 4);

    memcpy(buf + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    memcpy(buf + 16, &fmt_size, 4);
    uint16_t audio_fmt = 1;
    memcpy(buf + 20, &audio_fmt, 2);
    uint16_t channels = 1;
    memcpy(buf + 22, &channels, 2);
    uint32_t sr = (uint32_t)sample_rate;
    memcpy(buf + 24, &sr, 4);
    uint32_t byte_rate = sr * 2;
    memcpy(buf + 28, &byte_rate, 4);
    uint16_t block_align = 2;
    memcpy(buf + 32, &block_align, 2);
    uint16_t bits = 16;
    memcpy(buf + 34, &bits, 2);

    memcpy(buf + 36, "data", 4);
    memcpy(buf + 40, &data_size, 4);
    memcpy(buf + 44, pcm, data_size);

    *out_len = file_size;
    return buf;
}

/* HTTP response handler — accumulate body into user_data buffer */
typedef struct {
    char  *buf;
    size_t buf_len;
    size_t pos;
} http_resp_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_ctx_t *ctx = (http_resp_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && ctx && evt->data_len > 0) {
        size_t avail = ctx->buf_len - ctx->pos - 1;
        size_t copy = (evt->data_len < avail) ? evt->data_len : avail;
        memcpy(ctx->buf + ctx->pos, evt->data, copy);
        ctx->pos += copy;
        ctx->buf[ctx->pos] = '\0';
    }
    return ESP_OK;
}

esp_err_t stt_transcribe(const int16_t *pcm, size_t num_samples,
                         int sample_rate, char *out_text, size_t out_len)
{
    if (!s_host[0]) {
        ESP_LOGE(TAG, "STT not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Build WAV in PSRAM */
    size_t wav_len = 0;
    uint8_t *wav = build_wav(pcm, num_samples, sample_rate, &wav_len);
    if (!wav) {
        ESP_LOGE(TAG, "WAV build OOM");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "WAV built: %u bytes (%u samples @ %dHz)",
             (unsigned)wav_len, (unsigned)num_samples, sample_rate);

    /* Prepare URL — OpenAI-compatible endpoint */
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/v1/audio/transcriptions", s_host, s_port);

    /* Build multipart/form-data body in PSRAM */
    static const char *boundary = "----HeyClawyBoundary";
    const char *part_header =
        "------HeyClawyBoundary\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    const char *part_model =
        "\r\n------HeyClawyBoundary\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "Systran/faster-whisper-small";
    const char *part_end = "\r\n------HeyClawyBoundary--\r\n";

    size_t hdr_len = strlen(part_header);
    size_t mdl_len = strlen(part_model);
    size_t end_len = strlen(part_end);
    size_t body_len = hdr_len + wav_len + mdl_len + end_len;

    uint8_t *body = heap_caps_malloc(body_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!body) {
        free(wav);
        return ESP_ERR_NO_MEM;
    }
    memcpy(body, part_header, hdr_len);
    memcpy(body + hdr_len, wav, wav_len);
    memcpy(body + hdr_len + wav_len, part_model, mdl_len);
    memcpy(body + hdr_len + wav_len + mdl_len, part_end, end_len);
    free(wav);

    /* Response buffer in PSRAM */
    char *resp_buf = heap_caps_calloc(1, 4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!resp_buf) {
        free(body);
        return ESP_ERR_NO_MEM;
    }

    http_resp_ctx_t resp_ctx = { .buf = resp_buf, .buf_len = 4096, .pos = 0 };

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
        .event_handler = http_event_handler,
        .user_data = &resp_ctx,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(body);
        free(resp_buf);
        return ESP_FAIL;
    }

    char ct[80];
    snprintf(ct, sizeof(ct), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(client, "Content-Type", ct);
    esp_http_client_set_post_field(client, (const char *)body, body_len);

    int64_t t0 = esp_timer_get_time();
    esp_err_t err = esp_http_client_perform(client);
    int64_t elapsed_ms = (esp_timer_get_time() - t0) / 1000;

    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(body);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        free(resp_buf);
        return err;
    }

    ESP_LOGI(TAG, "STT response: status=%d len=%d time=%lldms", status, (int)resp_ctx.pos, elapsed_ms);

    if (status != 200) {
        ESP_LOGE(TAG, "STT error: %s", resp_buf);
        free(resp_buf);
        return ESP_FAIL;
    }

    /* Parse JSON response: {"text": "...", "language": "en", "duration": 3.0} */
    cJSON *json = cJSON_Parse(resp_buf);
    free(resp_buf);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse STT JSON response");
        return ESP_FAIL;
    }

    cJSON *text_item = cJSON_GetObjectItem(json, "text");
    if (text_item && cJSON_IsString(text_item) && text_item->valuestring[0]) {
        strncpy(out_text, text_item->valuestring, out_len - 1);
        out_text[out_len - 1] = '\0';

        /* Log detected language if available */
        cJSON *lang_item = cJSON_GetObjectItem(json, "language");
        if (lang_item && cJSON_IsString(lang_item)) {
            ESP_LOGI(TAG, "Transcribed (%lldms, lang=%s): %s",
                     elapsed_ms, lang_item->valuestring, out_text);
        } else {
            ESP_LOGI(TAG, "Transcribed (%lldms): %s", elapsed_ms, out_text);
        }
    } else {
        ESP_LOGW(TAG, "STT returned empty text");
        strncpy(out_text, "", out_len - 1);
        cJSON_Delete(json);
        return ESP_ERR_NOT_FOUND;
    }

    cJSON_Delete(json);
    return ESP_OK;
}
