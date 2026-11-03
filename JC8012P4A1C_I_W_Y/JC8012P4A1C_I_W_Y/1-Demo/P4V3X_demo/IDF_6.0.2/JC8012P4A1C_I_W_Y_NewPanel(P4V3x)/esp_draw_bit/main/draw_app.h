/*
 * Draw application header - LVGL Canvas touch drawing
 *
 * Resolution-adaptive: all UI sizes are derived from the screen's
 * shorter side (MIN(width, height)) relative to a 600 px baseline.
 * Works automatically on any screen from 240×240 up to 1920×1080.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Colour constants (resolution-independent) ---- */

#define TOOLBAR_BG_COLOR_32      0x2C2C2C
#define TOOLBAR_BG_COLOR()       lv_color_hex(TOOLBAR_BG_COLOR_32)
#define CANVAS_BG_COLOR_32       0xFFFFFF
#define CANVAS_BG_COLOR()        lv_color_hex(CANVAS_BG_COLOR_32)
#define SEP_COLOR_32             0x555555
#define BTN_BORDER_COLOR_32      0x888888
#define BTN_HIGHLIGHT_COLOR_32   0xFFFFFF
#define ERASER_ACTIVE_COLOR_32   0xFFD600
#define WIDTH_BTN_BG_COLOR_32    0x444444
#define ERASER_BTN_BG_COLOR_32   0x555555
#define CLEAR_BTN_BG_COLOR_32    0xC62828
#define TOGGLE_BTN_BG_COLOR_32   0x333333

#define NUM_COLORS              6
#define NUM_LINE_WIDTHS         3

/* ---- Application state ---- */

typedef struct {
    int screen_w;
    int screen_h;
    int short_side;          /* LV_MIN(screen_w, screen_h) — all sizes scale from this */

    /* Computed layout dimensions (all proportional to short_side) */
    int toolbar_h;           /* Toolbar height */
    int btn_size;            /* Square button size */
    int toggle_size;         /* Toggle button */
    int toggle_x;            /* Toggle X pos (from right) */
    int toggle_y;            /* Toggle Y pos */
    int toolbar_pad_l;       /* Toolbar left padding */
    int toolbar_pad_r;       /* Toolbar right padding */
    int sep_w;               /* Separator width */
    int sep_h;               /* Separator height */
    int eraser_radius;       /* Radius for eraser/clear buttons */
    int border_normal;       /* Normal border width */
    int border_highlight;    /* Highlight border width */
    int shadow_width;        /* Toggle shadow */

    /* Computed line widths (proportional) */
    uint8_t line_widths[NUM_LINE_WIDTHS];
    /* Computed dot sizes (proportional) */
    int dot_sizes[NUM_LINE_WIDTHS];

    lv_obj_t   *canvas;
    lv_color_t *canvas_buf;

    lv_point_t  last_point;
    bool        is_eraser;
    bool        toolbar_visible;
    lv_color_t  current_color;
    lv_color_t  bg_color;
    uint8_t     line_width;

    /* Toolbar widgets */
    lv_obj_t *toolbar;
    lv_obj_t *toggle_btn;
    lv_obj_t *color_btns[NUM_COLORS];
    lv_obj_t *width_btns[NUM_LINE_WIDTHS];
    lv_obj_t *clear_btn;
    lv_obj_t *eraser_btn;
} draw_app_t;

/* ---- Public API ---- */

/**
 * @brief Initialize and create the drawing UI on the active screen.
 *        All UI sizes auto-scale based on screen's shorter side.
 * @param app  Pointer to application state (will be zeroed)
 * @param w    Canvas width  (0 = auto-detect from screen)
 * @param h    Canvas height (0 = auto-detect from screen)
 * @return     true on success
 */
bool draw_app_run(draw_app_t *app, int w, int h);

/**
 * @brief Free resources (canvas buffer, etc.)
 */
void draw_app_close(draw_app_t *app);

#ifdef __cplusplus
}
#endif
