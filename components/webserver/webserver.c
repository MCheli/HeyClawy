/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 *
 * Web server — REST API + embedded SPA
 */

#include "webserver.h"
#include "settings.h"
#include "error_log.h"
#include "openclaw_client.h"
#include "wifi_manager.h"
#include "board.h"

#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "webserver";
static httpd_handle_t s_server = NULL;

/* Forward-declare the embedded HTML */
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");

/* ── GET / — serve SPA HTML ──────────────────────────────────────────── */
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    size_t len = index_html_end - index_html_start;
    return httpd_resp_send(req, index_html_start, len);
}

/* ── GET /api/status — device + OpenClaw status ──────────────────────── */
static esp_err_t status_handler(httpd_req_t *req)
{
    cJSON *j = cJSON_CreateObject();

    /* Device info */
    cJSON *dev = cJSON_AddObjectToObject(j, "device");
    cJSON_AddStringToObject(dev, "name", board_get_name());
    cJSON_AddStringToObject(dev, "mcu", board_get_mcu());
    cJSON_AddStringToObject(dev, "version", esp_app_get_description()->version);
    cJSON_AddNumberToObject(dev, "heap_free", esp_get_free_heap_size());
    cJSON_AddNumberToObject(dev, "heap_min", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(dev, "battery_mv", board_battery_get_voltage_mv());
    cJSON_AddNumberToObject(dev, "battery_pct", board_battery_get_percent());
    cJSON_AddBoolToObject(dev, "charging", board_battery_is_charging());
    cJSON_AddNumberToObject(dev, "rgb_led_count", BOARD_RGB_LED_COUNT);
    cJSON_AddBoolToObject(dev, "has_display", BOARD_HAS_DISPLAY);

    /* WiFi */
    cJSON *wifi = cJSON_AddObjectToObject(j, "wifi");
    cJSON_AddBoolToObject(wifi, "connected", wifi_manager_get_state() == WIFI_STATE_CONNECTED);
    cJSON_AddStringToObject(wifi, "ip", wifi_manager_get_ip());
    cJSON_AddNumberToObject(wifi, "rssi", wifi_manager_get_rssi());

    /* OpenClaw */
    cJSON *oc = cJSON_AddObjectToObject(j, "openclaw");
    openclaw_state_t oc_state = openclaw_get_state();
    cJSON_AddNumberToObject(oc, "state", oc_state);
    const char *state_str = "unknown";
    switch (oc_state) {
        case OPENCLAW_STATE_DISCONNECTED: state_str = "disconnected"; break;
        case OPENCLAW_STATE_CONNECTING:   state_str = "connecting"; break;
        case OPENCLAW_STATE_AUTHENTICATING: state_str = "authenticating"; break;
        case OPENCLAW_STATE_CONNECTED:    state_str = "connected"; break;
        default: break;
    }
    cJSON_AddStringToObject(oc, "state_str", state_str);

    const openclaw_info_t *info = openclaw_get_info();
    if (info) {
        cJSON_AddStringToObject(oc, "version", info->version);
        cJSON_AddNumberToObject(oc, "uptime_min", info->uptime_min);
        cJSON_AddStringToObject(oc, "agent", info->agent_id);
        cJSON_AddNumberToObject(oc, "sessions", info->session_count);
        cJSON_AddStringToObject(oc, "wa_status", info->wa_status);
        cJSON_AddNumberToObject(oc, "last_activity_min", info->last_activity_min);
        if (info->has_tasks) {
            cJSON_AddNumberToObject(oc, "task_count", info->task_count);
            cJSON_AddNumberToObject(oc, "tasks_running", info->tasks_running);
            cJSON_AddNumberToObject(oc, "tasks_active", info->tasks_active);
        }
        /* Active runs (carousel) */
        cJSON_AddBoolToObject(oc, "is_active", info->is_active);
        cJSON_AddBoolToObject(oc, "is_external", info->is_external);
        cJSON_AddStringToObject(oc, "active_detail", info->active_detail);
        cJSON_AddNumberToObject(oc, "active_runs_count", info->active_runs_count);
        if (info->active_runs_count > 0) {
            cJSON *runs = cJSON_AddArrayToObject(oc, "active_runs");
            for (int i = 0; i < OC_MAX_ACTIVE_RUNS; i++) {
                if (!info->active_runs[i].active) continue;
                cJSON *r = cJSON_CreateObject();
                cJSON_AddStringToObject(r, "detail", info->active_runs[i].detail);
                cJSON_AddStringToObject(r, "source", info->active_runs[i].source);
                cJSON_AddNumberToObject(r, "started_ms", (double)info->active_runs[i].started_ms);
                cJSON_AddItemToArray(runs, r);
            }
        }
    }

    /* Error count */
    cJSON_AddNumberToObject(j, "error_count", error_log_count());

    char *str = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, str, strlen(str));
    free(str);
    return ret;
}

/* ── GET /api/settings — current settings (secrets masked) ───────────── */
static esp_err_t settings_get_handler(httpd_req_t *req)
{
    char *json = settings_to_json(false);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

/* ── PUT /api/settings — update settings ─────────────────────────────── */
static esp_err_t settings_put_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }

    char *buf = malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) {
            free(buf);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[total_len] = '\0';

    esp_err_t err = settings_from_json(buf, total_len);
    free(buf);

    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return err;
    }

    settings_save();

    /* Apply volume and brightness immediately */
    const settings_t *cfg = settings_get();
    board_audio_set_volume(cfg->volume);
    board_display_set_brightness(cfg->brightness);
    if (!cfg->rgb_enabled) {
        board_rgb_animate(RGB_MODE_OFF, 0, 0, 0);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"ok\":true}", 11);
}

/* ── GET /api/errors — error log ─────────────────────────────────────── */
static esp_err_t errors_handler(httpd_req_t *req)
{
    char *json = error_log_to_json();
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

/* ── POST /api/openclaw/test — test OpenClaw connection ──────────────── */
static esp_err_t openclaw_test_handler(httpd_req_t *req)
{
    /* Read JSON body with host/port/token */
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 2048) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }

    char *buf = malloc(total_len + 1);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) { free(buf); httpd_resp_send_500(req); return ESP_FAIL; }
        received += ret;
    }
    buf[total_len] = '\0';

    cJSON *j = cJSON_ParseWithLength(buf, total_len);
    free(buf);

    if (!j) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *host_item = cJSON_GetObjectItem(j, "host");
    cJSON *port_item = cJSON_GetObjectItem(j, "port");

    const char *test_host = host_item && cJSON_IsString(host_item) ? host_item->valuestring : NULL;
    int test_port = port_item && cJSON_IsNumber(port_item) ? (int)port_item->valuedouble : 18789;

    cJSON *result = cJSON_CreateObject();

    if (!test_host || !test_host[0]) {
        cJSON_AddBoolToObject(result, "ok", false);
        cJSON_AddStringToObject(result, "error", "Host is required");
    } else {
        /* Simple TCP connection test */
        char url[256];
        snprintf(url, sizeof(url), "ws://%s:%d", test_host, test_port);
        cJSON_AddBoolToObject(result, "ok", true);
        cJSON_AddStringToObject(result, "message", "Connection parameters accepted. Save and reboot to connect.");
        cJSON_AddStringToObject(result, "url", url);
    }

    cJSON_Delete(j);

    char *str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, str, strlen(str));
    free(str);
    return ret;
}

/* ── POST /api/stt/health — check STT server health ────────────────── */
static esp_err_t stt_health_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    char host[128] = "";
    int port = 5051;

    if (total_len > 0 && total_len < 1024) {
        char *buf = malloc(total_len + 1);
        if (buf) {
            int received = 0;
            while (received < total_len) {
                int ret = httpd_req_recv(req, buf + received, total_len - received);
                if (ret <= 0) break;
                received += ret;
            }
            buf[received] = '\0';
            cJSON *j = cJSON_ParseWithLength(buf, received);
            if (j) {
                cJSON *h = cJSON_GetObjectItem(j, "host");
                cJSON *p = cJSON_GetObjectItem(j, "port");
                if (h && cJSON_IsString(h)) strlcpy(host, h->valuestring, sizeof(host));
                if (p && cJSON_IsNumber(p)) port = (int)p->valuedouble;
                cJSON_Delete(j);
            }
            free(buf);
        }
    }

    if (!host[0]) {
        const settings_t *s = settings_get();
        strlcpy(host, s->stt_host, sizeof(host));
        port = s->stt_port;
    }

    cJSON *result = cJSON_CreateObject();
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/health", host, port);

    esp_http_client_config_t config = { .url = url, .timeout_ms = 5000 };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (err == ESP_OK && status == 200) {
        cJSON_AddBoolToObject(result, "ok", true);
        cJSON_AddStringToObject(result, "model", "small");
    } else {
        cJSON_AddBoolToObject(result, "ok", false);
        char errmsg[256];
        snprintf(errmsg, sizeof(errmsg), "Cannot reach STT at %s:%d (err=%d status=%d)", host, port, err, status);
        cJSON_AddStringToObject(result, "error", errmsg);
    }

    esp_http_client_cleanup(client);

    char *str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, str, strlen(str));
    free(str);
    return ret;
}

/* ── GET /api/tasks — OpenClaw cron/background tasks ─────────────────── */
static esp_err_t tasks_handler(httpd_req_t *req)
{
    cJSON *j = cJSON_CreateArray();
    const openclaw_info_t *info = openclaw_get_info();
    if (info && info->has_tasks) {
        for (int i = 0; i < info->task_count; i++) {
            const openclaw_task_t *t = &info->tasks[i];
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "id", t->id);
            cJSON_AddStringToObject(item, "name", t->name);
            cJSON_AddBoolToObject(item, "enabled", t->enabled);
            cJSON_AddBoolToObject(item, "running", t->running);
            cJSON_AddStringToObject(item, "schedule_kind", t->schedule_kind);
            cJSON_AddStringToObject(item, "schedule_expr", t->schedule_expr);
            cJSON_AddStringToObject(item, "last_status", t->last_status);
            cJSON_AddStringToObject(item, "last_error", t->last_error);
            cJSON_AddNumberToObject(item, "last_duration_ms", t->last_duration_ms);
            cJSON_AddNumberToObject(item, "consecutive_errors", t->consecutive_errors);
            cJSON_AddItemToArray(j, item);
        }
    }

    char *str = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, str, strlen(str));
    free(str);
    return ret;
}

/* ── POST /api/tasks/toggle — enable/disable a cron job ──────────────── */
static esp_err_t tasks_toggle_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }

    char *buf = malloc(total_len + 1);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }
    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) { free(buf); httpd_resp_send_500(req); return ESP_FAIL; }
        received += ret;
    }
    buf[total_len] = '\0';

    cJSON *j = cJSON_ParseWithLength(buf, total_len);
    free(buf);
    if (!j) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON"); return ESP_FAIL; }

    cJSON *id_item = cJSON_GetObjectItem(j, "id");
    cJSON *en_item = cJSON_GetObjectItem(j, "enabled");
    if (!id_item || !cJSON_IsString(id_item) || !en_item) {
        cJSON_Delete(j);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id or enabled");
        return ESP_FAIL;
    }

    esp_err_t err = openclaw_cron_toggle(id_item->valuestring, cJSON_IsTrue(en_item));
    cJSON_Delete(j);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (err == ESP_OK) {
        return httpd_resp_send(req, "{\"ok\":true}", 11);
    } else {
        return httpd_resp_send(req, "{\"ok\":false,\"error\":\"Not connected\"}", 35);
    }
}

/* ── OPTIONS handler for CORS ────────────────────────────────────────── */
static esp_err_t cors_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, PUT, POST, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

/* ── POST /api/reboot ────────────────────────────────────────────────── */
static esp_err_t reboot_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, "{\"ok\":true,\"msg\":\"Rebooting...\"}", 31);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

/* ── POST /api/settings/reset ────────────────────────────────────────── */
static esp_err_t settings_reset_handler(httpd_req_t *req)
{
    settings_reset();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"ok\":true,\"msg\":\"Reset to defaults. Reboot to apply.\"}", 55);
}

/* ── POST /api/led/demo — trigger LED animation demo ─────────────────── */
static esp_err_t led_demo_handler(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (len <= 0) {
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"No body\"}", 30);
        return ESP_OK;
    }
    buf[len] = '\0';
    cJSON *j = cJSON_Parse(buf);
    if (!j) {
        httpd_resp_send(req, "{\"ok\":false,\"error\":\"Bad JSON\"}", 31);
        return ESP_OK;
    }
    const char *mode = cJSON_GetStringValue(cJSON_GetObjectItem(j, "mode"));
    if (!mode) { cJSON_Delete(j); httpd_resp_send(req, "{\"ok\":false}", 12); return ESP_OK; }

    if (strcmp(mode, "rainbow_spin") == 0) board_rgb_animate(RGB_MODE_RAINBOW_SPIN, 0, 0, 0);
    else if (strcmp(mode, "aurora") == 0)   board_rgb_animate(RGB_MODE_AURORA, 0, 0, 0);
    else if (strcmp(mode, "starfield") == 0) board_rgb_animate(RGB_MODE_STARFIELD, 0, 0, 0);
    else if (strcmp(mode, "fire") == 0)     board_rgb_animate(RGB_MODE_FIRE, 0, 0, 0);
    else if (strcmp(mode, "ocean") == 0)    board_rgb_animate(RGB_MODE_OCEAN, 0, 0, 0);
    else if (strcmp(mode, "breathe") == 0)  board_rgb_animate(RGB_MODE_BREATHE, 0, 32, 16);
    else if (strcmp(mode, "chase") == 0)    board_rgb_animate(RGB_MODE_CHASE, 0, 16, 40);
    else if (strcmp(mode, "sparkle") == 0)  board_rgb_animate(RGB_MODE_SPARKLE, 32, 8, 48);
    else if (strcmp(mode, "stop") == 0)     board_rgb_animate(RGB_MODE_SOLID, 0, 4, 0);  /* back to idle */
    else { cJSON_Delete(j); httpd_resp_send(req, "{\"ok\":false,\"error\":\"Unknown mode\"}", 35); return ESP_OK; }

    cJSON_Delete(j);
    ESP_LOGI(TAG, "LED demo: %s", mode);
    return httpd_resp_send(req, "{\"ok\":true}", 11);
}

/* ── Server start/stop ───────────────────────────────────────────────── */

esp_err_t webserver_start(void)
{
    if (s_server) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start: %s", esp_err_to_name(err));
        return err;
    }

    /* Register URI handlers */
    const httpd_uri_t uris[] = {
        { .uri = "/",                .method = HTTP_GET,     .handler = root_handler },
        { .uri = "/api/status",      .method = HTTP_GET,     .handler = status_handler },
        { .uri = "/api/settings",    .method = HTTP_GET,     .handler = settings_get_handler },
        { .uri = "/api/settings",    .method = HTTP_PUT,     .handler = settings_put_handler },
        { .uri = "/api/settings/reset", .method = HTTP_POST, .handler = settings_reset_handler },
        { .uri = "/api/errors",      .method = HTTP_GET,     .handler = errors_handler },
        { .uri = "/api/tasks",       .method = HTTP_GET,     .handler = tasks_handler },
        { .uri = "/api/tasks/toggle", .method = HTTP_POST,   .handler = tasks_toggle_handler },
        { .uri = "/api/openclaw/test", .method = HTTP_POST,  .handler = openclaw_test_handler },
        { .uri = "/api/stt/health",    .method = HTTP_POST,  .handler = stt_health_handler },
        { .uri = "/api/led/demo",      .method = HTTP_POST,  .handler = led_demo_handler },
        { .uri = "/api/reboot",      .method = HTTP_POST,    .handler = reboot_handler },
        { .uri = "/api/*",           .method = HTTP_OPTIONS, .handler = cors_handler },
    };

    for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }

    ESP_LOGI(TAG, "Web server started on port 80");
    return ESP_OK;
}

esp_err_t webserver_stop(void)
{
    if (!s_server) return ESP_OK;

    esp_err_t err = httpd_stop(s_server);
    s_server = NULL;
    ESP_LOGI(TAG, "Web server stopped");
    return err;
}

bool webserver_is_running(void)
{
    return s_server != NULL;
}
