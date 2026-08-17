cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(epaper_1in54_spi C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(epaper_1in54_spi epaper_1in54_spi.c)
pico_add_extra_outputs(epaper_1in54_spi)
pico_enable_stdio_usb(epaper_1in54_spi 1)
pico_enable_stdio_uart(epaper_1in54_spi 0)
target_link_libraries(epaper_1in54_spi pico_stdlib hardware_spi)
