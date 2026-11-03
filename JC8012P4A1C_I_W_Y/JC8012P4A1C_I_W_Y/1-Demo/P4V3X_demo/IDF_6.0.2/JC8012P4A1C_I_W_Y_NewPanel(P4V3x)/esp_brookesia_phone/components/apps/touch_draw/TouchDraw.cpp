/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "TouchDraw.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "TouchDraw";

#define TOOLBAR_HEIGHT          56
#define TOOLBAR_BG_COLOR        lv_color_hex(0x2C2C2C)
#define CANVAS_BG_COLOR         lv_color_hex(0xFFFFFF)
#define BTN_SIZE                38
#define BTN_RADIUS              LV_RADIUS_CIRCLE

static const lv_color_t s_color_palette[] = {
    lv_color_hex(0x000000),  // Black
    lv_color_hex(0xE53935),  // Red
    lv_color_hex(0x1E88E5),  // Blue
    lv_color_hex(0x43A047),  // Green
    lv_color_hex(0xFB8C00),  // Orange
    lv_color_hex(0x8E24AA),  // Purple
};

static const uint8_t s_width_options[] = {2, 6, 14};

LV_IMG_DECLARE(draw);

/* ──────────────── Constructor / Destructor ──────────────── */

TouchDraw::TouchDraw(int w, int h)
    : ESP_Brookesia_PhoneApp("Painting", &draw, true, false, false),
      _canvas(nullptr),
      _canvas_buf(nullptr),
      _is_drawing(false),
      _is_eraser(false),
      _toolbar_visible(true),
      _current_color(lv_color_hex(0x000000)),
      _bg_color(CANVAS_BG_COLOR),
      _line_width(2),
      _sw(w),
      _sh(h),
      _toolbar_h(TOOLBAR_HEIGHT),
      _toolbar(nullptr),
      _toggle_btn(nullptr),
      _clear_btn(nullptr),
      _eraser_btn(nullptr)
{
    memset(&_last_point, 0, sizeof(_last_point));
    memset(_color_btns, 0, sizeof(_color_btns));
    memset(_width_btns, 0, sizeof(_width_btns));
}

TouchDraw::~TouchDraw()
{
}

/* ──────────────── App lifecycle overrides ──────────────── */

bool TouchDraw::init(void)
{
    return true;
}

bool TouchDraw::run(void)
{
    lv_coord_t scr_w = lv_obj_get_width(lv_scr_act());
    lv_coord_t scr_h = lv_obj_get_height(lv_scr_act());
    if (_sw <= 0) _sw = scr_w;
    if (_sh <= 0) _sh = scr_h;
    if (_sw < 100) _sw = 1024;
    if (_sh < 100) _sh = 600;

    ESP_LOGI(TAG, "Screen: %dx%d, app size: %dx%d", scr_w, scr_h, _sw, _sh);

    /* 禁止屏幕滚动，否则触摸拖动会被当作屏滚动事件拦截 */
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Toolbar ---- */
    _toolbar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(_toolbar, _sw, _toolbar_h);
    lv_obj_set_pos(_toolbar, 0, 0);
    lv_obj_set_style_bg_color(_toolbar, TOOLBAR_BG_COLOR, 0);
    lv_obj_set_style_radius(_toolbar, 0, 0);
    lv_obj_set_style_border_width(_toolbar, 0, 0);
    lv_obj_set_style_pad_all(_toolbar, 0, 0);
    lv_obj_set_style_pad_left(_toolbar, 6, 0);
    lv_obj_set_style_pad_right(_toolbar, 6, 0);
    lv_obj_set_flex_flow(_toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_toolbar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_toolbar, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Colour buttons --- */
    for (int i = 0; i < 6; i++) {
        _color_btns[i] = lv_btn_create(_toolbar);
        lv_obj_set_size(_color_btns[i], BTN_SIZE, BTN_SIZE);
        lv_obj_set_style_radius(_color_btns[i], BTN_RADIUS, 0);
        lv_obj_set_style_bg_color(_color_btns[i], s_color_palette[i], 0);
        lv_obj_set_style_border_width(_color_btns[i], 2, 0);
        lv_obj_set_style_border_color(_color_btns[i], lv_color_hex(0x888888), 0);
        lv_obj_add_event_cb(_color_btns[i], color_btn_cb, LV_EVENT_CLICKED, this);
        if (i == 0) {
            lv_obj_set_style_border_color(_color_btns[i], lv_color_hex(0xAAAAAA), 0);
        }
    }
    /* Highlight the first (black) colour as active */
    lv_obj_set_style_border_color(_color_btns[0], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(_color_btns[0], 3, 0);

    /* Separator */
    lv_obj_t *sep1 = lv_obj_create(_toolbar);
    lv_obj_set_size(sep1, 1, BTN_SIZE - 6);
    lv_obj_set_style_bg_color(sep1, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_set_style_pad_all(sep1, 0, 0);

    /* --- Width buttons --- */
    for (int i = 0; i < 3; i++) {
        _width_btns[i] = lv_btn_create(_toolbar);
        lv_obj_set_size(_width_btns[i], BTN_SIZE, BTN_SIZE);
        lv_obj_set_style_radius(_width_btns[i], BTN_RADIUS, 0);
        lv_obj_set_style_bg_color(_width_btns[i], lv_color_hex(0x444444), 0);
        lv_obj_set_style_border_width(_width_btns[i], 2, 0);
        lv_obj_set_style_border_color(_width_btns[i], lv_color_hex(0x888888), 0);
        lv_obj_add_event_cb(_width_btns[i], toolbar_btn_cb, LV_EVENT_CLICKED, this);

        lv_obj_t *dot = lv_obj_create(_width_btns[i]);
        lv_coord_t dot_sz = (lv_coord_t)(4 + i * 5);
        lv_obj_set_size(dot, dot_sz, dot_sz);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_center(dot);
    }
    lv_obj_set_style_border_width(_width_btns[0], 3, 0);
    lv_obj_set_style_border_color(_width_btns[0], lv_color_hex(0xFFFFFF), 0);

    /* Separator */
    lv_obj_t *sep2 = lv_obj_create(_toolbar);
    lv_obj_set_size(sep2, 1, BTN_SIZE - 6);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);

    /* --- Eraser button --- */
    _eraser_btn = lv_btn_create(_toolbar);
    lv_obj_set_size(_eraser_btn, BTN_SIZE, BTN_SIZE);
    lv_obj_set_style_radius(_eraser_btn, 6, 0);
    lv_obj_set_style_bg_color(_eraser_btn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(_eraser_btn, 2, 0);
    lv_obj_set_style_border_color(_eraser_btn, lv_color_hex(0x888888), 0);
    lv_obj_add_event_cb(_eraser_btn, toolbar_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *eraser_lbl = lv_label_create(_eraser_btn);
    lv_label_set_text(eraser_lbl, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(eraser_lbl, lv_color_hex(0xDDDDDD), 0);
    lv_obj_center(eraser_lbl);

    /* --- Clear button --- */
    _clear_btn = lv_btn_create(_toolbar);
    lv_obj_set_size(_clear_btn, BTN_SIZE, BTN_SIZE);
    lv_obj_set_style_radius(_clear_btn, 6, 0);
    lv_obj_set_style_bg_color(_clear_btn, lv_color_hex(0xC62828), 0);
    lv_obj_set_style_border_width(_clear_btn, 2, 0);
    lv_obj_set_style_border_color(_clear_btn, lv_color_hex(0x888888), 0);
    lv_obj_add_event_cb(_clear_btn, clear_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *clear_lbl = lv_label_create(_clear_btn);
    lv_label_set_text(clear_lbl, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_color(clear_lbl, lv_color_white(), 0);
    lv_obj_center(clear_lbl);

    /* ---- Canvas ---- */
    _canvas = lv_canvas_create(lv_scr_act());
    lv_obj_add_flag(_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(_canvas, LV_OBJ_FLAG_SCROLLABLE);

    size_t buf_size = LV_CANVAS_BUF_SIZE_TRUE_COLOR(_sw, _sh);
    ESP_LOGI(TAG, "Canvas buffer = %zu bytes", buf_size);

    _canvas_buf = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (_canvas_buf == NULL) {
        ESP_LOGW(TAG, "PSRAM alloc failed, fallback to internal RAM");
        _canvas_buf = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL);
    }
    if (_canvas_buf == NULL) {
        ESP_LOGE(TAG, "Cannot allocate canvas buffer");
        return false;
    }

    lv_canvas_set_buffer(_canvas, _canvas_buf, _sw, _sh, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(_canvas, _bg_color, LV_OPA_COVER);

    lv_obj_add_event_cb(_canvas, canvas_event_cb, LV_EVENT_PRESSED,  this);
    lv_obj_add_event_cb(_canvas, canvas_event_cb, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(_canvas, canvas_event_cb, LV_EVENT_RELEASED, this);

    /* ---- 工具栏切换按钮（画布右上角浮动） ---- */
    _toggle_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(_toggle_btn, 32, 32);
    lv_obj_set_style_radius(_toggle_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_toggle_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_shadow_width(_toggle_btn, 4, 0);
    lv_obj_set_style_shadow_color(_toggle_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(_toggle_btn, LV_OPA_40, 0);
    lv_obj_set_style_border_width(_toggle_btn, 0, 0);
    lv_obj_add_event_cb(_toggle_btn, toggle_btn_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(_toggle_btn, toggle_btn_cb, LV_EVENT_LONG_PRESSED, this);

    lv_obj_t *tgl_lbl = lv_label_create(_toggle_btn);
    lv_label_set_text(tgl_lbl, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(tgl_lbl, lv_color_white(), 0);
    lv_obj_center(tgl_lbl);

    /* 应用初始布局 */
    updateLayout();

    return true;
}

/* ──────────────── 布局更新 ──────────────── */

void TouchDraw::updateLayout(void)
{
    lv_coord_t canvas_x = 0;
    lv_coord_t canvas_y = _toolbar_visible ? _toolbar_h : 0;
    lv_coord_t canvas_w = _sw;
    lv_coord_t canvas_h = _toolbar_visible ? (_sh - _toolbar_h) : _sh;

    /* 更新工具栏可见性 */
    if (_toolbar) {
        if (_toolbar_visible) {
            lv_obj_clear_flag(_toolbar, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_toolbar, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 更新画布位置和尺寸 */
    if (_canvas) {
        lv_obj_set_pos(_canvas, canvas_x, canvas_y);
        lv_obj_set_size(_canvas, canvas_w, canvas_h);
    }

    /* 切换按钮固定在工具栏行（屏幕顶部同一行） */
    if (_toggle_btn) {
        lv_obj_set_pos(_toggle_btn, _sw - 42, 6);
        lv_obj_t *lbl = lv_obj_get_child(_toggle_btn, 0);
        if (lbl) {
            lv_label_set_text(lbl, _toolbar_visible ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
        }
    }
}

bool TouchDraw::back(void)
{
    notifyCoreClosed();
    return true;
}

bool TouchDraw::close(void)
{
    if (_canvas_buf != NULL) {
        free(_canvas_buf);
        _canvas_buf = NULL;
    }
    return true;
}

/* ──────────────── Drawing helpers ──────────────── */

void TouchDraw::setDrawColor(lv_color_t color)
{
    _current_color = color;
    _is_eraser = false;
}

void TouchDraw::setLineWidth(uint8_t width)
{
    _line_width = width;
}

void TouchDraw::clearCanvas(void)
{
    if (_canvas == NULL) return;
    lv_canvas_fill_bg(_canvas, _bg_color, LV_OPA_COVER);
    lv_obj_invalidate(_canvas);
}

/* ──────────────── Event callbacks ──────────────── */

void TouchDraw::canvas_event_cb(lv_event_t *e)
{
    TouchDraw *app = (TouchDraw *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    lv_indev_t *indev = lv_event_get_indev(e);
    if (!indev) return;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    /* Convert to canvas-relative coordinates */
    lv_area_t coords;
    lv_obj_get_coords(app->_canvas, &coords);
    lv_coord_t cw = lv_obj_get_width(app->_canvas);
    lv_coord_t ch = lv_obj_get_height(app->_canvas);

    pt.x -= coords.x1;
    pt.y -= coords.y1;

    if (pt.x < 0)          pt.x = 0;
    if (pt.y < 0)          pt.y = 0;
    if (pt.x >= cw) pt.x = cw - 1;
    if (pt.y >= ch) pt.y = ch - 1;

    lv_color_t draw_color = app->_is_eraser ? app->_bg_color : app->_current_color;

    switch (code) {

    case LV_EVENT_PRESSED: {
        app->_is_drawing = true;
        app->_last_point = pt;

        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color       = draw_color;
        line_dsc.width       = app->_line_width;
        line_dsc.round_start = 1;
        line_dsc.round_end   = 1;

        lv_point_t pts[2] = { pt, pt };
        lv_canvas_draw_line(app->_canvas, pts, 2, &line_dsc);
        lv_obj_invalidate(app->_canvas);
        break;
    }

    case LV_EVENT_PRESSING: {
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color       = draw_color;
        line_dsc.width       = app->_line_width;
        line_dsc.round_start = 1;
        line_dsc.round_end   = 1;

        lv_point_t pts[2] = { app->_last_point, pt };
        lv_canvas_draw_line(app->_canvas, pts, 2, &line_dsc);
        lv_obj_invalidate(app->_canvas);

        app->_last_point = pt;
        break;
    }

    case LV_EVENT_RELEASED:
        app->_is_drawing = false;
        break;

    default:
        break;
    }
}

void TouchDraw::clear_btn_cb(lv_event_t *e)
{
    TouchDraw *app = (TouchDraw *)lv_event_get_user_data(e);
    app->clearCanvas();
}

void TouchDraw::color_btn_cb(lv_event_t *e)
{
    TouchDraw *app = (TouchDraw *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);

    for (int i = 0; i < 6; i++) {
        if (app->_color_btns[i] == btn) {
            app->setDrawColor(s_color_palette[i]);

            for (int j = 0; j < 6; j++) {
                lv_obj_set_style_border_width(app->_color_btns[j], 2, 0);
                lv_obj_set_style_border_color(app->_color_btns[j], lv_color_hex(0x888888), 0);
            }
            lv_obj_set_style_border_width(btn, 3, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), 0);
            break;
        }
    }
}

void TouchDraw::toolbar_btn_cb(lv_event_t *e)
{
    TouchDraw *app = (TouchDraw *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (btn == app->_eraser_btn) {
        app->_is_eraser = !app->_is_eraser;
        if (app->_is_eraser) {
            lv_obj_set_style_border_color(app->_eraser_btn, lv_color_hex(0xFFD600), 0);
            lv_obj_set_style_border_width(app->_eraser_btn, 3, 0);
        } else {
            lv_obj_set_style_border_color(app->_eraser_btn, lv_color_hex(0x888888), 0);
            lv_obj_set_style_border_width(app->_eraser_btn, 2, 0);
        }
        return;
    }

    for (int i = 0; i < 3; i++) {
        if (app->_width_btns[i] == btn) {
            app->setLineWidth(s_width_options[i]);

            for (int j = 0; j < 3; j++) {
                lv_obj_set_style_border_width(app->_width_btns[j], 2, 0);
                lv_obj_set_style_border_color(app->_width_btns[j], lv_color_hex(0x888888), 0);
            }
            lv_obj_set_style_border_width(btn, 3, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), 0);
            break;
        }
    }
}

void TouchDraw::toggle_btn_cb(lv_event_t *e)
{
    TouchDraw *app = (TouchDraw *)lv_event_get_user_data(e);

    /* 长按：退出应用 */
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
        app->back();
        return;
    }

    /* 单击：切换工具栏显隐 */
    app->_toolbar_visible = !app->_toolbar_visible;
    app->updateLayout();
}
