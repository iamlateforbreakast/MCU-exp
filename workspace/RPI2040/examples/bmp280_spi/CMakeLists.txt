cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(bmp280_spi C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(bmp280_spi bmp280_spi.c)
pico_add_extra_outputs(bmp280_spi)
pico_enable_stdio_usb(bmp280_spi 1)
pico_enable_stdio_uart(bmp280_spi 0)
target_link_libraries(bmp280_spi pico_stdlib hardware_spi)
