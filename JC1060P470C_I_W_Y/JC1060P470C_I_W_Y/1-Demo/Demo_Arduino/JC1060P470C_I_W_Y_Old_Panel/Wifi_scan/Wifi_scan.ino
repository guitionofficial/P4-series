#pragma GCC push_options
#pragma GCC optimize("O3")

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "WiFi.h"
#include "driver/i2c_master.h"
#include "demos/lv_demos.h"
#include "pins_config.h"
#include "src/lcd/jd9165_lcd.h"
#include "src/touch/gt911_touch.h"

bsp_lcd_handles_t lcd_panels;

jd9165_lcd lcd = jd9165_lcd(LCD_RST);
gt911_touch touch = gt911_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;
static lv_color_t *buf1;

LV_FONT_DECLARE(weiruanyahei_14);

lv_obj_t *obj = NULL;
lv_obj_t *label[25];

static bool lvgl_port_flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
    assert(disp_drv != NULL);
    // lv_disp_flush_ready(disp_drv);
    lv_disp_flush_ready(disp_drv);

    // if (disp_ctx->trans_size && disp_ctx->trans_sem) {
    //     xSemaphoreGiveFromISR(disp_ctx->trans_sem, &taskAwake);
    // }

    return false;
}

// 显示刷新
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  const int offsetx1 = area->x1;
  const int offsetx2 = area->x2;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;
  lcd.lcd_draw_bitmap(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, &color_p->full);
  // lv_disp_flush_ready(disp); // 告诉lvgl刷新完成
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  bool touched;
  uint16_t touchX, touchY;

  touched = touch.getTouch(&touchX, &touchY);
  // touchX = 800 - touchX;

  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;

    // 设置坐标
    data->point.x = touchX;
    data->point.y = touchY;
    Serial.printf("x=%d,y=%d \r\n",touchX,touchY);
  }
}

static void lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        touch.set_rotation(0);
        break;
    case LV_DISP_ROT_90:
        touch.set_rotation(1);
        break;
    case LV_DISP_ROT_180:
        touch.set_rotation(2);
        break;
    case LV_DISP_ROT_270:
        touch.set_rotation(3);
        break;
    }
}

static void wifiScanTask(void *arg)
{
  while(1)
  {
    printf("Scan start\r\n");

    // WiFi.scanNetworks will return the number of networks found.
    int n = WiFi.scanNetworks();
    printf("Scan done\r\n");
    if(n > 25)
      n=25;
    if (n == 0) {
      printf("no networks found\r\n");
    } else {
      printf("%d",n);
      printf(" networks found\r\n");
      printf("Nr | SSID                             | RSSI | CH | Encryption\r\n");
      for (int i = 0; i < n; ++i) {
        // Print SSID and RSSI for each network found
        printf("%2d", i + 1);
        printf(" | ");
        printf("%-32.32s", WiFi.SSID(i).c_str());
        printf(" | ");
        printf("%4ld", WiFi.RSSI(i));
        printf(" | ");
        printf("%2ld", WiFi.channel(i));
        printf(" | ");
        switch (WiFi.encryptionType(i)) {
          case WIFI_AUTH_OPEN:            printf("open"); break;
          case WIFI_AUTH_WEP:             printf("WEP"); break;
          case WIFI_AUTH_WPA_PSK:         printf("WPA"); break;
          case WIFI_AUTH_WPA2_PSK:        printf("WPA2"); break;
          case WIFI_AUTH_WPA_WPA2_PSK:    printf("WPA+WPA2"); break;
          case WIFI_AUTH_WPA2_ENTERPRISE: printf("WPA2-EAP"); break;
          case WIFI_AUTH_WPA3_PSK:        printf("WPA3"); break;
          case WIFI_AUTH_WPA2_WPA3_PSK:   printf("WPA2+WPA3"); break;
          case WIFI_AUTH_WAPI_PSK:        printf("WAPI"); break;
          default:                        printf("unknown");
        }
        printf("\r\n");
        lv_label_set_text_fmt(label[i],"%-32s  %4ld",WiFi.SSID(i).c_str(),WiFi.RSSI(i));
        delay(10);
      }
    }
    Serial.println("");

    // Delete the scan result to free memory for code below.
    WiFi.scanDelete();

    // Wait a bit before scanning again.
    delay(5000);
  }
}

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
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
  size_t buffer_size = sizeof(int16_t) * LCD_H_RES * LCD_V_RES;
  // buf = (int32_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  // buf1 = (int32_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  buf = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  assert(buf);
  assert(buf1);

  lv_disp_draw_buf_init(&draw_buf, buf, buf1, LCD_H_RES * LCD_V_RES);

  static lv_disp_drv_t disp_drv;
  /*Initialize the display*/
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = false;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
  cbs.on_color_trans_done = lvgl_port_flush_dpi_panel_ready_callback;
   /* Register done callback */
  esp_lcd_dpi_panel_register_event_callbacks(lcd_panels.panel, &cbs, &disp_drv);

  // lv_disp_set_rotation(NULL, 0);
  Serial.println("start demo");

  obj = lv_obj_create(lv_scr_act());
  lv_obj_set_size(obj,480,800);


  label[0] = lv_label_create(obj);
  lv_obj_align(label[0],LV_ALIGN_TOP_LEFT,0,0);
  lv_obj_set_style_text_font(label[0], &weiruanyahei_14, 0);
  for(int i =1;i<25;i++)
  {
    label[i] = lv_label_create(obj);
    lv_obj_set_style_text_font(label[i], &weiruanyahei_14, 0);
    lv_obj_align_to(label[i],label[i-1],LV_ALIGN_OUT_BOTTOM_LEFT,0,0);
  }

  xTaskCreatePinnedToCore(wifiScanTask, "WiFi Scan", 4096, NULL, 4, NULL,1);
  Serial.println("Setup done");
}

void loop() {

  lv_timer_handler();
  delay(5);
  
}