cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(bmp280_i2c C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(bmp280_i2c bmp280_i2c.c)
pico_add_extra_outputs(bmp280_i2c)
pico_enable_stdio_usb(bmp280_i2c 1)
pico_enable_stdio_uart(bmp280_i2c 0)
target_link_libraries(bmp280_i2c pico_stdlib hardware_i2c)
