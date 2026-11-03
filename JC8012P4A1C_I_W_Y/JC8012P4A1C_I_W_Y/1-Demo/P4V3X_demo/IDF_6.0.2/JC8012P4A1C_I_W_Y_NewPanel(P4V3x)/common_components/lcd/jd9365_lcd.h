#ifndef _JD9365_LCD_H
#define _JD9365_LCD_H

#include <stdio.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"

#define LCD_H_RES                   800
#define LCD_V_RES                   1280

#define LCD_BIT_PER_PIXEL           16

#define BSP_LCD_RST                 GPIO_NUM_27
#define BSP_LCD_BACKLIGHT           GPIO_NUM_23

#define BSP_MIPI_DSI_PHY_PWR_LDO_CHAN       3
#define BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500

#define BSP_LCD_MIPI_DSI_LANE_NUM           2
#define BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS  1000

#define LCD_LEDC_CH                 0

class jd9365_lcd
{
public:
    jd9365_lcd(int8_t lcd_rst = BSP_LCD_RST);

    void begin();
    void bsp_enable_dsi_phy_power();
    void bsp_display_brightness_init();
    void bsp_display_brightness_set(int brightness_percent);
    void lcd_draw_bitmap(uint16_t x_start, uint16_t y_start,
                         uint16_t x_end, uint16_t y_end, uint16_t *color_data);
    void draw16bitbergbbitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *color_data);
    void fillScreen(uint16_t color);
    void te_on();
    void te_off();
    uint16_t width();
    uint16_t height();

    esp_lcd_panel_handle_t get_panel_handle() { return panel_handle; }
    esp_lcd_panel_io_handle_t get_io_handle() { return io_handle; }

private:
    int8_t _lcd_rst;
    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_panel_io_handle_t io_handle;
};

#endif
