cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(freertos_blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

if(NOT FREERTOS_KERNEL_PATH AND NOT DEFINED ENV{FREERTOS_KERNEL_PATH})
    message(FATAL_ERROR "FREERTOS_KERNEL_PATH is not set - see Containerfile.rp2040, "
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

# Same source, built with the RP2040 SMP port's scheduler spread across both
# cores instead of one - see the file header and FreeRTOSConfig.h for what
# changes (just configNUMBER_OF_CORES, via this compile definition).
add_executable(freertos_blink_dual_core freertos_blink.c)
target_include_directories(freertos_blink_dual_core PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(freertos_blink_dual_core PRIVATE configNUMBER_OF_CORES=2)
pico_add_extra_outputs(freertos_blink_dual_core)
pico_enable_stdio_usb(freertos_blink_dual_core 1)
pico_enable_stdio_uart(freertos_blink_dual_core 0)
target_link_libraries(freertos_blink_dual_core pico_stdlib FreeRTOS-Kernel-Heap4)
