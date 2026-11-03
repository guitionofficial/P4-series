idf_component_register(
    SRCS main.c draw_app.c
    INCLUDE_DIRS .
    REQUIRES lvgl esp_lcd_touch esp_lcd_touch_gt911 esp32_p4_function_ev_board)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
set(EXTRA_COMPONENT_DIRS
    ./common_components
    )

target_compile_options(
    ${LVGL_LIB} 
    PRIVATE 
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)
