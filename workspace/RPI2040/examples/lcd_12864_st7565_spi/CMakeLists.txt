cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(lcd_12864_st7565_spi C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(lcd_12864_st7565_spi lcd_12864_st7565_spi.c)
pico_add_extra_outputs(lcd_12864_st7565_spi)
pico_enable_stdio_usb(lcd_12864_st7565_spi 1)
pico_enable_stdio_uart(lcd_12864_st7565_spi 0)
target_link_libraries(lcd_12864_st7565_spi pico_stdlib hardware_spi)
