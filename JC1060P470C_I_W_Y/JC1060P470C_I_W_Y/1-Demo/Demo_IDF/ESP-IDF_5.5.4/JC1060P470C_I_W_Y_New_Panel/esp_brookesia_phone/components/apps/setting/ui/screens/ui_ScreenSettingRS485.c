// RS485 Test Screen – all children directly on root, no nesting
//
// Keyboard behaviour:
//   - Always rendered (no HIDDEN flag at init) with explicit opaque background.
//   - Clicking the textarea → brings keyboard to foreground.
//   - Clicking ANY other widget → hides keyboard.
//
// Every panel is given LV_OBJ_FLAG_CLICKABLE so that LV_EVENT_CLICKED is
// generated for it (LVGL objects are NOT clickable by default).

#include "../ui.h"

/* ── Show keyboard when textarea is clicked ── */
static void on_ta_show_kb(lv_event_t *e)
{
    lv_obj_clear_flag(ui_KBRS485, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_KBRS485);
}

/* ── Hide keyboard when anything else is clicked ── */
static void on_hide_kb(lv_event_t *e)
{
    lv_obj_t *t = lv_event_get_target(e);
    if (t == ui_TextAreaRS485Input) {
        return; // the textarea handler already does show
    }
    lv_obj_add_flag(ui_KBRS485, LV_OBJ_FLAG_HIDDEN);
}

/* ── Keyboard "hide" key (LV_SYMBOL_KEYBOARD / LV_SYMBOL_CLOSE) ── */
static void on_kb_cancel(lv_event_t *e)
{
    lv_obj_add_flag(ui_KBRS485, LV_OBJ_FLAG_HIDDEN);
}

void ui_RS485_ConsoleAppend(const char *txt)
{
    if (!ui_LabelRS485Console || !txt) return;
    const char *old = lv_label_get_text(ui_LabelRS485Console);
    size_t olen = strlen(old);
    size_t nlen = strlen(txt);
    if (olen + nlen > 4000) {
        const char *half = old + olen / 2;
        while (*half && *half != '\n') half++;
        if (*half) half++;
        old = half;
    }
    char *buf = (char *)malloc(4096);
    if (!buf) return;
    snprintf(buf, 4096, "%s%s", old, txt);
    lv_label_set_text(ui_LabelRS485Console, buf);
    free(buf);
    lv_obj_scroll_to_y(lv_obj_get_parent(ui_LabelRS485Console), LV_COORD_MAX, LV_ANIM_OFF);
}

void ui_RS485_ConsoleClear(void)
{
    if (!ui_LabelRS485Console) return;
    lv_label_set_text(ui_LabelRS485Console, "RS485 Console Ready\n--------------------\n");
    lv_obj_scroll_to_y(lv_obj_get_parent(ui_LabelRS485Console), 0, LV_ANIM_OFF);
}

void ui_ScreenSettingRS485_screen_init(void)
{
    ui_ScreenSettingRS485 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenSettingRS485, lv_color_hex(0xF5F5F5), 0);
    lv_obj_clear_flag(ui_ScreenSettingRS485, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_ScreenSettingRS485, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ScreenSettingRS485, on_hide_kb, LV_EVENT_CLICKED, NULL);

    /* ─── Title bar ─── */
    ui_PanelRS485Title = lv_obj_create(ui_ScreenSettingRS485);
    lv_obj_set_pos(ui_PanelRS485Title, 0, 0);
    lv_obj_set_size(ui_PanelRS485Title, 1024, 50);
    lv_obj_clear_flag(ui_PanelRS485Title, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_PanelRS485Title, 0, 0);
    lv_obj_set_style_border_width(ui_PanelRS485Title, 0, 0);
    lv_obj_set_style_bg_color(ui_PanelRS485Title, lv_color_hex(0x1976D2), 0);
    lv_obj_add_flag(ui_PanelRS485Title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelRS485Title, on_hide_kb, LV_EVENT_CLICKED, NULL);

    ui_LabelRS485Title = lv_label_create(ui_PanelRS485Title);
    lv_label_set_text(ui_LabelRS485Title, "RS485 Test");
    lv_obj_set_style_text_font(ui_LabelRS485Title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ui_LabelRS485Title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(ui_LabelRS485Title, LV_ALIGN_LEFT_MID, 15, 0);

    ui_LabelRS485Status = lv_label_create(ui_PanelRS485Title);
    lv_label_set_text(ui_LabelRS485Status, "Status: Idle");
    lv_obj_set_style_text_font(ui_LabelRS485Status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui_LabelRS485Status, lv_color_hex(0xBBDEFB), 0);
    lv_obj_align(ui_LabelRS485Status, LV_ALIGN_RIGHT_MID, -15, 0);

    /* ─── Left panel (x0..306, y50..600) ─── */
    ui_PanelRS485Send = lv_obj_create(ui_ScreenSettingRS485);
    lv_obj_set_pos(ui_PanelRS485Send, 0, 50);
    lv_obj_set_size(ui_PanelRS485Send, 306, 550);
    lv_obj_clear_flag(ui_PanelRS485Send, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_PanelRS485Send, 0, 0);
    lv_obj_set_style_border_width(ui_PanelRS485Send, 0, 0);
    lv_obj_set_style_bg_color(ui_PanelRS485Send, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_flag(ui_PanelRS485Send, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_PanelRS485Send, on_hide_kb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *inp = lv_label_create(ui_PanelRS485Send);
    lv_label_set_text(inp, "Input:");
    lv_obj_set_style_text_font(inp, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(inp, lv_color_hex(0x424242), 0);
    lv_obj_set_pos(inp, 15, 15);

    ui_TextAreaRS485Input = lv_textarea_create(ui_PanelRS485Send);
    lv_obj_set_pos(ui_TextAreaRS485Input, 15, 40);
    lv_obj_set_size(ui_TextAreaRS485Input, 260, 40);
    lv_textarea_set_placeholder_text(ui_TextAreaRS485Input, "Enter text...");
    lv_textarea_set_text(ui_TextAreaRS485Input, "Hello,Guition");
    lv_textarea_set_max_length(ui_TextAreaRS485Input, 128);
    lv_textarea_set_one_line(ui_TextAreaRS485Input, true);
    lv_obj_set_style_text_font(ui_TextAreaRS485Input, &lv_font_montserrat_18, 0);
    lv_obj_set_style_radius(ui_TextAreaRS485Input, 6, 0);
    lv_obj_set_style_border_color(ui_TextAreaRS485Input, lv_color_hex(0xB0BEC5), 0);
    lv_obj_add_event_cb(ui_TextAreaRS485Input, on_ta_show_kb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaRS485Input, on_ta_show_kb, LV_EVENT_FOCUSED, NULL);

    ui_ButtonRS485Send = lv_btn_create(ui_PanelRS485Send);
    lv_obj_set_pos(ui_ButtonRS485Send, 30, 250);
    lv_obj_set_size(ui_ButtonRS485Send, 246, 56);
    lv_obj_set_style_radius(ui_ButtonRS485Send, 28, 0);
    lv_obj_set_style_bg_color(ui_ButtonRS485Send, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_bg_color(ui_ButtonRS485Send, lv_color_hex(0x1565C0), LV_STATE_PRESSED);
    lv_obj_add_flag(ui_ButtonRS485Send, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_ButtonRS485Send, on_hide_kb, LV_EVENT_CLICKED, NULL);

    ui_LabelRS485SendButton = lv_label_create(ui_ButtonRS485Send);
    lv_label_set_text(ui_LabelRS485SendButton, "Send");
    lv_obj_set_style_text_font(ui_LabelRS485SendButton, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(ui_LabelRS485SendButton, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(ui_LabelRS485SendButton);

    /* ─── Separator ─── */
    lv_obj_t *sep = lv_obj_create(ui_ScreenSettingRS485);
    lv_obj_set_pos(sep, 306, 50);
    lv_obj_set_size(sep, 2, 550);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sep, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sep, on_hide_kb, LV_EVENT_CLICKED, NULL);

    /* Console container — created BEFORE header/clear so it sits behind them */
    lv_obj_t *con = lv_obj_create(ui_ScreenSettingRS485);
    lv_obj_set_pos(con, 308, 95);
    lv_obj_set_size(con, 700, 450);
    lv_obj_set_style_bg_color(con, lv_color_hex(0x263238), 0);
    lv_obj_set_style_radius(con, 8, 0);
    lv_obj_set_style_border_width(con, 0, 0);
    lv_obj_set_style_pad_all(con, 6, 0);
    lv_obj_set_scroll_dir(con, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(con, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(con, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(con, on_hide_kb, LV_EVENT_CLICKED, NULL);

    ui_LabelRS485Console = lv_label_create(con);
    lv_obj_set_width(ui_LabelRS485Console, 700);
    lv_obj_align(ui_LabelRS485Console, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(ui_LabelRS485Console, "RS485 Console Ready\n--------------------\n");
    lv_obj_set_style_text_font(ui_LabelRS485Console, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelRS485Console, lv_color_hex(0x00E676), 0);

   

    ui_BtnRS485Clear = lv_btn_create(ui_ScreenSettingRS485);
    lv_obj_set_pos(ui_BtnRS485Clear, 930, 56);
    lv_obj_set_size(ui_BtnRS485Clear, 80, 26);
    lv_obj_set_style_radius(ui_BtnRS485Clear, 13, 0);
    lv_obj_set_style_bg_color(ui_BtnRS485Clear, lv_color_hex(0xEF5350), 0);
    lv_obj_set_style_bg_color(ui_BtnRS485Clear, lv_color_hex(0xC62828), LV_STATE_PRESSED);
    lv_obj_clear_flag(ui_BtnRS485Clear, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_BtnRS485Clear, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_BtnRS485Clear, on_hide_kb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cl = lv_label_create(ui_BtnRS485Clear);
    lv_label_set_text(cl, LV_SYMBOL_TRASH " Clear");
    lv_obj_set_style_text_font(cl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(cl);

    /* ─── Keyboard — overlays the panels at the bottom ─── */
    ui_KBRS485 = lv_keyboard_create(ui_ScreenSettingRS485);
    lv_obj_set_size(ui_KBRS485, 800, 180);
    lv_obj_align(ui_KBRS485, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(ui_KBRS485, ui_TextAreaRS485Input);
    lv_keyboard_set_mode(ui_KBRS485, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_clear_flag(ui_KBRS485, LV_OBJ_FLAG_SCROLLABLE);
    /* Keep LVGL default button spacing (don't force zero padding/gap) */
    lv_obj_set_style_bg_color(ui_KBRS485, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_bg_opa(ui_KBRS485, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ui_KBRS485, lv_color_hex(0xF0F0F0), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(ui_KBRS485, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(ui_KBRS485, lv_color_hex(0x000000), LV_PART_ITEMS);
    lv_obj_add_event_cb(ui_KBRS485, on_kb_cancel, LV_EVENT_CANCEL, NULL);
}
