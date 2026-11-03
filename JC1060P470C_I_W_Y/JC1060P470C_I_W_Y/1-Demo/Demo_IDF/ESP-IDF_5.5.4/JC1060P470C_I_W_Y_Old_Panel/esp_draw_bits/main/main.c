/*
 * esp_draw_bits - LVGL Canvas touch drawing application
 *
 * Entry point: initializes display hardware and runs the drawing app.
 * Drawing logic is separated into draw_app.c / draw_app.h.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "draw_app.h"

static const char *TAG = "Main";

void app_main(void)
{
    /* ---- Initialize display & touch ---- */
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size   = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma   = true,
            .buff_spiram = false,
            .sw_rotate  = false,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    /* ---- Launch drawing app ---- */
    ESP_LOGI(TAG, "Starting draw app...");

    lvgl_port_lock(0);

    static draw_app_t app;
    if (!draw_app_run(&app, 0, 0)) {
        ESP_LOGE(TAG, "Failed to start draw app!");
        lvgl_port_unlock();
        return;
    }

    lvgl_port_unlock();
    ESP_LOGI(TAG, "Ready.");

    /* ---- Main loop (LVGL timer-driven, nothing to do here) ---- */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* Not reached, but cleanup for completeness */
    draw_app_close(&app);
}
