#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "esp_lcd_touch.h"

/* Mirror the lvgl_port_touch_ctx_t struct from esp_lvgl_port_touch.c to access touch handle */
typedef struct {
    esp_lcd_touch_handle_t  handle;
    lv_indev_t              *indev;
    struct {
        float x;
        float y;
    } scale;
} lvgl_port_touch_ctx_t;

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = {
            .task_priority = 4,
            .task_stack = 16384,
            .task_affinity = -1,
            .task_max_sleep_ms = 500,
            .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT,
            .timer_period_ms = 5,
        },
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .sw_rotate = true,
        }
    };
   lv_disp_t *disp = bsp_display_start_with_config(&cfg);
    lv_disp_set_rotation(disp, LV_DISP_ROTATION_90);
     lv_indev_t *indev = bsp_display_get_input_dev();
    if (indev) {
        lvgl_port_touch_ctx_t *touch_ctx = (lvgl_port_touch_ctx_t *)lv_indev_get_driver_data(indev);
        if (touch_ctx && touch_ctx->handle) {
            esp_lcd_touch_set_mirror_x(touch_ctx->handle, true);
            esp_lcd_touch_set_mirror_y(touch_ctx->handle, false);
        }
    }
    bsp_display_backlight_on();

    bsp_display_lock(0);

    //lv_demo_music();
    // lv_demo_benchmark();
     lv_demo_widgets();

    bsp_display_unlock();
}
