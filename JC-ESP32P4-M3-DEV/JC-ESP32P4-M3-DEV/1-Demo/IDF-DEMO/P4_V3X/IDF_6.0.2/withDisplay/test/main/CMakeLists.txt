# 完整模式: LCD/LVGL + 以太网/WiFi + 摄像头检测 (app_enthernet.c 提供 test_enthernet_init)
idf_component_register(SRCS "app_camera.c" "app_enthernet.c" "test.c"
                    INCLUDE_DIRS "." "include")
