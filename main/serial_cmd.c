/*
 * SPDX-FileCopyrightText: 2024-2026 HeyClawy Contributors
 * SPDX-License-Identifier: MIT
 *
 * Serial console commands
 */

#include "serial_cmd.h"
#include "app_state.h"
#include "settings.h"

#include "board.h"
#include "openclaw_client.h"
#include "wifi_manager.h"
#include "ui.h"
#include "tts_client.h"

#include "app_tasks.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "serial";

static void serial_task(void *arg)
{
    char cmd_buf[128];
    int cmd_pos = 0;

    ESP_LOGI(TAG, "Commands: talk, say <msg>, play, details, status, web, tasks, camera, cron-add-test, cron-remove <id>, abort, wake, deepsleep, reboot");

    while (1) {
        int c = fgetc(stdin);
        if (c == EOF) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        if (c == '\n' || c == '\r') {
            if (cmd_pos > 0) {
                cmd_buf[cmd_pos] = '\0';
                cmd_pos = 0;

                /* Any serial command resets the activity timer (wakes display) */
                app_reset_activity_timer();

                if (strcmp(cmd_buf, "talk") == 0 || strcmp(cmd_buf, "t") == 0) {
                    xEventGroupSetBits(g_app_events, KNOB_PRESSED_BIT);
                } else if (strcmp(cmd_buf, "play") == 0 || strcmp(cmd_buf, "p") == 0) {
                    xEventGroupSetBits(g_app_events, TTS_PLAY_BIT);
                } else if (strcmp(cmd_buf, "details") == 0 || strcmp(cmd_buf, "d") == 0) {
                    xEventGroupSetBits(g_app_events, DETAILS_BIT);
                } else if (strcmp(cmd_buf, "web") == 0 || strcmp(cmd_buf, "w") == 0) {
                    xEventGroupSetBits(g_app_events, WEBSERVER_TOGGLE_BIT);
                } else if (strcmp(cmd_buf, "tasks") == 0) {
                    xEventGroupSetBits(g_app_events, TASKS_SCREEN_BIT);
                } else if (strcmp(cmd_buf, "abort") == 0) {
                    ESP_LOGI(TAG, "Aborting chat...");
                    openclaw_chat_abort();
                    app_set_state(UI_STATE_IDLE);
                } else if (strcmp(cmd_buf, "cron-add-test") == 0) {
                    ESP_LOGI(TAG, "Creating test cron job...");
                    openclaw_cron_add("HeyClawy Test",
                                      "10000",  /* every 10 seconds */
                                      "Check the current time and say it.");
                    /* Refresh tasks after a short delay */
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    openclaw_request_tasks();
                } else if (strncmp(cmd_buf, "cron-remove ", 12) == 0) {
                    ESP_LOGI(TAG, "Removing cron job: %s", cmd_buf + 12);
                    openclaw_cron_remove(cmd_buf + 12);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    openclaw_request_tasks();
                } else if (strcmp(cmd_buf, "status") == 0 || strcmp(cmd_buf, "s") == 0) {
                    ESP_LOGI(TAG, "[STATUS] UI=%d OC=%d WiFi=%s Heap=%lu",
                             ui_get_state(), openclaw_get_state(),
                             wifi_manager_get_ip(),
                             (unsigned long)esp_get_free_heap_size());
                    const openclaw_info_t *info = openclaw_get_info();
                    if (info->has_tasks) {
                        ESP_LOGI(TAG, "[TASKS] %s", info->task_summary);
                        for (int i = 0; i < info->task_count; i++) {
                            ESP_LOGI(TAG, "  [%d] %s id=%s enabled=%d running=%d last=%s err=%s",
                                     i, info->tasks[i].name, info->tasks[i].id,
                                     info->tasks[i].enabled, info->tasks[i].running,
                                     info->tasks[i].last_status, info->tasks[i].last_error);
                        }
                    }
                } else if (strcmp(cmd_buf, "camera") == 0 || strcmp(cmd_buf, "cam") == 0) {
                    xEventGroupSetBits(g_app_events, CAMERA_BIT);
                } else if (strcmp(cmd_buf, "reboot") == 0) {
                    esp_restart();
                } else if (strcmp(cmd_buf, "deepsleep") == 0) {
                    ESP_LOGI(TAG, "Deep sleep command");
                    app_enter_deep_sleep();
                } else if (strcmp(cmd_buf, "wake") == 0) {
                    ESP_LOGI(TAG, "Wake command — display on");
                } else if (strncmp(cmd_buf, "say ", 4) == 0) {
                    if (openclaw_get_state() != OPENCLAW_STATE_CONNECTED) {
                        ESP_LOGW(TAG, "OC not connected");
                        continue;
                    }
                    app_set_state(UI_STATE_SENDING);
                    if (g_pending_jpeg && g_pending_jpeg_size > 0) {
                        ESP_LOGI(TAG, "Sending text + image (%d bytes)", (int)g_pending_jpeg_size);
                        openclaw_chat_send_with_image(cmd_buf + 4, g_pending_jpeg, g_pending_jpeg_size, app_on_chat_response);
                        free(g_pending_jpeg);
                        g_pending_jpeg = NULL;
                        g_pending_jpeg_size = 0;
                    } else {
                        openclaw_chat_send(cmd_buf + 4, app_on_chat_response);
                    }
                } else {
                    ESP_LOGI(TAG, "Unknown: '%s'. Try: talk, say, play, details, web, tasks, abort, status", cmd_buf);
                }
            }
        } else if (cmd_pos < (int)sizeof(cmd_buf) - 1) {
            cmd_buf[cmd_pos++] = (char)c;
        }
    }
}

void serial_cmd_task_start(void)
{
    xTaskCreatePinnedToCore(serial_task, "serial", 4096, NULL, 3, NULL, 0);
}
