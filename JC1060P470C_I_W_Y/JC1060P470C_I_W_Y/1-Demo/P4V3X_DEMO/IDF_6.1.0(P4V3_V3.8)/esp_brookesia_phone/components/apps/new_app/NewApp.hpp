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

#define NUM_WAVE_BARS       32      /* 音浪竖条数量 */
#define WAVE_BAR_MIN_H      3       /* 竖条最低高度 */
#define WAVE_BAR_GAP        6       /* 竖条间距 */
#define WAVE_SMOOTH_FACTOR  0.60f   /* 平滑系数（增大使音浪更灵敏） */

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
    static void onVolumeSliderChange(lv_event_t *e);
    static void onLevelMeterTimer(lv_timer_t *timer);
    void updateLevelMeter(int16_t peak);

    esp_err_t setup_audio(void);
    esp_err_t start_echo(void);
    esp_err_t stop_echo(void);

    /* ---- 音浪UI ---- */
    static void onWaveTimer(lv_timer_t *timer);
    static void onDotPulseAnim(void *obj, int32_t v);
    void updateWaveBars(void);
    void startDotPulse(void);
    void stopDotPulse(void);

    lv_obj_t *_screen;
    lv_obj_t *_start_btn;
    lv_obj_t *_stop_btn;
    lv_obj_t *_status_label;
    lv_obj_t *_status_dot;
    lv_obj_t *_subtitle_label;
    lv_obj_t *_level_label;           /* 音量电平数字 */
    lv_obj_t *_bg_glow;               /* 背景光晕 */
    lv_obj_t *_level_bar;
    lv_obj_t *_level_indicator;
    lv_obj_t *_volume_slider;
    lv_obj_t *_volume_label;

    lv_timer_t *_level_timer;
    lv_anim_t _dot_anim;

    /* 屏幕尺寸 */
    int _sw, _sh;
    int _bar_max_h;             /* 竖条最大高度 (按屏幕比例计算) */

    /* 音浪组件 */
    lv_obj_t *_wave_container;              /* 波形区域容器 */
    lv_obj_t *_wave_bars[NUM_WAVE_BARS];    /* 竖条 */
    lv_obj_t *_wave_glows[NUM_WAVE_BARS];   /* 发光底层 */
    lv_obj_t *_wave_guides[3];              /* 水平参考线 */
    lv_timer_t *_wave_timer;                /* 刷新定时器 */
    volatile uint8_t _wave_raw[NUM_WAVE_BARS];    /* echo task 写入的原始峰值 */
    float _wave_smoothed[NUM_WAVE_BARS];          /* 平滑后的显示值 */

    TaskHandle_t _echo_task_handle;
    bool _is_recording;
    volatile int _peak_level;
};