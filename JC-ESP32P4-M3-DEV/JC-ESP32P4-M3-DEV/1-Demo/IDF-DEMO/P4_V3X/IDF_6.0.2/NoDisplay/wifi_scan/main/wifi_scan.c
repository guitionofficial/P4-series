#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"

#define TAG  "app_main"
#define SCAN_LIST_SIZE 20

/* 是否允许保持连接：
 *  - false（默认）：C6 因 AUTO_CONNECT 自动连上的 AP 会被立即断开
 *  - true ：仅当手动调用 connect_to_ap() 后才允许建立连接 */
static bool s_allow_connect = false;

/* scan 因 auto-connect 占用 WiFi 状态而失败时，置位等待断开后重试 */
static bool s_scan_pending_retry = false;
static uint8_t s_scan_retry_count = 0;
#define SCAN_RETRY_MAX 3

/* 扫描完成后打印 AP 列表 */
static void dump_scan_results(void)
{
    uint16_t ap_count = 0;
    uint16_t number = SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[SCAN_LIST_SIZE];
    memset(ap_info, 0, sizeof(ap_info));

    if (esp_wifi_scan_get_ap_num(&ap_count) != ESP_OK) {
        ESP_LOGE(TAG, "scan_get_ap_num failed");
        return;
    }
    ESP_LOGI(TAG, "AP count = %u", ap_count);
    if (ap_count == 0) {
        ESP_LOGW(TAG, "No AP found");
        return;
    }

    number = (ap_count > SCAN_LIST_SIZE) ? SCAN_LIST_SIZE : ap_count;
    if (esp_wifi_scan_get_ap_records(&number, ap_info) != ESP_OK) {
        ESP_LOGE(TAG, "scan_get_ap_records failed");
        return;
    }

    ESP_LOGI(TAG, "-------------------------------------------------------");
    ESP_LOGI(TAG, "|  #   SSID                            RSSI    CH    |");
    for (int i = 0; i < number; i++) {
        ESP_LOGI(TAG, "| %-3d  %-32s %4d  %3u  |",
                 i, ap_info[i].ssid, ap_info[i].rssi, ap_info[i].primary);
    }
    ESP_LOGI(TAG, "-------------------------------------------------------");
}

/* 发起一次全信道主动扫描（非阻塞，结果在 SCAN_DONE 后取） */
static void start_scan(void)
{
    wifi_scan_config_t cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    esp_err_t err = esp_wifi_scan_start(&cfg, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "scan_start failed: %s", esp_err_to_name(err));
        /* STA 刚启动时 C6 可能在 AUTO_CONNECT/连接流程中，
         * 等它被断开（STA_DISCONNECTED）后再重试 */
        s_scan_pending_retry = true;
    } else {
        s_scan_pending_retry = false;
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base != WIFI_EVENT) {
        return;
    }

    switch (id) {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "STA started, start scanning...");
        s_scan_pending_retry = false;
        s_scan_retry_count = 0;
        start_scan();
        break;

    case WIFI_EVENT_SCAN_DONE:
        ESP_LOGI(TAG, "Scan done");
        s_scan_pending_retry = false;
        dump_scan_results();
        break;

    case WIFI_EVENT_STA_CONNECTED:
        if (!s_allow_connect) {
            /* C6 自动连上了保存的 AP，这里主动断开它 */
            ESP_LOGW(TAG, "Auto-connected AP, force disconnect (manual connect only)");
            esp_wifi_disconnect();
        } else {
            ESP_LOGI(TAG, "STA connected (manual)");
        }
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t *dis = data;
            ESP_LOGI(TAG, "STA disconnected, reason=%d",
                     dis ? dis->reason : -1);

            /* 现在 WiFi 处于干净的未连接状态，重试之前失败的 scan */
            if (s_scan_pending_retry && s_scan_retry_count < SCAN_RETRY_MAX) {
                s_scan_retry_count++;
                ESP_LOGW(TAG, "WiFi state is clean now, retry scan (%d/%d)...",
                         s_scan_retry_count, SCAN_RETRY_MAX);
                start_scan();
            } else if (s_scan_pending_retry) {
                ESP_LOGE(TAG, "Scan keeps failing. Disable AUTO_CONNECT in the "
                              "C6 slave firmware to fix.");
            }
        }
        break;

    default:
        break;
    }
}

/* 手动连接指定 AP（调用前会先断开已连接的网络）。
 * 注意：调用该函数后 s_allow_connect=true，连接不会被自动断开。
 * 当前未启用，需要时在 app_main 中调用即可。 */
static void __attribute__((unused)) connect_to_ap(const char *ssid, const char *password)
{
    if (!ssid) {
        return;
    }
    s_allow_connect = true;

    esp_wifi_disconnect();   /* 连接前先断开已连接的 WiFi */
    vTaskDelay(pdMS_TO_TICKS(200));

    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_cfg.sta.password, password,
                sizeof(wifi_cfg.sta.password) - 1);
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "Connecting to %s ...", ssid);
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 示例：连接前自动断开已连接 wifi，再连目标 AP
     * connect_to_ap("your_ssid", "your_password"); */
}
