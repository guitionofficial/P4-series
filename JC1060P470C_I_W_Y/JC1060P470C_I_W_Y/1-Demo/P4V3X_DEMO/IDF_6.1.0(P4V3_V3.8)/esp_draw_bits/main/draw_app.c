/*
 * Draw application implementation - LVGL Canvas touch drawing
 *
 * Resolution-adaptive design:
 *  - All UI sizes derive from short_side = MIN(screen_w, screen_h)
 *  - Baseline: 600 px → toolbar 56 px, buttons 38 px, etc.
 *  - Scale factor = short_side / 600 clamped to [240/600, 1920/600]
 *  - Works on any screen from 240×240 to 1920×1080+
 *
 * Improvements over original main.c:
 *  - Uses lv_canvas_draw_line() (native LVGL API) for efficient drawing
 *  - Eraser mode (draw with background color)
 *  - Toolbar at top with flex layout + separators
 *  - Floating toggle button to show/hide toolbar
 *  - PSRAM-first memory allocation with internal RAM fallback
 *  - Round line caps for smooth strokes
 */

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "draw_app.h"

static const char *TAG = "DrawApp";

/* ---- Colour palette (RGB values, converted at init) ---- */
static const uint32_t s_palette_rgb[NUM_COLORS] = {
    0x000000,  /* Black  */
    0xE53935,  /* Red    */
    0x1E88E5,  /* Blue   */
    0x43A047,  /* Green  */
    0xFB8C00,  /* Orange */
    0x8E24AA,  /* Purple */
};

/* ---- Forward declarations ---- */
static void compute_layout(draw_app_t *app);
static void on_canvas_event(lv_event_t *e);
static void on_clear_click(lv_event_t *e);
static void on_color_click(lv_event_t *e);
static void on_toolbar_btn_click(lv_event_t *e);
static void on_toggle_btn_event(lv_event_t *e);
static void update_layout(draw_app_t *app);
static void set_draw_color(draw_app_t *app, lv_color_t color);
static void set_line_width(draw_app_t *app, uint8_t width);
static void clear_canvas(draw_app_t *app);

/* ==================================================================
 *  Layout computation — the core of resolution adaptation
 * ================================================================== */

static void compute_layout(draw_app_t *app)
{
    int ref = app->short_side;

    /* Clamp scale so tiny or huge screens don't produce absurd sizes.
     * baseline: 600 px short-side -> scale = 1.0
     * min:      240 px  -> scale = 0.4
     * max:     1920 px  -> scale = 3.2
     */
    int clamped = ref;
    if (clamped < 240) clamped = 240;
    if (clamped > 1920) clamped = 1920;

    float s = (float)clamped / 600.0f;

    app->toolbar_h        = (int)(56.0f  * s);
    app->btn_size         = (int)(38.0f  * s);
    app->toggle_size      = (int)(32.0f  * s);
    app->toggle_x         = (int)(42.0f  * s);
    app->toggle_y         = (int)(6.0f   * s);
    app->toolbar_pad_l    = (int)(6.0f   * s);
    app->toolbar_pad_r    = (int)(50.0f  * s);
    app->sep_w            = (int)(2.0f   * s);
    app->sep_h            = (int)(30.0f  * s);    /* BTN_SIZE - 8 at baseline */
    app->eraser_radius    = (int)(6.0f   * s);
    app->shadow_width     = (int)(4.0f   * s);

    /* Border widths — keep reasonable, don't scale below 1 or above 6 */
    app->border_normal    = (int)(2.0f * s);  if (app->border_normal    < 1) app->border_normal    = 1; if (app->border_normal    > 6) app->border_normal    = 6;
    app->border_highlight = (int)(3.0f * s);  if (app->border_highlight < 2) app->border_highlight = 2; if (app->border_highlight > 9) app->border_highlight = 9;

    /* Line widths: baseline {2, 6, 14}, scaled proportionally */
    int lw[NUM_LINE_WIDTHS] = {
        (int)(2.0f  * s),
        (int)(6.0f  * s),
        (int)(14.0f * s),
    };
    for (int i = 0; i < NUM_LINE_WIDTHS; i++) {
        if (lw[i] < 1)  lw[i] = 1;
        if (lw[i] > 40) lw[i] = 40;
        app->line_widths[i] = (uint8_t)lw[i];
    }

    /* Dot indicators: baseline {4, 9, 14} */
    int ds[NUM_LINE_WIDTHS] = {
        (int)(4.0f  * s),
        (int)(9.0f  * s),
        (int)(14.0f * s),
    };
    for (int i = 0; i < NUM_LINE_WIDTHS; i++) {
        if (ds[i] < 2)  ds[i] = 2;
        if (ds[i] > app->btn_size - 4) ds[i] = app->btn_size - 4;
        app->dot_sizes[i] = ds[i];
    }
}

/* ==================================================================
 *  Public API
 * ================================================================== */

bool draw_app_run(draw_app_t *app, int w, int h)
{
    memset(app, 0, sizeof(*app));

    lv_obj_t *scr = lv_scr_act();
    lv_coord_t scr_w = lv_obj_get_width(scr);
    lv_coord_t scr_h = lv_obj_get_height(scr);

    /* Resolve canvas size */
    if (w <= 0) app->screen_w = scr_w; else app->screen_w = w;
    if (h <= 0) app->screen_h = scr_h; else app->screen_h = h;

    /* Sanity floor */
    if (app->screen_w < 100) app->screen_w = 1024;
    if (app->screen_h < 100) app->screen_h = 600;

    app->short_side = LV_MIN(app->screen_w, app->screen_h);

    /* Compute all layout dimensions */
    compute_layout(app);

    ESP_LOGI(TAG, "Screen %" LV_PRId32 "x%" LV_PRId32 ", canvas %dx%d, "
             "toolbar_h=%d, btn=%d, line_w=%d/%d/%d",
             scr_w, scr_h, app->screen_w, app->screen_h,
             app->toolbar_h, app->btn_size,
             app->line_widths[0], app->line_widths[1], app->line_widths[2]);

    /* Default state */
    app->current_color   = CANVAS_BG_COLOR();
    app->bg_color        = CANVAS_BG_COLOR();
    app->line_width      = app->line_widths[0];
    app->is_eraser       = false;
    app->toolbar_visible = true;

    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ------------------------------------------------------------------
     *  Toolbar  (top, flex row)
     * ------------------------------------------------------------------ */
    app->toolbar = lv_obj_create(scr);
    lv_obj_set_size(app->toolbar, app->screen_w, app->toolbar_h);
    lv_obj_set_pos(app->toolbar, 0, 0);
    lv_obj_set_style_bg_color(app->toolbar, TOOLBAR_BG_COLOR(), 0);
    lv_obj_set_style_radius(app->toolbar, 0, 0);
    lv_obj_set_style_border_width(app->toolbar, 0, 0);
    lv_obj_set_style_pad_all(app->toolbar, 0, 0);
    lv_obj_set_style_pad_left(app->toolbar, app->toolbar_pad_l, 0);
    lv_obj_set_style_pad_right(app->toolbar, app->toolbar_pad_r, 0);
    lv_obj_set_flex_flow(app->toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(app->toolbar, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(app->toolbar, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Colour buttons --- */
    for (int i = 0; i < NUM_COLORS; i++) {
        app->color_btns[i] = lv_btn_create(app->toolbar);
        lv_obj_set_size(app->color_btns[i], app->btn_size, app->btn_size);
        lv_obj_set_style_radius(app->color_btns[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(app->color_btns[i],
                                  lv_color_hex(s_palette_rgb[i]), 0);
        lv_obj_set_style_border_width(app->color_btns[i], app->border_normal, 0);
        lv_obj_set_style_border_color(app->color_btns[i],
                                      lv_color_hex(BTN_BORDER_COLOR_32), 0);
        lv_obj_add_event_cb(app->color_btns[i], on_color_click,
                            LV_EVENT_CLICKED, app);
    }
    /* Highlight first colour as active */
    lv_obj_set_style_border_width(app->color_btns[0], app->border_highlight, 0);
    lv_obj_set_style_border_color(app->color_btns[0],
                                  lv_color_hex(BTN_HIGHLIGHT_COLOR_32), 0);

    /* Separator 1 */
    lv_obj_t *sep1 = lv_obj_create(app->toolbar);
    lv_obj_set_size(sep1, app->sep_w, app->sep_h);
    lv_obj_set_style_bg_color(sep1, lv_color_hex(SEP_COLOR_32), 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_set_style_pad_all(sep1, 0, 0);

    /* --- Line-width buttons --- */
    for (int i = 0; i < NUM_LINE_WIDTHS; i++) {
        app->width_btns[i] = lv_btn_create(app->toolbar);
        lv_obj_set_size(app->width_btns[i], app->btn_size, app->btn_size);
        lv_obj_set_style_radius(app->width_btns[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(app->width_btns[i],
                                  lv_color_hex(WIDTH_BTN_BG_COLOR_32), 0);
        lv_obj_set_style_border_width(app->width_btns[i], app->border_normal, 0);
        lv_obj_set_style_border_color(app->width_btns[i],
                                      lv_color_hex(BTN_BORDER_COLOR_32), 0);
        lv_obj_add_event_cb(app->width_btns[i], on_toolbar_btn_click,
                            LV_EVENT_CLICKED, app);

        /* Dot indicator */
        lv_obj_t *dot = lv_obj_create(app->width_btns[i]);
        lv_obj_set_size(dot, app->dot_sizes[i], app->dot_sizes[i]);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_center(dot);
    }
    /* Highlight first width as active */
    lv_obj_set_style_border_width(app->width_btns[0], app->border_highlight, 0);
    lv_obj_set_style_border_color(app->width_btns[0],
                                  lv_color_hex(BTN_HIGHLIGHT_COLOR_32), 0);

    /* Separator 2 */
    lv_obj_t *sep2 = lv_obj_create(app->toolbar);
    lv_obj_set_size(sep2, app->sep_w, app->sep_h);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(SEP_COLOR_32), 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);

    /* --- Eraser button --- */
    app->eraser_btn = lv_btn_create(app->toolbar);
    lv_obj_set_size(app->eraser_btn, app->btn_size, app->btn_size);
    lv_obj_set_style_radius(app->eraser_btn, app->eraser_radius, 0);
    lv_obj_set_style_bg_color(app->eraser_btn,
                              lv_color_hex(ERASER_BTN_BG_COLOR_32), 0);
    lv_obj_set_style_border_width(app->eraser_btn, app->border_normal, 0);
    lv_obj_set_style_border_color(app->eraser_btn,
                                  lv_color_hex(BTN_BORDER_COLOR_32), 0);
    lv_obj_add_event_cb(app->eraser_btn, on_toolbar_btn_click,
                        LV_EVENT_CLICKED, app);

    lv_obj_t *eraser_lbl = lv_label_create(app->eraser_btn);
    lv_label_set_text(eraser_lbl, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(eraser_lbl, lv_color_hex(0xDDDDDD), 0);
    lv_obj_center(eraser_lbl);

    /* --- Clear button --- */
    app->clear_btn = lv_btn_create(app->toolbar);
    lv_obj_set_size(app->clear_btn, app->btn_size, app->btn_size);
    lv_obj_set_style_radius(app->clear_btn, app->eraser_radius, 0);
    lv_obj_set_style_bg_color(app->clear_btn,
                              lv_color_hex(CLEAR_BTN_BG_COLOR_32), 0);
    lv_obj_set_style_border_width(app->clear_btn, app->border_normal, 0);
    lv_obj_set_style_border_color(app->clear_btn,
                                  lv_color_hex(BTN_BORDER_COLOR_32), 0);
    lv_obj_add_event_cb(app->clear_btn, on_clear_click, LV_EVENT_CLICKED, app);

    lv_obj_t *clear_lbl = lv_label_create(app->clear_btn);
    lv_label_set_text(clear_lbl, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_color(clear_lbl, lv_color_white(), 0);
    lv_obj_center(clear_lbl);

    /* ------------------------------------------------------------------
     *  Canvas
     * ------------------------------------------------------------------ */
    app->canvas = lv_canvas_create(scr);
    lv_obj_add_flag(app->canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(app->canvas, LV_OBJ_FLAG_SCROLLABLE);

    size_t buf_size = LV_CANVAS_BUF_SIZE_TRUE_COLOR(app->screen_w, app->screen_h);
    ESP_LOGI(TAG, "Canvas buffer = %zu bytes", buf_size);

    app->canvas_buf = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (app->canvas_buf == NULL) {
        ESP_LOGW(TAG, "PSRAM alloc failed, fallback to internal RAM");
        app->canvas_buf = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL);
    }
    if (app->canvas_buf == NULL) {
        ESP_LOGE(TAG, "Cannot allocate canvas buffer");
        return false;
    }

    lv_canvas_set_buffer(app->canvas, app->canvas_buf,
                         app->screen_w, app->screen_h, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(app->canvas, app->bg_color, LV_OPA_COVER);

    lv_obj_add_event_cb(app->canvas, on_canvas_event, LV_EVENT_PRESSED,  app);
    lv_obj_add_event_cb(app->canvas, on_canvas_event, LV_EVENT_PRESSING, app);
    lv_obj_add_event_cb(app->canvas, on_canvas_event, LV_EVENT_RELEASED, app);

    /* ------------------------------------------------------------------
     *  Toolbar toggle button  (floating, top-right)
     * ------------------------------------------------------------------ */
    app->toggle_btn = lv_btn_create(scr);
    lv_obj_set_size(app->toggle_btn, app->toggle_size, app->toggle_size);
    lv_obj_set_style_radius(app->toggle_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(app->toggle_btn,
                              lv_color_hex(TOGGLE_BTN_BG_COLOR_32), 0);
    lv_obj_set_style_shadow_width(app->toggle_btn, app->shadow_width, 0);
    lv_obj_set_style_shadow_color(app->toggle_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(app->toggle_btn, LV_OPA_40, 0);
    lv_obj_set_style_border_width(app->toggle_btn, 0, 0);
    lv_obj_add_event_cb(app->toggle_btn, on_toggle_btn_event,
                        LV_EVENT_CLICKED, app);
    lv_obj_add_event_cb(app->toggle_btn, on_toggle_btn_event,
                        LV_EVENT_LONG_PRESSED, app);

    lv_obj_t *tgl_lbl = lv_label_create(app->toggle_btn);
    lv_label_set_text(tgl_lbl, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(tgl_lbl, lv_color_white(), 0);
    lv_obj_center(tgl_lbl);

    /* Apply initial layout */
    update_layout(app);

    return true;
}

void draw_app_close(draw_app_t *app)
{
    if (app->canvas_buf) {
        free(app->canvas_buf);
        app->canvas_buf = NULL;
    }
}

/* ==================================================================
 *  Layout
 * ================================================================== */

static void update_layout(draw_app_t *app)
{
    lv_coord_t canvas_y = app->toolbar_visible ? app->toolbar_h : 0;
    lv_coord_t canvas_h = app->toolbar_visible
                          ? (app->screen_h - app->toolbar_h)
                          : app->screen_h;

    /* Show / hide toolbar */
    if (app->toolbar) {
        if (app->toolbar_visible)
            lv_obj_clear_flag(app->toolbar, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(app->toolbar, LV_OBJ_FLAG_HIDDEN);
    }

    /* Update canvas position & size */
    if (app->canvas) {
        lv_obj_set_pos(app->canvas, 0, canvas_y);
        lv_obj_set_size(app->canvas, app->screen_w, canvas_h);
    }

    /* Position toggle button (always visible, top-right) */
    if (app->toggle_btn) {
        lv_obj_set_pos(app->toggle_btn,
                       app->screen_w - app->toggle_x,
                       app->toggle_y);
        lv_obj_t *lbl = lv_obj_get_child(app->toggle_btn, 0);
        if (lbl) {
            lv_label_set_text(lbl, app->toolbar_visible
                                    ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
        }
    }
}

/* ==================================================================
 *  Drawing helpers
 * ================================================================== */

static void set_draw_color(draw_app_t *app, lv_color_t color)
{
    app->current_color = color;
    app->is_eraser = false;

    /* Reset eraser button style */
    lv_obj_set_style_border_color(app->eraser_btn,
                                  lv_color_hex(BTN_BORDER_COLOR_32), 0);
    lv_obj_set_style_border_width(app->eraser_btn, app->border_normal, 0);
}

static void set_line_width(draw_app_t *app, uint8_t width)
{
    app->line_width = width;
}

static void clear_canvas(draw_app_t *app)
{
    if (!app->canvas) return;
    lv_canvas_fill_bg(app->canvas, app->bg_color, LV_OPA_COVER);
    lv_obj_invalidate(app->canvas);
}

/* ==================================================================
 *  Event callbacks
 * ================================================================== */

static void on_canvas_event(lv_event_t *e)
{
    draw_app_t *app = (draw_app_t *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    lv_indev_t *indev = lv_event_get_indev(e);
    if (!indev) return;

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    /* Convert screen coords to canvas-relative */
    lv_area_t coords;
    lv_obj_get_coords(app->canvas, &coords);
    lv_coord_t cw = lv_obj_get_width(app->canvas);
    lv_coord_t ch = lv_obj_get_height(app->canvas);

    pt.x -= coords.x1;
    pt.y -= coords.y1;

    if (pt.x <  0) pt.x = 0;
    if (pt.y <  0) pt.y = 0;
    if (pt.x >= cw) pt.x = cw - 1;
    if (pt.y >= ch) pt.y = ch - 1;

    lv_color_t draw_color = app->is_eraser ? app->bg_color : app->current_color;

    switch (code) {

    case LV_EVENT_PRESSED:
        app->last_point = pt;

        /* Draw initial dot */
        {
            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color       = draw_color;
            line_dsc.width       = app->line_width;
            line_dsc.round_start = 1;
            line_dsc.round_end   = 1;

            lv_point_t pts[2] = { pt, pt };
            lv_canvas_draw_line(app->canvas, pts, 2, &line_dsc);
            lv_obj_invalidate(app->canvas);
        }
        break;

    case LV_EVENT_PRESSING:
        {
            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color       = draw_color;
            line_dsc.width       = app->line_width;
            line_dsc.round_start = 1;
            line_dsc.round_end   = 1;

            lv_point_t pts[2] = { app->last_point, pt };
            lv_canvas_draw_line(app->canvas, pts, 2, &line_dsc);
            lv_obj_invalidate(app->canvas);

            app->last_point = pt;
        }
        break;

    case LV_EVENT_RELEASED:
        app->last_point.x = 0;
        app->last_point.y = 0;
        break;

    default:
        break;
    }
}

static void on_clear_click(lv_event_t *e)
{
    draw_app_t *app = (draw_app_t *)lv_event_get_user_data(e);
    clear_canvas(app);
}

static void on_color_click(lv_event_t *e)
{
    draw_app_t *app = (draw_app_t *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);

    for (int i = 0; i < NUM_COLORS; i++) {
        if (app->color_btns[i] == btn) {
            set_draw_color(app, lv_color_hex(s_palette_rgb[i]));

            /* Update all button styles */
            for (int j = 0; j < NUM_COLORS; j++) {
                lv_obj_set_style_border_width(app->color_btns[j],
                                              app->border_normal, 0);
                lv_obj_set_style_border_color(app->color_btns[j],
                                              lv_color_hex(BTN_BORDER_COLOR_32), 0);
            }
            /* Highlight selected */
            lv_obj_set_style_border_width(btn, app->border_highlight, 0);
            lv_obj_set_style_border_color(btn,
                                          lv_color_hex(BTN_HIGHLIGHT_COLOR_32), 0);
            break;
        }
    }
}

static void on_toolbar_btn_click(lv_event_t *e)
{
    draw_app_t *app = (draw_app_t *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);

    /* Eraser */
    if (btn == app->eraser_btn) {
        app->is_eraser = !app->is_eraser;
        if (app->is_eraser) {
            lv_obj_set_style_border_color(app->eraser_btn,
                                          lv_color_hex(ERASER_ACTIVE_COLOR_32), 0);
            lv_obj_set_style_border_width(app->eraser_btn,
                                          app->border_highlight, 0);
        } else {
            lv_obj_set_style_border_color(app->eraser_btn,
                                          lv_color_hex(BTN_BORDER_COLOR_32), 0);
            lv_obj_set_style_border_width(app->eraser_btn,
                                          app->border_normal, 0);
        }
        return;
    }

    /* Line width */
    for (int i = 0; i < NUM_LINE_WIDTHS; i++) {
        if (app->width_btns[i] == btn) {
            set_line_width(app, app->line_widths[i]);

            for (int j = 0; j < NUM_LINE_WIDTHS; j++) {
                lv_obj_set_style_border_width(app->width_btns[j],
                                              app->border_normal, 0);
                lv_obj_set_style_border_color(app->width_btns[j],
                                              lv_color_hex(BTN_BORDER_COLOR_32), 0);
            }
            lv_obj_set_style_border_width(btn, app->border_highlight, 0);
            lv_obj_set_style_border_color(btn,
                                          lv_color_hex(BTN_HIGHLIGHT_COLOR_32), 0);
            break;
        }
    }
}

static void on_toggle_btn_event(lv_event_t *e)
{
    draw_app_t *app = (draw_app_t *)lv_event_get_user_data(e);

    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
        ESP_LOGI(TAG, "Long press on toggle — clearing canvas");
        clear_canvas(app);
        return;
    }

    /* Click: toggle toolbar */
    app->toolbar_visible = !app->toolbar_visible;
    update_layout(app);
}
