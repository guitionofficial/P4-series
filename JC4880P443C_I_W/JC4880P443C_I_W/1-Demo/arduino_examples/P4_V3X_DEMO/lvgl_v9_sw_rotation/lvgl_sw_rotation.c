#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "src/touch/esp_lcd_touch_gt911.h"
#include "src/lcd/esp_lcd_st7701.h"
#include "lvgl_port_v9.h"
#include "demos/lv_demos.h"
#include "driver/ppa.h"

#define TAG                                 "main"


#define BSP_MIPI_DSI_PHY_PWR_LDO_CHAN       (3)  // LDO_VO3 is connected to VDD_MIPI_DPHY
#define BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

#define BSP_LCD_DPI_BUFFER_NUMS             (1)

#define BSP_LCD_H_RES                       (480)
#define BSP_LCD_V_RES                       (800)

#define BSP_I2C_NUM                         (I2C_NUM_1)
#define BSP_I2C_SDA                         (GPIO_NUM_7)
#define BSP_I2C_SCL                         (GPIO_NUM_8)

#define BSP_LCD_TOUCH_RST                   (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_INT                   (GPIO_NUM_NC)

#define BSP_LCD_RST                         (GPIO_NUM_5)

i2c_master_bus_handle_t i2c_handle = NULL; 


#define BSP_LCD_BACKLIGHT   GPIO_NUM_23
#define LCD_LEDC_CH         LEDC_CHANNEL_0
static esp_err_t bsp_display_brightness_init(void)
{
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));
    return ESP_OK;
}

static esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
    return ESP_OK;
}

static esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

static esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

IRAM_ATTR static bool mipi_dsi_lcd_on_vsync_event(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    return lvgl_port_notify_lcd_vsync();
}


void lvgl_sw_rotation_main(void)
{
    bsp_display_brightness_init();

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .i2c_port = BSP_I2C_NUM,
    };
    i2c_new_master_bus(&i2c_bus_conf, &i2c_handle);

    static esp_ldo_channel_handle_t phy_pwr_chan = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BSP_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan);
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");

    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = ST7701_PANEL_BUS_DSI_2CH_CONFIG();

    esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);

     ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_dbi_io_config_t dbi_config =ST7701_PANEL_IO_DBI_CONFIG();

    esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io);

    esp_lcd_panel_handle_t disp_panel = NULL;
    esp_lcd_dpi_panel_config_t dpi_config = ST7701_480_360_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);

    dpi_config.num_fbs = LVGL_PORT_LCD_BUFFER_NUMS;

    st7701_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
        .flags = {
            .use_mipi_interface = 1,
        },
    };
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .bits_per_pixel = 16,
        .rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB,
        .reset_gpio_num = BSP_LCD_RST,
        .vendor_config = &vendor_config,
    };
    esp_lcd_new_panel_st7701(io, &lcd_dev_config, &disp_panel);
    esp_lcd_panel_reset(disp_panel);
    esp_lcd_panel_init(disp_panel);

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
#if LVGL_PORT_AVOID_TEAR_MODE
        .on_refresh_done = mipi_dsi_lcd_on_vsync_event,
#else
        .on_color_trans_done = mipi_dsi_lcd_on_vsync_event,
#endif
    };
    esp_lcd_dpi_panel_register_event_callbacks(disp_panel, &cbs, NULL);
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = 100000;
    esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle);
     const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_LCD_TOUCH_RST, // Shared with LCD reset
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    
    esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle);

    lvgl_port_interface_t interface = (dpi_config.flags.use_dma2d) ? LVGL_PORT_INTERFACE_MIPI_DSI_DMA : LVGL_PORT_INTERFACE_MIPI_DSI_NO_DMA;
    ESP_LOGI(TAG,"interface is %d",interface);
    ESP_ERROR_CHECK(lvgl_port_init(disp_panel, tp_handle, interface));

     bsp_display_brightness_set(100);

    if(lvgl_port_lock(-1))
    {
        // lv_demo_music();
        // lv_demo_benchmark();
        lv_demo_widgets();

        lvgl_port_unlock();
    }
}
