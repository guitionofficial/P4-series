#ifndef _ENTHERNET_H
#define _ENTHERNET_H

#include "esp_err.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "c" {
#endif

extern lv_obj_t *label;
extern lv_obj_t *wifi_panel;   /* 右侧 WiFi 列表容器（黑底面板） */
void wifi_panel_show(bool scan_ok, uint16_t ap_num, const void *ap_records);

void test_enthernet_init(void);
void get_enthernet_Result(void);

#ifdef __cplusplus
}
#endif

#endif