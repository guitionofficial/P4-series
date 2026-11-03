/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_codec_dev.h"

#define NUM_WAVE_BARS       32              /* 音浪竖条数量 */

/* ===== 全部使用百分比布局，以 min(sw,sh) 为基准 ref，横竖屏通用 ===== */

/* 基准参考: ref / DIV */
#define REF_DIV             1000            /* ref = min(sw,sh), 所有尺寸 = ref * 分子 / REF_DIV */

/* --- 背景装饰 --- */
#define DECO_C1_RATIO       650             /* 外圈直径 = ref * 650/1000 */
#define DECO_C2_RATIO       400             /* 内圈直径 = ref * 400/1000 */
#define DECO_C1_OFFSET_X    125             /* 外圈 X 偏移 = sw * -125/1000 */
#define DECO_C1_OFFSET_Y    100             /* 外圈 Y 偏移 = sh * 100/1000 */
#define DECO_C2_OFFSET_X    143             /* 内圈 X 偏移 = sw * 143/1000 */
#define DECO_C2_OFFSET_Y    125             /* 内圈 Y 偏移 = sh * -125/1000 */
#define DECO_LINE_W_RATIO   550             /* 上横线宽度 = sw * 550/1000 */
#define DECO_LINE_Y_TOP     180             /* 上横线 Y  = sh * 180/1000 */
#define DECO_LINE_W2_RATIO  450             /* 下横线宽度 = sw * 450/1000 */
#define DECO_LINE_Y_BOT     200             /* 下横线 Y  = sh * -200/1000 (from bottom) */

/* --- 标题区 --- */
#define TITLE_Y_RATIO       100             /* 标题 Y  = sh * 100/1000 */
#define SUBTITLE_Y_ADD      42              /* 副标题 Y 间距 (基于 ref) = ref * 42/1000 */
#define TITLE_FONT_H        100             /* 标题字体 (始终选中最大内置字体 montserrat_30) */
#define SUBTITLE_FONT_H     28              /* 副标题字体高度 = ref * 28/1000 */

/* --- 状态行 --- */
#define STATUS_ROW_Y_RATIO  250             /* 状态行 Y  = sh * 250/1000 */
#define STATUS_ROW_W_RATIO  270             /* 状态行宽度 = sw * 270/1000 */
#define STATUS_ROW_H_RATIO  28              /* 状态行高度 = ref * 28/1000 */
#define STATUS_DOT_RATIO    12              /* 状态点尺寸 = ref * 12/1000 */
#define STATUS_FONT_H       28              /* 状态字体高度 = ref * 28/1000 */
#define STATUS_LEVEL_FONT_H 30              /* 电平数字字体高度 = ref * 30/1000 */

/* --- 波形容器 --- */
#define WAVE_W_RATIO        900             /* 波形容器宽度 = sw * 900/1000 (更宽) */
#define WAVE_H_RATIO        350             /* 波形容器高度 = sh * 350/1000 */
#define WAVE_GAP_RATIO      5               /* 竖条间距 = ref * 5/1000 */
#define WAVE_RADIUS_RATIO   20              /* 容器圆角 = ref * 20/1000 */
#define WAVE_BAR_MIN_H      3               /* 竖条最低高度 (绝对像素保持可见) */
#define WAVE_SMOOTH_FACTOR  0.40f           /* 平滑系数 */

/* --- 按钮 --- */
#define BTN_SIZE_DIV        7               /* 按钮直径 = ref / 7 */
#define BTN_Y_RATIO         100             /* 按钮 Y = sh * 100/1000 (from bottom = -100) */
#define BTN_SHADOW_DIV      5               /* 阴影 = 按钮/5 */
#define BTN_SHADOW_OFS_DIV  12              /* 阴影偏移 = 按钮/12 */
#define BTN_ICON_FONT_H     44              /* 按钮图标字体 = ref * 44/1000 */

/* --- 底部提示 --- */
#define HINT_Y_RATIO        240             /* 提示 Y = sh * 240/1000 (from bottom = -240) */
#define HINT_FONT_H         18              /* 提示字体 = ref * 18/1000 */

/* ============================================================ */

class NewApp: public ESP_Brookesia_PhoneApp {
public:
    NewApp(int w, int h);
    ~NewApp();

    bool run(void);
    bool pause(void);
    bool resume(void);
    bool back(void);
    bool close(void);

    bool init(void) override;
    bool isRecording() const;
    void clearTaskHandle();

private:
    static void onScreenBtnClick(lv_event_t *e);
    void updateWaveBars(void);

    esp_err_t start_echo(void);
    esp_err_t stop_echo(void);

    /* ---- 音浪UI ---- */
    static void onWaveTimer(lv_timer_t *timer);
    static void onDotPulseAnim(void *obj, int32_t v);
    void startDotPulse(void);
    void stopDotPulse(void);

    lv_obj_t *_screen;
    lv_obj_t *_start_btn;
    lv_obj_t *_status_label;
    lv_obj_t *_status_dot;
    lv_obj_t *_subtitle_label;
    lv_obj_t *_level_label;

    lv_anim_t _dot_anim;

    /* 屏幕尺寸 + 基准 */
    int _sw, _sh;               /* 屏幕宽高 */
    int _ref;                   /* min(_sw, _sh) — 百分比基准 */
    int _bar_max_h;             /* 竖条最大高度 */

    /* 音浪组件 */
    lv_obj_t *_wave_container;
    lv_obj_t *_wave_bars[NUM_WAVE_BARS];
    lv_obj_t *_wave_glows[NUM_WAVE_BARS];
    lv_obj_t *_wave_guides[3];
    lv_timer_t *_wave_timer;
    volatile uint8_t _wave_raw[NUM_WAVE_BARS];
    float _wave_smoothed[NUM_WAVE_BARS];

    TaskHandle_t _echo_task_handle;
    bool _is_recording;
};
