# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
    ../common_components
    )

# esp-brookesia only supports lvgl >=8.3,<9, while this project uses lvgl 9.x.
# Exclude it so that the component manager can resolve the lvgl version.
set(EXCLUDE_COMPONENTS espressif__esp-brookesia)

add_compile_options(-Wno-format)
add_compile_options(-Wno-ignored-qualifiers)
project(lvgl_demo_v9)
