#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"

class TouchDraw : public ESP_Brookesia_PhoneApp
{
public:
    TouchDraw(int w, int h);
    ~TouchDraw();

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;

private:
    static void canvas_event_cb(lv_event_t *e);
    static void clear_btn_cb(lv_event_t *e);
    static void color_btn_cb(lv_event_t *e);
    static void toolbar_btn_cb(lv_event_t *e);
    static void toggle_btn_cb(lv_event_t *e);

    void setDrawColor(lv_color_t color);
    void setLineWidth(uint8_t width);
    void clearCanvas(void);
    void updateLayout(void);

    lv_obj_t *_canvas;
    lv_color_t *_canvas_buf;
    lv_point_t _last_point;
    bool _is_drawing;
    bool _is_eraser;
    bool _toolbar_visible;
    lv_color_t _current_color;
    lv_color_t _bg_color;
    uint8_t _line_width;
    int _sw;
    int _sh;
    uint16_t _toolbar_h;

    lv_obj_t *_toolbar;
    lv_obj_t *_toggle_btn;
    lv_obj_t *_color_btns[6];
    lv_obj_t *_width_btns[3];
    lv_obj_t *_clear_btn;
    lv_obj_t *_eraser_btn;
};
