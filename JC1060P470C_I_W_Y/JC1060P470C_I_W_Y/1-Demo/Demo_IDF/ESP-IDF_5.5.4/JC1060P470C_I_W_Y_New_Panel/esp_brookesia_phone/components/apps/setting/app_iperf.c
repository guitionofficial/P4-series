/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "iperf.h"
#include "app_iperf.h"

#define TAG "app_iperf"

#define IPERF_SERVER_TASK_STACK   (4096)
#define IPERF_SERVER_TASK_PRIO    (5)

typedef enum {
    IPERF_SERVER_IDLE,
    IPERF_SERVER_RUNNING,
    IPERF_SERVER_STOPPING,
} app_iperf_server_state_t;

typedef struct {
    app_iperf_status_t status;
    app_iperf_server_state_t state;
    TaskHandle_t task_handle;
    uint32_t session_count;
    bool use_tcp;   /* true for TCP, false for UDP */
} app_iperf_ctx_t;

static app_iperf_ctx_t s_ctx = {0};

static void iperf_hook(iperf_traffic_type_t type, iperf_status_t status)
{
    if (type != IPERF_UDP_SERVER && type != IPERF_TCP_SERVER) return;

    if (status == IPERF_STARTED) {
        ESP_LOGI(TAG, "server session started");
        s_ctx.status.client_connected = true;
        s_ctx.session_count++;
    } else {
        ESP_LOGI(TAG, "server session ended");
        s_ctx.status.client_connected = false;
        memset(s_ctx.status.client_ip, 0, sizeof(s_ctx.status.client_ip));
    }
}

static void app_iperf_server_task(void *arg)
{
    ESP_LOGI(TAG, "Server task started");
    s_ctx.state = IPERF_SERVER_RUNNING;

    while (s_ctx.state == IPERF_SERVER_RUNNING) {
        uint32_t proto_flag = s_ctx.use_tcp ? IPERF_FLAG_TCP : IPERF_FLAG_UDP;
        iperf_cfg_t cfg = {
            .flag = IPERF_FLAG_SERVER | proto_flag,
            .type = IPERF_IP_TYPE_IPV4,
            .dport = IPERF_DEFAULT_PORT,
            .sport = IPERF_DEFAULT_PORT,
            .interval = IPERF_DEFAULT_INTERVAL,
            .time = 86400,
            .bw_lim = IPERF_DEFAULT_NO_BW_LIMIT,
        };

        esp_err_t err = iperf_start(&cfg);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "iperf_start failed, retrying in 3s...");
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        s_ctx.status.is_running = true;
        s_ctx.status.current_speed_mbps = 0;
        strcpy(s_ctx.status.status_text, "Running - waiting for client...");

        while (g_iperf_is_running && s_ctx.state == IPERF_SERVER_RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        s_ctx.status.is_running = false;
        s_ctx.status.current_speed_mbps = 0;

        if (s_ctx.state == IPERF_SERVER_STOPPING) {
            iperf_stop();
            strcpy(s_ctx.status.status_text, "Server stopped");
            break;
        }

        strcpy(s_ctx.status.status_text, "Server restarting...");
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    s_ctx.state = IPERF_SERVER_IDLE;
    s_ctx.status.is_running = false;
    s_ctx.task_handle = NULL;
    ESP_LOGI(TAG, "Server task exiting");
    vTaskDelete(NULL);
}

/* Public API */

esp_err_t app_iperf_init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = IPERF_SERVER_IDLE;
    s_ctx.status.active_iface = APP_IPERF_IFACE_NONE;
    strcpy(s_ctx.status.status_text, "Ready");
    iperf_register_hook_func(iperf_hook);
    return ESP_OK;
}

static esp_err_t app_iperf_start_common(app_iperf_iface_t iface, bool use_tcp)
{
    /* Mutual exclusion: stop other interface test if running */
    if (s_ctx.state == IPERF_SERVER_RUNNING) {
        if (s_ctx.status.active_iface == iface) {
            ESP_LOGW(TAG, "Server already running on this interface");
            return ESP_ERR_INVALID_STATE;
        }
        /* Stop the other interface test */
        ESP_LOGI(TAG, "Stopping current test to switch interface");
        app_iperf_stop();
        /* Small delay to ensure clean stop */
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    s_ctx.state = IPERF_SERVER_RUNNING;
    s_ctx.session_count = 0;
    s_ctx.use_tcp = use_tcp;
    memset(s_ctx.status.client_ip, 0, sizeof(s_ctx.status.client_ip));
    s_ctx.status.max_speed_mbps = 0;
    s_ctx.status.current_speed_mbps = 0;
    s_ctx.status.active_iface = iface;
    s_ctx.status.client_connected = false;

    strcpy(s_ctx.status.status_text, "Starting...");

    BaseType_t ret = xTaskCreatePinnedToCore(app_iperf_server_task, "iperf_srv",
                                  IPERF_SERVER_TASK_STACK, NULL,
                                  IPERF_SERVER_TASK_PRIO, &s_ctx.task_handle, 0);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create server task");
        s_ctx.state = IPERF_SERVER_IDLE;
        s_ctx.status.active_iface = APP_IPERF_IFACE_NONE;
        strcpy(s_ctx.status.status_text, "Failed to start");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t app_iperf_start_udp_server(void)
{
    return app_iperf_start_common(APP_IPERF_IFACE_ETHERNET, false);
}

esp_err_t app_iperf_start_wifi_server(void)
{
    return app_iperf_start_common(APP_IPERF_IFACE_WIFI, true);
}

esp_err_t app_iperf_stop(void)
{
    if (s_ctx.state == IPERF_SERVER_IDLE) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.state = IPERF_SERVER_STOPPING;
    s_ctx.status.is_running = false;

    if (g_iperf_is_running) {
        iperf_stop();
    }

    for (int i = 0; i < 20; i++) {
        if (s_ctx.task_handle == NULL) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (s_ctx.task_handle) {
        vTaskDelete(s_ctx.task_handle);
        s_ctx.task_handle = NULL;
    }

    s_ctx.state = IPERF_SERVER_IDLE;
    s_ctx.status.client_connected = false;
    s_ctx.status.active_iface = APP_IPERF_IFACE_NONE;
    strcpy(s_ctx.status.status_text, "Server stopped");
    return ESP_OK;
}

bool app_iperf_is_running(void)
{
    return s_ctx.state == IPERF_SERVER_RUNNING;
}

app_iperf_iface_t app_iperf_get_active_iface(void)
{
    return s_ctx.status.active_iface;
}

const app_iperf_status_t* app_iperf_get_status(void)
{
    return &s_ctx.status;
}
