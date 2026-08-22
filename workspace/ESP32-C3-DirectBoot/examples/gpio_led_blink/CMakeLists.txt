cmake_minimum_required(VERSION 3.12)

set(ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../)
set(CMAKE_TOOLCHAIN_FILE ${ROOT_DIR}/toolchain-rv32.cmake)

project(gpio_led_blink)

add_executable(gpio_led_blink gpio_led_blink.c)

add_subdirectory(${ROOT_DIR}/common common)
add_subdirectory(${ROOT_DIR}/hal hal)

target_link_libraries(gpio_led_blink PRIVATE common hal)

target_compile_options(gpio_led_blink PRIVATE -g -Og)
target_link_options(gpio_led_blink PRIVATE -g)
target_compile_options(gpio_led_blink PRIVATE -Wall -Werror -Wextra)

include(${ROOT_DIR}/utils.cmake)
add_linker_scripts(gpio_led_blink)
add_map_file(gpio_led_blink gpio_led_blink.map)
make_binary(gpio_led_blink gpio_led_blink.bin)
