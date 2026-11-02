#pragma GCC push_options
#pragma GCC optimize("O3")

#include <Arduino.h>
#include "lvgl.h"
#include "driver/i2c_master.h"
#include "demos/lv_demos.h"
#include "pins_config.h"
#include "src/lcd/jd9365_lcd.h"
#include "src/touch/gt911_touch.h"

bsp_lcd_handles_t lcd_panels;

jd9365_lcd lcd = jd9365_lcd(LCD_RST);
gt911_touch touch = gt911_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);

static lv_display_t *lv_disp;

// DPI 面板颜色传输完成回调（每帧刷新完成后由 esp_lcd 驱动调用）
static bool lvgl_port_flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    assert(disp != NULL);
    lv_display_flush_ready(disp);

    return false;
}

// 显示刷新（LVGL v9：flush_cb 只负责把像素数据交给屏幕，等待异步传输完成后回调 flush_ready）
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  const int offsetx1 = area->x1;
  const int offsetx2 = area->x2;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;
  lcd.lcd_draw_bitmap(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, (uint16_t *)px_map);
  // lv_display_flush_ready(disp); // 异步传输，真正完成后再由上面的回调通知 LVGL
}

// 触摸读取（LVGL v9：read_cb）
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
  bool touched;
  uint16_t touchX, touchY;

  touched = touch.getTouch(&touchX, &touchY);
  // touchX = 800 - touchX;

  if (!touched)
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
  else
  {
    data->state = LV_INDEV_STATE_PRESSED;

    // 设置坐标
    data->point.x = touchX;
    data->point.y = touchY;
    // Serial.printf("x=%d,y=%d \r\n",touchX,touchY);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("ESP32P4 MIPI DSI LVGL");
  
  i2c_master_bus_handle_t i2c_handle = NULL;

  i2c_master_bus_config_t i2c_bus_conf = {
      .i2c_port = I2C_NUM_1,
      .sda_io_num = (gpio_num_t)TP_I2C_SDA,
      .scl_io_num = (gpio_num_t)TP_I2C_SCL,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .intr_priority = 0,
      .trans_queue_depth = 0,
      .flags = {
          .enable_internal_pullup = 1,
      },
  };
  i2c_new_master_bus(&i2c_bus_conf, &i2c_handle);
  
  lcd.begin();
  touch.begin();

  lcd.get_handle(&lcd_panels);
  

  lv_init();
  // LVGL v9：默认 tick 不接系统时钟，把 lv_tick_get 指向 Arduino millis()
  lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

  size_t buffer_size = sizeof(uint16_t) * LCD_H_RES * LCD_V_RES; // 一个全屏缓冲 = 800*1280*2 字节
  // buf = (int32_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  // buf1 = (int32_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  uint16_t *buf = (uint16_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  uint16_t *buf1 = (uint16_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  assert(buf);
  assert(buf1);

  // LVGL v9：创建显示设备，挂两个全屏大小的缓冲
  lv_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
  lv_display_set_buffers(lv_disp, buf, buf1, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(lv_disp, my_disp_flush);
  lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565);

  // LVGL v9：触摸输入设备
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
  cbs.on_color_trans_done = lvgl_port_flush_dpi_panel_ready_callback;
   /* Register done callback */
  esp_lcd_dpi_panel_register_event_callbacks(lcd_panels.panel, &cbs, lv_disp);

  // lv_display_set_rotation(NULL, 0);
  Serial.println("start demo");

  lv_demo_widgets(); /* 小部件示例 */
  // lv_demo_music();        /* 类似智能手机的现代音乐播放器演示 */
  // lv_demo_stress();       /* LVGL 压力测试 */
  // lv_demo_benchmark();    /* 用于测量 LVGL 性能或比较不同设置的演示 */
}

void loop()
{
  lv_timer_handler();
  delay(5);
}
