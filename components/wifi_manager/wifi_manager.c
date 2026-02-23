/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 */

#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "wifi_mgr";
static wifi_state_t s_state = WIFI_STATE_DISCONNECTED;
static wifi_state_cb_t s_cb = NULL;
static char s_ip_str[16] = {0};
static int s_retry_count = 0;
#define MAX_RETRIES 10

static void set_state(wifi_state_t st)
{
    s_state = st;
    if (s_cb) s_cb(st);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            set_state(WIFI_STATE_CONNECTING);
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            if (s_retry_count < MAX_RETRIES) {
                s_retry_count++;
                ESP_LOGW(TAG, "Disconnected, retry %d/%d", s_retry_count, MAX_RETRIES);
                esp_wifi_connect();
            } else {
                set_state(WIFI_STATE_FAILED);
                ESP_LOGE(TAG, "WiFi connection failed after %d retries", MAX_RETRIES);
            }
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        set_state(WIFI_STATE_CONNECTED);
        ESP_LOGI(TAG, "Connected, IP: %s", s_ip_str);

        /* Set WiFi power save mode.
         * M5Stick (small battery): MAX_MODEM for aggressive modem sleep.
         * Other devices: MIN_MODEM (allows quick event wakeup). */
#if defined(CONFIG_HEYCLAWY_BOARD_M5STICKCPLUS2)
        esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
        ESP_LOGI(TAG, "WiFi PS: MAX_MODEM (M5Stick battery optimization)");
#else
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        ESP_LOGI(TAG, "WiFi PS: MIN_MODEM");
#endif

        // Start SNTP time sync (needed for auth timestamps)
        if (esp_sntp_enabled()) {
            esp_sntp_stop();
        }
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
        ESP_LOGI(TAG, "SNTP time sync started");
    }
}

esp_err_t wifi_manager_init(const char *ssid, const char *password, wifi_state_cb_t cb)
{
    s_cb = cb;

    // Init NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "NVS init failed");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "Event loop");
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "WiFi init");

    esp_event_handler_instance_t inst_any, inst_ip;
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &inst_any), TAG, "");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &inst_ip), TAG, "");

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Set STA");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Set config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "WiFi start");

    ESP_LOGI(TAG, "WiFi STA started, connecting to '%s'", ssid);
    return ESP_OK;
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_state;
}

const char *wifi_manager_get_ip(void)
{
    return s_ip_str;
}

int8_t wifi_manager_get_rssi(void)
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}
