# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
    ./components   
)
# Use the locally maintained copies instead of the ones from the registry:
# - chmorgan/esp-audio-player (i2s_std driver fix)
# - waveshare/esp_lcd_jd9365_10_1 (skip LCD/touch when not detected)
list(APPEND OVERRIDE_COMPONENTS chmorgan__esp-audio-player)
list(APPEND OVERRIDE_COMPONENTS waveshare__esp_lcd_jd9365_10_1)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test)
