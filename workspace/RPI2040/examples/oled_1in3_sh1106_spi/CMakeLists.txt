cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(oled_1in3_sh1106_spi C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(oled_1in3_sh1106_spi oled_1in3_sh1106_spi.c)
pico_add_extra_outputs(oled_1in3_sh1106_spi)
pico_enable_stdio_usb(oled_1in3_sh1106_spi 1)
pico_enable_stdio_uart(oled_1in3_sh1106_spi 0)
target_link_libraries(oled_1in3_sh1106_spi pico_stdlib hardware_spi)
