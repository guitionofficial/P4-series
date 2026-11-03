/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_IPERF_IFACE_NONE,
    APP_IPERF_IFACE_ETHERNET,
    APP_IPERF_IFACE_WIFI,
} app_iperf_iface_t;

/**
 * @brief Current iperf server status
 */
typedef struct {
    bool is_running;
    bool client_connected;
    char client_ip[16];
    float max_speed_mbps;
    float current_speed_mbps;
    char status_text[64];
    app_iperf_iface_t active_iface;
} app_iperf_status_t;

const app_iperf_status_t* app_iperf_get_status(void);
app_iperf_iface_t app_iperf_get_active_iface(void);

esp_err_t app_iperf_start_udp_server(void);
esp_err_t app_iperf_start_wifi_server(void);
esp_err_t app_iperf_stop(void);
bool app_iperf_is_running(void);
esp_err_t app_iperf_init(void);

#ifdef __cplusplus
}
#endif
