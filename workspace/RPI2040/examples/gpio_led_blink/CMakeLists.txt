cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(gpio_led_blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(gpio_led_blink gpio_led_blink.c)
pico_add_extra_outputs(gpio_led_blink)
target_link_libraries(gpio_led_blink pico_stdlib)
