// RS485 Test Screen
// Top: Send area (compact), Bottom: Receive terminal (fills remaining, never overflows)

#include "../ui.h"

/* ── Keyboard show/hide callbacks ── */
static void on_ta_focus(lv_event_t *e)
{
    lv_obj_clear_flag(ui_KBRS485, LV_OBJ_FLAG_HIDDEN);
}
static void on_ta_defocus(lv_event_t *e)
{
    lv_obj_add_flag(ui_KBRS485, LV_OBJ_FLAG_HIDDEN);
}

void ui_RS485_ConsoleAppend(const char *txt)
{
    if (ui_LabelRS485Console == NULL || txt == NULL) return;
    const char *old_text = lv_label_get_text(ui_LabelRS485Console);
    size_t old_len = strlen(old_text);
    size_t new_len = strlen(txt);
    // If total would exceed 4KB, keep only the tail (discard first half)
    if (old_len + new_len > 4000) {
        const char *half = old_text + old_len / 2;
        // Skip to next newline for clean cut
        while (*half && *half != '\n') half++;
        if (*half) half++;
        old_text = half;
        old_len = strlen(old_text);
    }
    /* Use heap instead of stack to avoid overflowing the RS485 task's 4K stack */
    char *new_buf = malloc(4096);
    if (new_buf == NULL) return;
    snprintf(new_buf, 4096, "%s%s", old_text, txt);
    lv_label_set_text(ui_LabelRS485Console, new_buf);
    free(new_buf);
    /* 自动滚动到最底部，让最新内容始终可见 */
    lv_obj_scroll_to_y(lv_obj_get_parent(ui_LabelRS485Console), LV_COORD_MAX, LV_ANIM_OFF);
}

void ui_RS485_ConsoleClear(void)
{
    if (ui_LabelRS485Console == NULL) return;
    lv_label_set_text(ui_LabelRS485Console, "RS485 Console Ready\n--------------------\n");
    lv_obj_scroll_to_y(lv_obj_get_parent(ui_LabelRS485Console), 0, LV_ANIM_OFF);
}

void ui_ScreenSettingRS485_screen_init(void)
{
    /* ══════════════════════════════════════════════════
       Root screen — flex column so all children fit
       within bounds without manual position math.
       ══════════════════════════════════════════════════ */
    ui_ScreenSettingRS485 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenSettingRS485, lv_color_hex(0xF5F5F5), 0);
    lv_obj_clear_flag(ui_ScreenSettingRS485, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ui_ScreenSettingRS485, LV_FLEX_FLOW_COLUMN);

    /* ═══════ Title bar (fixed height) ═══════ */
    ui_PanelRS485Title = lv_obj_create(ui_ScreenSettingRS485);
    lv_obj_set_width(ui_PanelRS485Title, lv_pct(100));
    lv_obj_set_height(ui_PanelRS485Title, 50);
    lv_obj_clear_flag(ui_PanelRS485Title, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_PanelRS485Title, 0, 0);
    lv_obj_set_style_border_width(ui_PanelRS485Title, 0, 0);
    lv_obj_set_style_bg_color(ui_PanelRS485Title, lv_color_hex(0x1976D2), 0);
    lv_obj_set_style_pad_all(ui_PanelRS485Title, 0, 0);

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

    /* ═══════ Main area (flex-grow fills everything below title) ═══════ */
    lv_obj_t *main_area = lv_obj_create(ui_ScreenSettingRS485);
    lv_obj_set_width(main_area, lv_pct(100));
    lv_obj_set_flex_grow(main_area, 1);
    lv_obj_set_style_radius(main_area, 0, 0);
    lv_obj_set_style_border_width(main_area, 0, 0);
    lv_obj_set_style_pad_all(main_area, 0, 0);
    lv_obj_set_style_bg_opa(main_area, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(main_area, LV_FLEX_FLOW_COLUMN);

    /* ═══════ Send area (top — fixed height) ═══════ */
    ui_PanelRS485Send = lv_obj_create(main_area);
    lv_obj_set_width(ui_PanelRS485Send, lv_pct(100));
    lv_obj_set_height(ui_PanelRS485Send, 90);
    lv_obj_set_style_radius(ui_PanelRS485Send, 0, 0);
    lv_obj_set_style_border_width(ui_PanelRS485Send, 0, 0);
    lv_obj_set_style_bg_color(ui_PanelRS485Send, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(ui_PanelRS485Send, 6, 0);
    lv_obj_clear_flag(ui_PanelRS485Send, LV_OBJ_FLAG_SCROLLABLE);

    /* "Input:" label */
    lv_obj_t *input_label = lv_label_create(ui_PanelRS485Send);
    lv_label_set_text(input_label, "Input:");
    lv_obj_set_style_text_font(input_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(input_label, lv_color_hex(0x424242), 0);

    /* Row: textarea + send button side by side */
    lv_obj_t *send_row = lv_obj_create(ui_PanelRS485Send);
    lv_obj_set_width(send_row, lv_pct(100));
    lv_obj_set_height(send_row, 40);
    lv_obj_set_style_bg_opa(send_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(send_row, 0, 0);
    lv_obj_set_style_pad_all(send_row, 0, 0);
    lv_obj_clear_flag(send_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(send_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(send_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_TextAreaRS485Input = lv_textarea_create(send_row);
    lv_obj_set_height(ui_TextAreaRS485Input, 40);
    lv_obj_set_flex_grow(ui_TextAreaRS485Input, 1);
    lv_textarea_set_placeholder_text(ui_TextAreaRS485Input, "Enter text...");
    lv_textarea_set_text(ui_TextAreaRS485Input, "Hello,Guition");
    lv_textarea_set_max_length(ui_TextAreaRS485Input, 128);
    lv_textarea_set_one_line(ui_TextAreaRS485Input, true);
    lv_obj_set_style_text_font(ui_TextAreaRS485Input, &lv_font_montserrat_18, 0);
    lv_obj_set_style_radius(ui_TextAreaRS485Input, 6, 0);
    lv_obj_set_style_border_color(ui_TextAreaRS485Input, lv_color_hex(0xB0BEC5), 0);
    lv_obj_add_event_cb(ui_TextAreaRS485Input, on_ta_focus, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ui_TextAreaRS485Input, on_ta_defocus, LV_EVENT_DEFOCUSED, NULL);

    /* 8px gap */
    lv_obj_t *spacer = lv_obj_create(send_row);
    lv_obj_set_width(spacer, 8);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    ui_ButtonRS485Send = lv_btn_create(send_row);
    lv_obj_set_size(ui_ButtonRS485Send, 80, 40);
    lv_obj_set_style_radius(ui_ButtonRS485Send, 6, 0);
    lv_obj_set_style_bg_color(ui_ButtonRS485Send, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_bg_color(ui_ButtonRS485Send, lv_color_hex(0x1565C0), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui_ButtonRS485Send, 4, 0);
    lv_obj_set_style_shadow_color(ui_ButtonRS485Send, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_shadow_opa(ui_ButtonRS485Send, LV_OPA_40, 0);
    lv_obj_set_style_shadow_ofs_y(ui_ButtonRS485Send, 3, 0);
    lv_obj_add_flag(ui_ButtonRS485Send, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_ButtonRS485Send, LV_OBJ_FLAG_SCROLLABLE);

    ui_LabelRS485SendButton = lv_label_create(ui_ButtonRS485Send);
    lv_label_set_text(ui_LabelRS485SendButton, "Send");
    lv_obj_set_style_text_font(ui_LabelRS485SendButton, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui_LabelRS485SendButton, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(ui_LabelRS485SendButton);

    /* ═══════ Receive terminal (bottom — flex-grow fills, never overflows) ═══════ */
    ui_PanelRS485Console = lv_obj_create(main_area);
    lv_obj_set_width(ui_PanelRS485Console, lv_pct(100));
    lv_obj_set_flex_grow(ui_PanelRS485Console, 1);
    lv_obj_set_style_radius(ui_PanelRS485Console, 0, 0);
    lv_obj_set_style_border_width(ui_PanelRS485Console, 0, 0);
    lv_obj_set_style_bg_color(ui_PanelRS485Console, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_pad_all(ui_PanelRS485Console, 6, 0);
    lv_obj_set_flex_flow(ui_PanelRS485Console, LV_FLEX_FLOW_COLUMN);

    /* Header row: "Receive" label + Clear button */
    lv_obj_t *hd_row = lv_obj_create(ui_PanelRS485Console);
    lv_obj_set_width(hd_row, lv_pct(100));
    lv_obj_set_height(hd_row, 28);
    lv_obj_set_style_bg_opa(hd_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hd_row, 0, 0);
    lv_obj_set_style_pad_all(hd_row, 0, 0);
    lv_obj_clear_flag(hd_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hd_lbl = lv_label_create(hd_row);
    lv_label_set_text(hd_lbl, "Receive");
    lv_obj_set_style_text_font(hd_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hd_lbl, lv_color_hex(0x424242), 0);
    lv_obj_align(hd_lbl, LV_ALIGN_LEFT_MID, 4, 0);

    ui_BtnRS485Clear = lv_btn_create(hd_row);
    lv_obj_set_size(ui_BtnRS485Clear, 75, 24);
    lv_obj_set_style_radius(ui_BtnRS485Clear, 12, 0);
    lv_obj_set_style_bg_color(ui_BtnRS485Clear, lv_color_hex(0xEF5350), 0);
    lv_obj_set_style_bg_color(ui_BtnRS485Clear, lv_color_hex(0xC62828), LV_STATE_PRESSED);
    lv_obj_clear_flag(ui_BtnRS485Clear, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_BtnRS485Clear, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(ui_BtnRS485Clear, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_t *clr_lbl = lv_label_create(ui_BtnRS485Clear);
    lv_label_set_text(clr_lbl, LV_SYMBOL_TRASH " Clear");
    lv_obj_set_style_text_font(clr_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(clr_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(clr_lbl);

    /* Console terminal — flex-grow fills remaining height within its flex parent.
       Parent is in a flex-column, so this height is naturally bounded. */
    lv_obj_t *console = lv_obj_create(ui_PanelRS485Console);
    lv_obj_set_width(console, lv_pct(100));
    lv_obj_set_flex_grow(console, 1);
    lv_obj_set_style_bg_color(console, lv_color_hex(0x263238), 0);
    lv_obj_set_style_radius(console, 8, 0);
    lv_obj_set_style_border_width(console, 0, 0);
    lv_obj_set_style_pad_all(console, 6, 0);
    lv_obj_set_scroll_dir(console, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(console, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_clip_corner(console, true, 0);

    ui_LabelRS485Console = lv_label_create(console);
    lv_obj_set_width(ui_LabelRS485Console, lv_pct(100));
    lv_obj_align(ui_LabelRS485Console, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(ui_LabelRS485Console, "RS485 Console Ready\n--------------------\n");
    lv_obj_set_style_text_font(ui_LabelRS485Console, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_LabelRS485Console, lv_color_hex(0x00E676), 0);
    lv_label_set_long_mode(ui_LabelRS485Console, LV_LABEL_LONG_WRAP);

    /* ─── Keyboard ─── */
    ui_KBRS485 = lv_keyboard_create(ui_ScreenSettingRS485);
    lv_obj_set_size(ui_KBRS485, lv_pct(100), 180);
    lv_obj_align(ui_KBRS485, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(ui_KBRS485, ui_TextAreaRS485Input);
    lv_keyboard_set_mode(ui_KBRS485, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_clear_flag(ui_KBRS485, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_KBRS485, LV_OBJ_FLAG_HIDDEN);
}
