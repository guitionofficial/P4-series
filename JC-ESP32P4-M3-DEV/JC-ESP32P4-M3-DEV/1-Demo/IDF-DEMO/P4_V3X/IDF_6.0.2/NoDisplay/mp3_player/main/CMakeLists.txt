idf_component_register(SRCS "mp3_player.c"
                    INCLUDE_DIRS "."
                    REQUIRES bsp_extra esp-audio-player espressif__esp32_p4_function_ev_board esp_driver_gpio esp_driver_i2s esp_driver_ledc fatfs spiffs)
