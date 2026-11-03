/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "driver/gpio.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_jd9365.h"
#include "esp_lcd_jd9165.h"
#include "esp_lcd_st7701.h"
#include "app_lcd.h"

static const char *TAG = "app_lcd";

#define EXAMPLE_LDO_MIPI_CHAN               (3)
#define EXAMPLE_LDO_MIPI_VOLTAGE_MV         (2500)

#define EXAMPLE_PIN_NUM_LCD_RST             (27)

static esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
static esp_lcd_panel_io_handle_t mipi_dbi_io;
static esp_lcd_panel_handle_t display_handle;


#if CONFIG_BOARD_TYPE_JC8012P4A1

static const jd9365_lcd_init_cmd_t lcd_cmd[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0xE0, (uint8_t[]){0x00}, 1, 0},
    {0xE1, (uint8_t[]){0x93}, 1, 0},
    {0xE2, (uint8_t[]){0x65}, 1, 0},
    {0xE3, (uint8_t[]){0xF8}, 1, 0},
    {0x80, (uint8_t[]){0x01}, 1, 0},

    {0xE0, (uint8_t[]){0x01}, 1, 0},
    {0x00, (uint8_t[]){0x00}, 1, 0},
    {0x01, (uint8_t[]){0x39}, 1, 0},
    {0x03, (uint8_t[]){0x10}, 1, 0},
    {0x04, (uint8_t[]){0x41}, 1, 0},

    {0x0C, (uint8_t[]){0x74}, 1, 0},
    {0x17, (uint8_t[]){0x00}, 1, 0},
    {0x18, (uint8_t[]){0xD7}, 1, 0},
    {0x19, (uint8_t[]){0x00}, 1, 0},
    {0x1A, (uint8_t[]){0x00}, 1, 0},

    {0x1B, (uint8_t[]){0xD7}, 1, 0},
    {0x1C, (uint8_t[]){0x00}, 1, 0},
    {0x24, (uint8_t[]){0xFE}, 1, 0},
    {0x35, (uint8_t[]){0x26}, 1, 0},
    {0x37, (uint8_t[]){0x69}, 1, 0},

    {0x38, (uint8_t[]){0x05}, 1, 0},
    {0x39, (uint8_t[]){0x06}, 1, 0},
    {0x3A, (uint8_t[]){0x08}, 1, 0},
    {0x3C, (uint8_t[]){0x78}, 1, 0},
    {0x3D, (uint8_t[]){0xFF}, 1, 0},

    {0x3E, (uint8_t[]){0xFF}, 1, 0},
    {0x3F, (uint8_t[]){0xFF}, 1, 0},
    {0x40, (uint8_t[]){0x06}, 1, 0},
    {0x41, (uint8_t[]){0xA0}, 1, 0},
    {0x43, (uint8_t[]){0x14}, 1, 0},

    {0x44, (uint8_t[]){0x0B}, 1, 0},
    {0x45, (uint8_t[]){0x30}, 1, 0},
    //{0x4A, (uint8_t[]){0x35}, 1, 0},//bist
    {0x4B, (uint8_t[]){0x04}, 1, 0},
    {0x55, (uint8_t[]){0x02}, 1, 0},
    {0x57, (uint8_t[]){0x89}, 1, 0},

    {0x59, (uint8_t[]){0x0A}, 1, 0},
    {0x5A, (uint8_t[]){0x28}, 1, 0},

    {0x5B, (uint8_t[]){0x15}, 1, 0},
    {0x5D, (uint8_t[]){0x50}, 1, 0},
    {0x5E, (uint8_t[]){0x37}, 1, 0},
    {0x5F, (uint8_t[]){0x29}, 1, 0},
    {0x60, (uint8_t[]){0x1E}, 1, 0},

    {0x61, (uint8_t[]){0x1D}, 1, 0},
    {0x62, (uint8_t[]){0x12}, 1, 0},
    {0x63, (uint8_t[]){0x1A}, 1, 0},
    {0x64, (uint8_t[]){0x08}, 1, 0},
    {0x65, (uint8_t[]){0x25}, 1, 0},

    {0x66, (uint8_t[]){0x26}, 1, 0},
    {0x67, (uint8_t[]){0x28}, 1, 0},
    {0x68, (uint8_t[]){0x49}, 1, 0},
    {0x69, (uint8_t[]){0x3A}, 1, 0},
    {0x6A, (uint8_t[]){0x43}, 1, 0},

    {0x6B, (uint8_t[]){0x3A}, 1, 0},
    {0x6C, (uint8_t[]){0x3B}, 1, 0},
    {0x6D, (uint8_t[]){0x32}, 1, 0},
    {0x6E, (uint8_t[]){0x1F}, 1, 0},
    {0x6F, (uint8_t[]){0x0E}, 1, 0},

    {0x70, (uint8_t[]){0x50}, 1, 0},
    {0x71, (uint8_t[]){0x37}, 1, 0},
    {0x72, (uint8_t[]){0x29}, 1, 0},
    {0x73, (uint8_t[]){0x1E}, 1, 0},
    {0x74, (uint8_t[]){0x1D}, 1, 0},

    {0x75, (uint8_t[]){0x12}, 1, 0},
    {0x76, (uint8_t[]){0x1A}, 1, 0},
    {0x77, (uint8_t[]){0x08}, 1, 0},
    {0x78, (uint8_t[]){0x25}, 1, 0},
    {0x79, (uint8_t[]){0x26}, 1, 0},

    {0x7A, (uint8_t[]){0x28}, 1, 0},
    {0x7B, (uint8_t[]){0x49}, 1, 0},
    {0x7C, (uint8_t[]){0x3A}, 1, 0},
    {0x7D, (uint8_t[]){0x43}, 1, 0},
    {0x7E, (uint8_t[]){0x3A}, 1, 0},

    {0x7F, (uint8_t[]){0x3B}, 1, 0},
    {0x80, (uint8_t[]){0x32}, 1, 0},
    {0x81, (uint8_t[]){0x1F}, 1, 0},
    {0x82, (uint8_t[]){0x0E}, 1, 0},
    {0xE0,(uint8_t []){0x02},1,0},

    {0x00,(uint8_t []){0x1F},1,0},
    {0x01,(uint8_t []){0x1F},1,0},
    {0x02,(uint8_t []){0x52},1,0},
    {0x03,(uint8_t []){0x51},1,0},
    {0x04,(uint8_t []){0x50},1,0},

    {0x05,(uint8_t []){0x4B},1,0},
    {0x06,(uint8_t []){0x4A},1,0},
    {0x07,(uint8_t []){0x49},1,0},
    {0x08,(uint8_t []){0x48},1,0},
    {0x09,(uint8_t []){0x47},1,0},

    {0x0A,(uint8_t []){0x46},1,0},
    {0x0B,(uint8_t []){0x45},1,0},
    {0x0C,(uint8_t []){0x44},1,0},
    {0x0D,(uint8_t []){0x40},1,0},
    {0x0E,(uint8_t []){0x41},1,0},

    {0x0F,(uint8_t []){0x1F},1,0},
    {0x10,(uint8_t []){0x1F},1,0},
    {0x11,(uint8_t []){0x1F},1,0},
    {0x12,(uint8_t []){0x1F},1,0},
    {0x13,(uint8_t []){0x1F},1,0},

    {0x14,(uint8_t []){0x1F},1,0},
    {0x15,(uint8_t []){0x1F},1,0},
    {0x16,(uint8_t []){0x1F},1,0},
    {0x17,(uint8_t []){0x1F},1,0},
    {0x18,(uint8_t []){0x52},1,0},

    {0x19,(uint8_t []){0x51},1,0},
    {0x1A,(uint8_t []){0x50},1,0},
    {0x1B,(uint8_t []){0x4B},1,0},
    {0x1C,(uint8_t []){0x4A},1,0},
    {0x1D,(uint8_t []){0x49},1,0},

    {0x1E,(uint8_t []){0x48},1,0},
    {0x1F,(uint8_t []){0x47},1,0},
    {0x20,(uint8_t []){0x46},1,0},
    {0x21,(uint8_t []){0x45},1,0},
    {0x22,(uint8_t []){0x44},1,0},

    {0x23,(uint8_t []){0x40},1,0},
    {0x24,(uint8_t []){0x41},1,0},
    {0x25,(uint8_t []){0x1F},1,0},
    {0x26,(uint8_t []){0x1F},1,0},
    {0x27,(uint8_t []){0x1F},1,0},

    {0x28,(uint8_t []){0x1F},1,0},
    {0x29,(uint8_t []){0x1F},1,0},
    {0x2A,(uint8_t []){0x1F},1,0},
    {0x2B,(uint8_t []){0x1F},1,0},
    {0x2C,(uint8_t []){0x1F},1,0},

    {0x2D,(uint8_t []){0x1F},1,0},
    {0x2E,(uint8_t []){0x52},1,0},
    {0x2F,(uint8_t []){0x40},1,0},
    {0x30,(uint8_t []){0x41},1,0},
    {0x31,(uint8_t []){0x48},1,0},

    {0x32,(uint8_t []){0x49},1,0},
    {0x33,(uint8_t []){0x4A},1,0},
    {0x34,(uint8_t []){0x4B},1,0},
    {0x35,(uint8_t []){0x44},1,0},
    {0x36,(uint8_t []){0x45},1,0},

    {0x37,(uint8_t []){0x46},1,0},
    {0x38,(uint8_t []){0x47},1,0},
    {0x39,(uint8_t []){0x51},1,0},
    {0x3A,(uint8_t []){0x50},1,0},
    {0x3B,(uint8_t []){0x1F},1,0},

    {0x3C,(uint8_t []){0x1F},1,0},
    {0x3D,(uint8_t []){0x1F},1,0},
    {0x3E,(uint8_t []){0x1F},1,0},
    {0x3F,(uint8_t []){0x1F},1,0},
    {0x40,(uint8_t []){0x1F},1,0},

    {0x41,(uint8_t []){0x1F},1,0},
    {0x42,(uint8_t []){0x1F},1,0},
    {0x43,(uint8_t []){0x1F},1,0},
    {0x44,(uint8_t []){0x52},1,0},
    {0x45,(uint8_t []){0x40},1,0},

    {0x46,(uint8_t []){0x41},1,0},
    {0x47,(uint8_t []){0x48},1,0},
    {0x48,(uint8_t []){0x49},1,0},
    {0x49,(uint8_t []){0x4A},1,0},
    {0x4A,(uint8_t []){0x4B},1,0},

    {0x4B,(uint8_t []){0x44},1,0},
    {0x4C,(uint8_t []){0x45},1,0},
    {0x4D,(uint8_t []){0x46},1,0},
    {0x4E,(uint8_t []){0x47},1,0},
    {0x4F,(uint8_t []){0x51},1,0},

    {0x50,(uint8_t []){0x50},1,0},
    {0x51,(uint8_t []){0x1F},1,0},
    {0x52,(uint8_t []){0x1F},1,0},
    {0x53,(uint8_t []){0x1F},1,0},
    {0x54,(uint8_t []){0x1F},1,0},

    {0x55,(uint8_t []){0x1F},1,0},
    {0x56,(uint8_t []){0x1F},1,0},
    {0x57,(uint8_t []){0x1F},1,0},
    {0x58,(uint8_t []){0x40},1,0},
    {0x59,(uint8_t []){0x00},1,0},

    {0x5A,(uint8_t []){0x00},1,0},
    {0x5B,(uint8_t []){0x10},1,0},
    {0x5C,(uint8_t []){0x05},1,0},
    {0x5D,(uint8_t []){0x50},1,0},
    {0x5E,(uint8_t []){0x01},1,0},

    {0x5F,(uint8_t []){0x02},1,0},
    {0x60,(uint8_t []){0x50},1,0},
    {0x61,(uint8_t []){0x06},1,0},
    {0x62,(uint8_t []){0x04},1,0},
    {0x63,(uint8_t []){0x03},1,0},

    {0x64,(uint8_t []){0x64},1,0},
    {0x65,(uint8_t []){0x65},1,0},
    {0x66,(uint8_t []){0x0B},1,0},
    {0x67,(uint8_t []){0x73},1,0},
    {0x68,(uint8_t []){0x07},1,0},

    {0x69,(uint8_t []){0x06},1,0},
    {0x6A,(uint8_t []){0x64},1,0},
    {0x6B,(uint8_t []){0x08},1,0},
    {0x6C,(uint8_t []){0x00},1,0},
    {0x6D,(uint8_t []){0x32},1,0},

    {0x6E,(uint8_t []){0x08},1,0},
    {0xE0,(uint8_t []){0x04},1,0},
    {0x2C,(uint8_t []){0x6B},1,0},
    {0x35,(uint8_t []){0x08},1,0},
    {0x37,(uint8_t []){0x00},1,0},

    {0xE0,(uint8_t []){0x00},1,0},
    {0x11,(uint8_t []){0x00},1,0},
    {0x29, (uint8_t[]){0x00}, 1, 5},
    {0x11, (uint8_t[]){0x00}, 1, 120},
    {0x35, (uint8_t[]){0x00}, 1, 0},
};
#elif CONFIG_BOARD_TYPE_JC8012P4A1
static const st7701_lcd_init_cmd_t lcd_cmd[] = {
    {0xFF, (uint8_t []){0x77,0x01,0x00,0x00,0x13},5,0},
    {0xEF, (uint8_t []){0x08}, 1, 0},
    {0xFF, (uint8_t []){0x77,0x01,0x00,0x00,0x10},5,0},
    {0xC0, (uint8_t []){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t []){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t []){0x10, 0x08}, 2, 0},
    {0xCC, (uint8_t []){0x10}, 1, 0},

    {0xB0, (uint8_t []){0x80, 0x09, 0x53, 0x0C, 0xD0, 0x07, 0x0C, 0x09, 0x09, 0x28, 0x06, 0xD4, 0x13, 0x69, 0x2B, 0x71}, 16, 0},
    {0xB1, (uint8_t []){0x80, 0x94, 0x5A, 0x10, 0xD3, 0x06, 0x0A, 0x08, 0x08, 0x25, 0x03, 0xD3, 0x12, 0x66, 0x6A, 0x0D}, 16, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},

    {0xB0, (uint8_t []){0x5D}, 1, 0},
    {0xB1, (uint8_t []){0x58}, 1, 0},
    {0xB2, (uint8_t []){0x87}, 1, 0},
    {0xB3, (uint8_t []){0x80}, 1, 0},
    {0xB5, (uint8_t []){0x4E}, 1, 0},
    {0xB7, (uint8_t []){0x85}, 1, 0},
    {0xB8, (uint8_t []){0x21}, 1, 0},
    {0xB9, (uint8_t []){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t []){0x03}, 1,0},
    {0xBC, (uint8_t []){0x00}, 1,0},
    
    {0xC1, (uint8_t []){0x78}, 1, 0},
    {0xC2, (uint8_t []){0x78}, 1, 0},
    {0xD0, (uint8_t []){0x88}, 1, 0},

    {0xE0, (uint8_t []){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t []){0x04, 0xA0, 0x00, 0xA0, 0x05,0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t []){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t []){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t []){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t []){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t []){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},

    {0xEB, (uint8_t []){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t []){0x08, 0x01}, 2, 0},

    {0xED, (uint8_t []){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t []){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},

    // {0x3A, (uint8_t []){0x66}, 1, 0},
    {0x11, (uint8_t []){0x00}, 1, 120},
    {0x29, (uint8_t []){0x00}, 1, 20},

};
#elif CONFIG_BOARD_TYPE_JC1060P470
static const jd9165_lcd_init_cmd_t lcd_cmd[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0x30, (uint8_t[]){0x00}, 1, 0},
    {0xF7, (uint8_t[]){0x49,0x61,0x02,0x00}, 4, 0},
    {0x30, (uint8_t[]){0x01}, 1, 0},
    {0x04, (uint8_t[]){0x0C}, 1, 0},
    {0x05, (uint8_t[]){0x00}, 1, 0},
    {0x06, (uint8_t[]){0x00}, 1, 0},
    {0x0B, (uint8_t[]){0x11}, 1, 0},
    {0x17, (uint8_t[]){0x00}, 1, 0},
    {0x20, (uint8_t[]){0x04}, 1, 0},
    {0x1F, (uint8_t[]){0x05}, 1, 0},
    {0x23, (uint8_t[]){0x00}, 1, 0},
    {0x25, (uint8_t[]){0x19}, 1, 0},
    {0x28, (uint8_t[]){0x18}, 1, 0},
    {0x29, (uint8_t[]){0x04}, 1, 0},
    {0x2A, (uint8_t[]){0x01}, 1, 0},
    {0x2B, (uint8_t[]){0x04}, 1, 0},
    {0x2C, (uint8_t[]){0x01}, 1, 0},
    {0x30, (uint8_t[]){0x02}, 1, 0},
    {0x01, (uint8_t[]){0x22}, 1, 0},
    {0x03, (uint8_t[]){0x12}, 1, 0},
    {0x04, (uint8_t[]){0x00}, 1, 0},
    {0x05, (uint8_t[]){0x64}, 1, 0},
    {0x0A, (uint8_t[]){0x08}, 1, 0},
    {0x0B, (uint8_t[]){0x0A,0x1A,0x0B,0x0D,0x0D,0x11,0x10,0x06,0x08,0x1F,0x1D}, 11, 0},
    {0x0C, (uint8_t[]){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}, 11, 0},
    {0x0D, (uint8_t[]){0x16,0x1B,0x0B,0x0D,0x0D,0x11,0x10,0x07,0x09,0x1E,0x1C}, 11, 0},
    {0x0E, (uint8_t[]){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}, 11, 0},
    {0x0F, (uint8_t[]){0x16,0x1B,0x0D,0x0B,0x0D,0x11,0x10,0x1C,0x1E,0x09,0x07}, 11, 0},
    {0x10, (uint8_t[]){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}, 11, 0},
    {0x11, (uint8_t[]){0x0A,0x1A,0x0D,0x0B,0x0D,0x11,0x10,0x1D,0x1F,0x08,0x06}, 11, 0},
    {0x12, (uint8_t[]){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}, 11, 0},
    {0x14, (uint8_t[]){0x00,0x00,0x11,0x11}, 4, 0},
    {0x18, (uint8_t[]){0x99}, 1, 0},
    {0x30, (uint8_t[]){0x06}, 1, 0},
    {0x12, (uint8_t[]){0x36,0x2C,0x2E,0x3C,0x38,0x35,0x35,0x32,0x2E,0x1D,0x2B,0x21,0x16,0x29}, 14, 0},
    {0x13, (uint8_t[]){0x36,0x2C,0x2E,0x3C,0x38,0x35,0x35,0x32,0x2E,0x1D,0x2B,0x21,0x16,0x29}, 14, 0},
    
    // {0x30, (uint8_t[]){0x08}, 1, 0},
    // {0x05, (uint8_t[]){0x01}, 1, 0},
    // {0x0C, (uint8_t[]){0x1A}, 1, 0},
    // {0x0D, (uint8_t[]){0x0E}, 1, 0},

    // {0x30, (uint8_t[]){0x07}, 1, 0},
    // {0x01, (uint8_t[]){0x04}, 1, 0},

    {0x30, (uint8_t[]){0x0A}, 1, 0},
    {0x02, (uint8_t[]){0x4F}, 1, 0},
    {0x0B, (uint8_t[]){0x40}, 1, 0},
    {0x12, (uint8_t[]){0x3E}, 1, 0},
    {0x13, (uint8_t[]){0x78}, 1, 0},
    {0x30, (uint8_t[]){0x0D}, 1, 0},
    {0x0D, (uint8_t[]){0x04}, 1, 0},
    {0x10, (uint8_t[]){0x0C}, 1, 0},
    {0x11, (uint8_t[]){0x0C}, 1, 0},
    {0x12, (uint8_t[]){0x0C}, 1, 0},
    {0x13, (uint8_t[]){0x0C}, 1, 0},
    {0x30, (uint8_t[]){0x00}, 1, 0},

    {0X3A, (uint8_t[]){0x55}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 1, 120},
    {0x29, (uint8_t[]){0x00}, 1, 20},
};

#endif

static void lcd_ldo_power_on(void)
{
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = EXAMPLE_LDO_MIPI_CHAN,
        .voltage_mv = EXAMPLE_LDO_MIPI_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));
}

esp_err_t app_lcd_init(esp_lcd_panel_handle_t *panel_handle)
{
    lcd_ldo_power_on();

    ESP_LOGD(TAG, "Install LCD driver");
    // esp_lcd_dsi_bus_config_t bus_config = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
#if CONFIG_BOARD_TYPE_JC8012P4A1
    esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
#elif CONFIG_BOARD_TYPE_JC4880P443
    esp_lcd_dsi_bus_config_t bus_config = ST7701_PANEL_BUS_DSI_2CH_CONFIG();
#elif CONFIG_BOARD_TYPE_JC1060P470
    esp_lcd_dsi_bus_config_t bus_config = JD9165_PANEL_BUS_DSI_2CH_CONFIG();
#endif

    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install panel IO");

    esp_lcd_dbi_io_config_t dbi_config =  {                             
        .virtual_channel = 0,         
        .lcd_cmd_bits = 8,            
        .lcd_param_bits = 8,         
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    
#if CONFIG_BOARD_TYPE_JC8012P4A1
    ESP_LOGI(TAG, "Install LCD driver of JD9365");
    esp_lcd_dpi_panel_config_t dpi_config = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,     
        .dpi_clock_freq_mhz = 60,                        
        .virtual_channel = 0,                            
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,                       
        .num_fbs = EXAMPLE_LCD_BUF_NUM,                                    
        .video_timing = {                                
            .h_size = 800,                               
            .v_size = 1280,                              
            .hsync_back_porch = 20,                      
            .hsync_pulse_width = 20,                     
            .hsync_front_porch = 40,                     
            .vsync_back_porch = 8,                      
            .vsync_pulse_width = 4,                      
            .vsync_front_porch = 20,                     
        },                                               
        .flags.use_dma2d = true,                         
    };
#elif CONFIG_BOARD_TYPE_JC4880P443
    ESP_LOGI(TAG, "Install LCD driver of st7701");
    esp_lcd_dpi_panel_config_t dpi_config ={
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,  
        .dpi_clock_freq_mhz = 34,                     
        .virtual_channel = 0,                         
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,                    
        .num_fbs = EXAMPLE_LCD_BUF_NUM,                                 
        .video_timing = {                             
            .h_size = 480,                            
            .v_size = 800,                            
            .hsync_back_porch = 42,                   
            .hsync_pulse_width = 12,                  
            .hsync_front_porch = 42,                  
            .vsync_back_porch = 8,                      
            .vsync_pulse_width = 2,                     
            .vsync_front_porch = 166,                  
        },                                            
        .flags.use_dma2d = true,                      
    };

#elif    CONFIG_BOARD_TYPE_JC1060P470
    esp_lcd_dpi_panel_config_t dpi_config ={
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,  
        .dpi_clock_freq_mhz = 52,                     
        .virtual_channel = 0,                         
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,                    
        .num_fbs = EXAMPLE_LCD_BUF_NUM,                                 
        .video_timing = {                             
            .h_size = 1024,                            
            .v_size = 600,                            
            .hsync_back_porch = 160,                   
            .hsync_pulse_width = 20,                  
            .hsync_front_porch = 160,                  
            .vsync_back_porch = 23,                      
            .vsync_pulse_width = 10,                     
            .vsync_front_porch = 12,                  
        },                                            
        .flags.use_dma2d = true,                      
    };

#endif


#if CONFIG_BOARD_TYPE_JC8012P4A1
    jd9365_vendor_config_t vendor_config = {
        .init_cmds = lcd_cmd,
        .init_cmds_size = sizeof(lcd_cmd) / sizeof(jd9365_lcd_init_cmd_t),
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    // ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(mipi_dbi_io, &panel_config, &display_handle));
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9365(mipi_dbi_io, &panel_config, &display_handle));
#elif CONFIG_BOARD_TYPE_JC4880P443
    st7701_vendor_config_t vendor_config = {
        .init_cmds = lcd_cmd,
        .init_cmds_size = sizeof(lcd_cmd) / sizeof(st7701_lcd_init_cmd_t),
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
        .flags = {
            .use_mipi_interface = 1,
        }
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_5,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(mipi_dbi_io, &panel_config, &display_handle));
#elif CONFIG_BOARD_TYPE_JC1060P470
    jd9165_vendor_config_t vendor_config = {
        .init_cmds = lcd_cmd,
        .init_cmds_size = sizeof(lcd_cmd) / sizeof(jd9165_lcd_init_cmd_t),
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_5,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9165(mipi_dbi_io, &panel_config, &display_handle));
#endif
    ESP_ERROR_CHECK(esp_lcd_panel_reset(display_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(display_handle));

    *panel_handle = display_handle;

    return ESP_OK;
}
