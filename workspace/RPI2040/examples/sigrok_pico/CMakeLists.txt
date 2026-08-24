cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_sdk_sigrok C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Lets PulseView/sigrok-cli talk to the device without asserting DTR first -
# see https://github.com/pico-coder/sigrok-pico/pull/63.
add_definitions(-DPICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1)

pico_sdk_init()

add_executable(pico_sdk_sigrok
  pico_sdk_sigrok.c
  sr_device.c
)

pico_enable_stdio_usb(pico_sdk_sigrok 1)
pico_enable_stdio_uart(pico_sdk_sigrok 0)

pico_add_extra_outputs(pico_sdk_sigrok)

target_link_libraries(
  pico_sdk_sigrok
  pico_stdlib
  hardware_adc
  hardware_dma
  hardware_pio
  hardware_sync
  pico_multicore
  pico_stdio_usb
)
