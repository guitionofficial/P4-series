# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

 

set(EXCLUDE_COMPONENTS espressif__esp-brookesia)

add_compile_options(-Wno-format)
add_compile_options(-Wno-ignored-qualifiers)
project(lvgl_demo_v9)

# Fix: esp-audio-player v2.0.0 switch doesn't handle new HTTP_EVENT enum values in IDF 6.x
idf_component_get_property(audio_player_lib chmorgan__esp-audio-player COMPONENT_LIB)
target_compile_options(${audio_player_lib} PRIVATE -Wno-switch)


