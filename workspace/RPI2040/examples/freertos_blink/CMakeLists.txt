cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(freertos_blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

if(NOT FREERTOS_KERNEL_PATH AND NOT DEFINED ENV{FREERTOS_KERNEL_PATH})
    message(FATAL_ERROR "FREERTOS_KERNEL_PATH is not set - see Dockerfile.rp2040, "
                         "which clones it to ~/FreeRTOS-Kernel and exports the env var")
endif()
include($ENV{FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

add_executable(freertos_blink freertos_blink.c)
target_include_directories(freertos_blink PRIVATE ${CMAKE_CURRENT_LIST_DIR})
pico_add_extra_outputs(freertos_blink)
pico_enable_stdio_usb(freertos_blink 1)
pico_enable_stdio_uart(freertos_blink 0)

# FreeRTOS-Kernel-Heap4: dynamic (malloc-style) allocation - this example
# doesn't need the static-allocation variant the official example also shows.
target_link_libraries(freertos_blink pico_stdlib FreeRTOS-Kernel-Heap4)
