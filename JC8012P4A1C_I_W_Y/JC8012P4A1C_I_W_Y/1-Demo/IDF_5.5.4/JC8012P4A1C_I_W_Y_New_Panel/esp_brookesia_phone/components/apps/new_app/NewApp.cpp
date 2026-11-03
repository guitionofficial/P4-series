/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <cstddef>
#include <cmath>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_check.h"

#include "NewApp.hpp"
#include "example_config.h"

#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"


static const char *TAG = "Echo";

static bool echo_task_running = false;
static volatile uint8_t *g_wave_raw = nullptr;

extern const lv_img_dsc_t echo_app_round;
const lv_img_dsc_t *echo_app_icon = &echo_app_round;
static SemaphoreHandle_t echo_task_sem = NULL;

/* =================== Echo 音频任务 =================== */
void i2s_echo1(void *args)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Echo task start");
    echo_task_sem = xSemaphoreCreateBinary();
    if (echo_task_sem == NULL) {
        ESP_LOGE(TAG, "create semaphore failed");
        vTaskDelete(NULL);
        return;
    }
    bsp_extra_codec_mute_set(true);
    xSemaphoreGive(*(SemaphoreHandle_t *)args);
    xSemaphoreTake(echo_task_sem, portMAX_DELAY);
    xSemaphoreGive(echo_task_sem);
    int system_volume = bsp_extra_codec_volume_get();
    bsp_extra_codec_volume_set(system_volume, NULL);
    ESP_LOGI(TAG, "Echo using system volume: %d", system_volume);
    vTaskDelay(pdMS_TO_TICKS(50));
    bsp_extra_codec_mute_set(false);

    ESP_LOGI(TAG, "Echo audio ready, entering loop");

    int16_t *buffer = (int16_t *)malloc(EXAMPLE_RECV_BUF_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "malloc failed");
        bsp_extra_codec_mute_set(true);
        vTaskDelete(NULL);
        return;
    }

    echo_task_running = true;
    size_t sample_count = EXAMPLE_RECV_BUF_SIZE / sizeof(int16_t);
    size_t samples_per_bar = sample_count / NUM_WAVE_BARS;
    if (samples_per_bar < 1) samples_per_bar = 1;

    while (echo_task_running) {
        size_t bytes_read = 0;
        size_t bytes_written = 0;

        ret = bsp_extra_i2s_read(buffer, EXAMPLE_RECV_BUF_SIZE,
                                 &bytes_read, pdMS_TO_TICKS(100));
        if (ret != ESP_OK || bytes_read == 0) continue;

        if (g_wave_raw) {
            size_t samples = bytes_read / sizeof(int16_t);
            for (int bar = 0; bar < NUM_WAVE_BARS; bar++) {
                int32_t peak = 0;
                size_t seg_start = bar * samples_per_bar;
                size_t seg_end   = (bar == NUM_WAVE_BARS - 1)
                                   ? samples : (seg_start + samples_per_bar);
                for (size_t s = seg_start; s < seg_end; s++) {
                    int32_t val = (buffer[s] < 0)
                                  ? -(int32_t)buffer[s] : (int32_t)buffer[s];
                    if (val > peak) peak = val;
                }
                g_wave_raw[bar] = (uint8_t)((peak * 255U) / 32767U);
            }
        }

        /* 衰减回放防啸叫 */
        size_t samples = bytes_read / sizeof(int16_t);
        for (size_t i = 0; i < samples && i < sample_count; i++)
            buffer[i] = (int16_t)(buffer[i] * 8 / 10);

        bsp_extra_i2s_write(buffer, bytes_read, &bytes_written, pdMS_TO_TICKS(100));
    }

    free(buffer);
    bsp_extra_codec_mute_set(true);
    ESP_LOGI(TAG, "Echo task exit");
    vTaskDelete(NULL);
}

/* =================== 构造 / 析构 =================== */
NewApp::NewApp(int w, int h)
    : ESP_Brookesia_PhoneApp("Echo", echo_app_icon, true),
      _screen(nullptr), _start_btn(nullptr),
      _status_label(nullptr), _status_dot(nullptr),
      _subtitle_label(nullptr), _level_label(nullptr),
      _sw(w), _sh(h),
      _ref(w < h ? w : h),           /* min(sw, sh) */
      _bar_max_h(h * 30 / 100),
      _wave_container(nullptr), _wave_timer(nullptr),
      _echo_task_handle(nullptr), _is_recording(false)
{
    memset(_wave_bars,   0, sizeof(_wave_bars));
    memset(_wave_glows,  0, sizeof(_wave_glows));
    memset(_wave_guides, 0, sizeof(_wave_guides));
    memset((void*)_wave_raw,     0, sizeof(_wave_raw));
    memset(_wave_smoothed,       0, sizeof(_wave_smoothed));
    memset(&_dot_anim,           0, sizeof(_dot_anim));
}

NewApp::~NewApp()
{
    stop_echo();
    bsp_extra_codec_dev_stop();
}

/* =================== 字体自适应 =================== */
static const lv_font_t *choose_font(int ref, int ratio)
{
    int target_h = ref * ratio / REF_DIV;
    /* LVGL 内置字体高度: 10→20px, 12→22, 14→26, 16→30, 18→32, 20→36, 24→42, 30→50 */
    /* 阈值刻意压低，让中屏也选中更大字体 */
    if (target_h <= 14) return &lv_font_montserrat_10;
    if (target_h <= 18) return &lv_font_montserrat_12;
    if (target_h <= 22) return &lv_font_montserrat_14;
    if (target_h <= 26) return &lv_font_montserrat_16;
    if (target_h <= 30) return &lv_font_montserrat_18;
    if (target_h <= 36) return &lv_font_montserrat_20;
    if (target_h <= 44) return &lv_font_montserrat_24;
    return &lv_font_montserrat_30;
}

/* =================== 界面初始化 (全百分比布局) =================== */
bool NewApp::init()
{
    int sw = _sw, sh = _sh, ref = _ref;

    /* ===== 计算所有尺寸 (百分比 × 基准 / REF_DIV) ===== */
    int gap      = ref * WAVE_GAP_RATIO / REF_DIV;           /* 竖条间距 */
    int bar_w    = (sw * WAVE_W_RATIO / REF_DIV - gap * 4)
                   / NUM_WAVE_BARS - gap;                    /* 竖条宽度 */
    int glow_w   = bar_w + (ref * 6 / REF_DIV);             /* 发光底层宽 */
    int area_h   = sh * WAVE_H_RATIO / REF_DIV;              /* 波形容器高 */
    int total_w  = (bar_w + gap) * NUM_WAVE_BARS + gap * 2; /* 波形容器宽 */
    int btn_size = ref / BTN_SIZE_DIV;                       /* 按钮直径 */
    int title_y  = sh * TITLE_Y_RATIO / REF_DIV;             /* 标题 Y */
    int subtitle_y_add = ref * SUBTITLE_Y_ADD / REF_DIV;     /* 副标题间距 */

    _bar_max_h = area_h - gap * 2 - WAVE_BAR_MIN_H;
    if (_bar_max_h < 10) _bar_max_h = 10;

    /* ===== 屏幕背景 (稍深灰色，与白色波形容器拉开对比) ===== */
    _screen = lv_obj_create(NULL);
    lv_obj_set_size(_screen, sw, sh);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0xEAECEE), 0);
    lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* ===== 背景装饰圆 c1 (外圈) ===== */
    {
        int d = ref * DECO_C1_RATIO / REF_DIV;
        int ox = -(sw * DECO_C1_OFFSET_X / REF_DIV);
        int oy =  (sh * DECO_C1_OFFSET_Y / REF_DIV);
        lv_obj_t *c = lv_obj_create(_screen);
        lv_obj_set_size(c, d, d);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(0xD6E0F0), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_40, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_align(c, LV_ALIGN_CENTER, ox, oy);
    }

    /* ===== 背景装饰圆 c2 (内圈) ===== */
    {
        int d = ref * DECO_C2_RATIO / REF_DIV;
        int ox =  (sw * DECO_C2_OFFSET_X / REF_DIV);
        int oy = -(sh * DECO_C2_OFFSET_Y / REF_DIV);
        lv_obj_t *c = lv_obj_create(_screen);
        lv_obj_set_size(c, d, d);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(0xDBD5F0), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_40, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_align(c, LV_ALIGN_CENTER, ox, oy);
    }

    /* ===== 装饰横线 h1 (上方) ===== */
    {
        int lw = sw * DECO_LINE_W_RATIO / REF_DIV;
        int ly = sh * DECO_LINE_Y_TOP / REF_DIV;
        lv_obj_t *h = lv_obj_create(_screen);
        lv_obj_set_size(h, lw, 1);
        lv_obj_set_style_bg_color(h, lv_color_hex(0xD5D9DF), 0);
        lv_obj_set_style_border_width(h, 0, 0);
        lv_obj_align(h, LV_ALIGN_TOP_MID, 0, ly);
    }

    /* ===== 装饰横线 h2 (下方) ===== */
    {
        int lw = sw * DECO_LINE_W2_RATIO / REF_DIV;
        int ly = -(sh * DECO_LINE_Y_BOT / REF_DIV);
        lv_obj_t *h = lv_obj_create(_screen);
        lv_obj_set_size(h, lw, 1);
        lv_obj_set_style_bg_color(h, lv_color_hex(0xD5D9DF), 0);
        lv_obj_set_style_border_width(h, 0, 0);
        lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, ly);
    }

    /* ===== 标题 “ECHO” ===== */
    {
        lv_obj_t *title = lv_label_create(_screen);
        lv_label_set_text(title, "ECHO");
        lv_obj_set_style_text_font(title, choose_font(ref, TITLE_FONT_H), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_text_letter_space(title, ref * 10 / REF_DIV, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, title_y);
    }

    /* ===== 副标题 “Audio Visualizer” ===== */
    _subtitle_label = lv_label_create(_screen);
    lv_label_set_text(_subtitle_label, "Audio Visualizer");
    lv_obj_set_style_text_font(_subtitle_label, choose_font(ref, SUBTITLE_FONT_H), 0);
    lv_obj_set_style_text_color(_subtitle_label, lv_color_hex(0x95A5A6), 0);
    lv_obj_set_style_text_letter_space(_subtitle_label, ref * 4 / REF_DIV, 0);
    lv_obj_align(_subtitle_label, LV_ALIGN_TOP_MID, 0, title_y + subtitle_y_add);

    /* ===== 状态行 (Flex 布局: 圆点 + 状态文字 + 电平数字) ===== */
    {
        int row_y = sh * STATUS_ROW_Y_RATIO / REF_DIV;
        int row_w = sw * STATUS_ROW_W_RATIO / REF_DIV;
        int row_h = ref * STATUS_ROW_H_RATIO / REF_DIV;
        int dot_s = ref * STATUS_DOT_RATIO / REF_DIV;

        lv_obj_t *status_row = lv_obj_create(_screen);
        lv_obj_set_size(status_row, row_w, row_h);
        lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(status_row, 0, 0);
        lv_obj_set_style_pad_all(status_row, 0, 0);
        lv_obj_align(status_row, LV_ALIGN_TOP_MID, 0, row_y);
        lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        _status_dot = lv_obj_create(status_row);
        lv_obj_set_size(_status_dot, dot_s, dot_s);
        lv_obj_set_style_radius(_status_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(_status_dot, lv_color_hex(0xB0BEC5), 0);
        lv_obj_set_style_border_width(_status_dot, 0, 0);
        lv_obj_set_style_shadow_width(_status_dot, dot_s, 0);
        lv_obj_set_style_shadow_color(_status_dot, lv_color_hex(0x26C6DA), 0);
        lv_obj_set_style_shadow_opa(_status_dot, LV_OPA_0, 0);

        _status_label = lv_label_create(status_row);
        lv_label_set_text(_status_label, "Ready");
        lv_obj_set_style_text_font(_status_label, choose_font(ref, STATUS_FONT_H), 0);
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0x90A4AE), 0);
        lv_obj_set_style_pad_left(_status_label, ref * 14 / REF_DIV, 0);

        _level_label = lv_label_create(status_row);
        lv_label_set_text(_level_label, "— dBm");
        lv_obj_set_style_text_font(_level_label, choose_font(ref, STATUS_LEVEL_FONT_H), 0);
        lv_obj_set_style_text_color(_level_label, lv_color_hex(0xB0BEC5), 0);
        lv_obj_set_style_pad_left(_level_label, ref * 18 / REF_DIV, 0);
    }

    /* ===== 波形容器 (白底加强阴影与灰背景形成对比) ===== */
    _wave_container = lv_obj_create(_screen);
    lv_obj_set_size(_wave_container, total_w, area_h);
    lv_obj_set_style_bg_color(_wave_container, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(_wave_container, 2, 0);
    lv_obj_set_style_border_color(_wave_container, lv_color_hex(0xD0D5DB), 0);
    lv_obj_set_style_radius(_wave_container, ref * WAVE_RADIUS_RATIO / REF_DIV, 0);
    lv_obj_set_style_shadow_width(_wave_container, ref * 30 / REF_DIV, 0);
    lv_obj_set_style_shadow_color(_wave_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(_wave_container, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(_wave_container, ref * 8 / REF_DIV, 0);
    lv_obj_set_style_pad_all(_wave_container, gap, 0);
    lv_obj_align(_wave_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(_wave_container, LV_OBJ_FLAG_SCROLLABLE);

    /* ===== 参考线 (3 条水平) ===== */
    for (int g = 0; g < 3; g++) {
        _wave_guides[g] = lv_obj_create(_wave_container);
        lv_obj_set_size(_wave_guides[g], total_w - gap * 4, 1);
        lv_obj_set_style_bg_color(_wave_guides[g], lv_color_hex(0xECEFF1), 0);
        lv_obj_set_style_border_width(_wave_guides[g], 0, 0);
        int y_offset = (g + 1) * (_bar_max_h / 4) + gap;
        lv_obj_align(_wave_guides[g], LV_ALIGN_BOTTOM_MID, 0, -y_offset);
    }

    /* ===== 32 对 (发光底层 + 竖条) ===== */
    for (int i = 0; i < NUM_WAVE_BARS; i++) {
        int x = i * (bar_w + gap) + gap;

        _wave_glows[i] = lv_obj_create(_wave_container);
        lv_obj_set_size(_wave_glows[i], glow_w, WAVE_BAR_MIN_H);
        lv_obj_set_style_radius(_wave_glows[i], ref * 7 / REF_DIV, 0);
        lv_obj_set_style_border_width(_wave_glows[i], 0, 0);
        lv_obj_set_style_pad_all(_wave_glows[i], 0, 0);
        lv_obj_align(_wave_glows[i], LV_ALIGN_BOTTOM_LEFT,
                     x - (ref * 3 / REF_DIV), -gap);

        _wave_bars[i] = lv_obj_create(_wave_container);
        lv_obj_set_size(_wave_bars[i], bar_w, WAVE_BAR_MIN_H);
        lv_obj_set_style_radius(_wave_bars[i], ref * 5 / REF_DIV, 0);
        lv_obj_set_style_border_width(_wave_bars[i], 0, 0);
        lv_obj_set_style_pad_all(_wave_bars[i], 0, 0);
        lv_obj_align(_wave_bars[i], LV_ALIGN_BOTTOM_LEFT, x, -gap);
    }

    /* ===== 圆形播放/暂停按钮 ===== */
    _start_btn = lv_btn_create(_screen);
    lv_obj_set_size(_start_btn, btn_size, btn_size);
    lv_obj_set_style_radius(_start_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_start_btn, lv_color_hex(0xE91E63), 0);
    lv_obj_set_style_bg_color(_start_btn, lv_color_hex(0xC2185B), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(_start_btn, btn_size / BTN_SHADOW_DIV, 0);
    lv_obj_set_style_shadow_color(_start_btn, lv_color_hex(0xE91E63), 0);
    lv_obj_set_style_shadow_opa(_start_btn, LV_OPA_30, 0);
    lv_obj_set_style_shadow_ofs_y(_start_btn, btn_size / BTN_SHADOW_OFS_DIV, 0);
    lv_obj_add_event_cb(_start_btn, onScreenBtnClick, LV_EVENT_CLICKED, this);
    lv_obj_align(_start_btn, LV_ALIGN_BOTTOM_MID, 0,
                 -(sh * BTN_Y_RATIO / REF_DIV));

    /* 按钮内图标 */
    {
        lv_obj_t *icon = lv_label_create(_start_btn);
        lv_label_set_text(icon, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_font(icon, choose_font(ref, BTN_ICON_FONT_H), 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(icon);
    }

    /* ===== 底部提示文字 ===== */
    {
        lv_obj_t *hint = lv_label_create(_screen);
        lv_label_set_text(hint, "Tap to start");
        lv_obj_set_style_text_font(hint, choose_font(ref, HINT_FONT_H), 0);
        lv_obj_set_style_text_color(hint, lv_color_hex(0xB0BEC5), 0);
        lv_obj_set_style_text_letter_space(hint, ref * 3 / REF_DIV, 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0,
                     -(sh * HINT_Y_RATIO / REF_DIV));
    }

    /* ===== 音浪刷新定时器 ===== */
    _wave_timer = lv_timer_create(onWaveTimer, 25, this);

    return true;
}

/* =================== 生命周期 =================== */
bool NewApp::run()
{
    if (_screen) {
        lv_scr_load(_screen);
        return true;
    }
    return false;
}

bool NewApp::pause()
{
    stop_echo();
    return notifyCoreClosed();
}

bool NewApp::resume()  { return true; }

bool NewApp::back()    { stop_echo(); bsp_extra_codec_dev_stop(); return notifyCoreClosed(); }
bool NewApp::close()   { stop_echo(); bsp_extra_codec_dev_stop(); return notifyCoreClosed(); }

/* =================== Echo 启动/停止 =================== */
esp_err_t NewApp::start_echo()
{
    if (_echo_task_handle != nullptr) stop_echo();

    SemaphoreHandle_t init_sem = xSemaphoreCreateBinary();
    if (init_sem == NULL) {
        lv_obj_set_style_bg_color(_status_dot, lv_color_hex(0xFF4444), 0);
        lv_label_set_text(_status_label, "Failed");
        return ESP_FAIL;
    }

    bsp_extra_codec_dev_stop();
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_ERROR_CHECK(bsp_extra_codec_init());
    ESP_ERROR_CHECK(bsp_extra_player_init());
    bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE,
                           CODEC_DEFAULT_BIT_WIDTH,
                           (i2s_slot_mode_t)CODEC_DEFAULT_CHANNEL);

    _is_recording = true;

    xTaskCreate(i2s_echo1, "i2s_echo1", 4096, &init_sem, 5, &_echo_task_handle);

    if (xSemaphoreTake(init_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
        ESP_LOGE(TAG, "echo task init timeout");
        _echo_task_handle = nullptr;
        _is_recording = false;
        vSemaphoreDelete(init_sem);
        lv_obj_set_style_bg_color(_status_dot, lv_color_hex(0xFFAA00), 0);
        lv_label_set_text(_status_label, "Timeout");
        return ESP_FAIL;
    }
    vSemaphoreDelete(init_sem);

    if (_echo_task_handle == nullptr) {
        _is_recording = false;
        lv_obj_set_style_bg_color(_status_dot, lv_color_hex(0xFF4444), 0);
        lv_label_set_text(_status_label, "Failed");
        return ESP_FAIL;
    }

    xSemaphoreGive(echo_task_sem);
    g_wave_raw = (volatile uint8_t *)_wave_raw;

    lv_obj_set_style_bg_color(_status_dot, lv_color_hex(0x26C6DA), 0);
    lv_label_set_text(_status_label, "Listening");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x546E7A), 0);
    lv_label_set_text(_level_label, "— dBm");
    startDotPulse();

    lv_obj_set_style_bg_color(_start_btn, lv_color_hex(0x7E57C2), 0);
    lv_obj_set_style_bg_color(_start_btn, lv_color_hex(0x5E35B1), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(_start_btn, lv_color_hex(0x7E57C2), 0);
    lv_obj_t *child = lv_obj_get_child(_start_btn, 0);
    if (child) lv_label_set_text(child, LV_SYMBOL_PAUSE);

    return ESP_OK;
}

void NewApp::startDotPulse(void)
{
    lv_anim_init(&_dot_anim);
    lv_anim_set_var(&_dot_anim, _status_dot);
    lv_anim_set_exec_cb(&_dot_anim, onDotPulseAnim);
    lv_anim_set_values(&_dot_anim, LV_OPA_0, LV_OPA_40);
    lv_anim_set_time(&_dot_anim, 800);
    lv_anim_set_playback_time(&_dot_anim, 800);
    lv_anim_set_repeat_count(&_dot_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&_dot_anim);
}

void NewApp::stopDotPulse(void)
{
    lv_anim_del(_status_dot, onDotPulseAnim);
    lv_obj_set_style_shadow_opa(_status_dot, LV_OPA_0, 0);
}

void NewApp::onDotPulseAnim(void *obj, int32_t v)
{
    lv_obj_set_style_shadow_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

esp_err_t NewApp::stop_echo()
{
    if (!_is_recording) return ESP_OK;

    echo_task_running = false;

    if (_echo_task_handle != nullptr) {
        eTaskState state = eTaskGetState(_echo_task_handle);
        if (state != eDeleted && state != eInvalid) {
            vTaskDelay(pdMS_TO_TICKS(150));
            int timeout = 0;
            while (eTaskGetState(_echo_task_handle) != eDeleted && timeout < 100) {
                vTaskDelay(pdMS_TO_TICKS(20));
                timeout++;
            }
        }
    }

    _echo_task_handle = nullptr;
    _is_recording = false;
    g_wave_raw = nullptr;
    memset((void*)_wave_raw, 0, sizeof(_wave_raw));
    memset(_wave_smoothed, 0, sizeof(_wave_smoothed));
    bsp_extra_codec_dev_stop();

    /* 重置 UI 状态 */
    stopDotPulse();
    lv_obj_set_style_bg_color(_status_dot, lv_color_hex(0xB0BEC5), 0);
    lv_label_set_text(_status_label, "Ready");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x90A4AE), 0);
    lv_label_set_text(_level_label, "— dBm");
    lv_obj_set_style_text_color(_level_label, lv_color_hex(0xB0BEC5), 0);

    lv_obj_set_style_bg_color(_start_btn, lv_color_hex(0xE91E63), 0);
    lv_obj_set_style_bg_color(_start_btn, lv_color_hex(0xC2185B), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(_start_btn, lv_color_hex(0xE91E63), 0);
    lv_obj_t *child = lv_obj_get_child(_start_btn, 0);
    if (child) lv_label_set_text(child, LV_SYMBOL_PLAY);

    return ESP_OK;
}

bool NewApp::isRecording() const       { return _is_recording; }
void NewApp::clearTaskHandle()         { _echo_task_handle = nullptr; }

/* =================== 音浪 UI 更新 =================== */
void NewApp::onWaveTimer(lv_timer_t *timer)
{
    ((NewApp *)timer->user_data)->updateWaveBars();
}

void NewApp::updateWaveBars(void)
{
    uint32_t avg_raw = 0;

    for (int i = 0; i < NUM_WAVE_BARS; i++) {
        float target  = (float)_wave_raw[i];
        float current = _wave_smoothed[i];

     
        _wave_smoothed[i] = current + (target - current) * WAVE_SMOOTH_FACTOR * 1.5f;
       

        uint8_t level = (uint8_t)_wave_smoothed[i];
        if (level < 2) level = 2;

        avg_raw += _wave_raw[i];

        int h = WAVE_BAR_MIN_H + (int)((uint32_t)level * (_bar_max_h - WAVE_BAR_MIN_H) / 255U);

        /* 七彩: 每根竖条不同色相 */
        uint16_t hue = (uint16_t)((uint32_t)i * 360U / NUM_WAVE_BARS);
        uint16_t sat = 80  + (level * 20U / 255U);
        uint16_t val = 60  + (level * 40U / 255U);
        lv_color_t color = lv_color_hsv_to_rgb(hue, (uint8_t)sat, (uint8_t)val);

        uint8_t glow_opa = (uint8_t)(15 + (level / 6));
        lv_obj_set_height(_wave_glows[i], h + 8);
        lv_obj_set_style_bg_color(_wave_glows[i], color, 0);
        lv_obj_set_style_opa(_wave_glows[i], glow_opa, 0);

        lv_obj_set_height(_wave_bars[i], h);
        lv_obj_set_style_bg_color(_wave_bars[i], color, 0);
    }

    avg_raw /= NUM_WAVE_BARS;
    int db = (int)(avg_raw * 60 / 255);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d dBm", db);
    lv_label_set_text(_level_label, buf);

    lv_color_t lvl_clr;
    if      (avg_raw < 30)  lvl_clr = lv_color_hex(0xB0BEC5);
    else if (avg_raw < 100) lvl_clr = lv_color_hex(0x26C6DA);
    else if (avg_raw < 180) lvl_clr = lv_color_hex(0x7E57C2);
    else                    lvl_clr = lv_color_hex(0xE91E63);
    lv_obj_set_style_text_color(_level_label, lvl_clr, 0);
}

/* =================== 按钮点击回调 =================== */
void NewApp::onScreenBtnClick(lv_event_t *e)
{
    NewApp *app = (NewApp *)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (app->_is_recording) app->stop_echo();
        else                    app->start_echo();
    }
}
