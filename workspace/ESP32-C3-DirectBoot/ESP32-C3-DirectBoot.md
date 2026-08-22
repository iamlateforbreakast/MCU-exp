# ESP32-C3 direct boot development

The `esp32c3-directboot-dev` container (see `Dockerfile.esp32c3-directboot` / `compose.yaml`)
provides the `riscv-none-elf-gcc` toolchain and `esptool` needed to build and flash bare-metal
applications that run directly from flash on the ESP32-C3, using Espressif's "direct boot"
feature instead of ESP-IDF's 2nd stage bootloader.

Distinct from the other two ESP32-C3 containers in this repo:
- `esp32c3-dev` (`Dockerfile.esp32c3`) - full ESP-IDF/FreeRTOS stack, loaded via the 2nd
  stage bootloader.
- `esp32c3-rtems-dev` (`Dockerfile.esp32c3-rtems`) - RTEMS's `esp32c3db` BSP, also loaded
  via a 2nd stage bootloader.
- `esp32c3-directboot-dev` (this one) - no bootloader at all. The ROM bootloader jumps
  straight into the application in flash.

Everything under this directory (`common/`, `hal/`, `ld/`, `toolchain-rv32.cmake`,
`utils.cmake`) is vendored, with minor adjustments, from Espressif's
[esp32c3-direct-boot-example](https://github.com/espressif/esp32c3-direct-boot-example)
(MIT license, see `LICENSE.espressif`). See that repo's README for the full background on
how direct boot works. In short:

* The ROM bootloader looks for the magic number `0xaedb041d` repeated twice at the start of
  flash. If found (and secure boot / legacy-SPI-boot-disable eFuses allow it), it sets up the
  Flash MMU to map flash into the CPU address space and jumps to offset 8 in flash - no 2nd
  stage bootloader, no ESP-IDF binary image format.
* `common/start.S` provides the entry point: it sets up the global pointer and stack, zeroes
  `.bss`, copies `.data` from flash into RAM, points `mtvec` at the vector table in
  `common/vectors.S`, runs C library init, then calls `main()`.
* `ld/esp32c3/` places `.text`/`.rodata` at the flash-mapped IROM/DROM addresses and `.data`/
  `.bss`/the stack in RAM, and defines the ROM function addresses (`memcpy`, `memset`, ...)
  the example links against directly instead of pulling in a full libc.
* `hal/` fetches Espressif's `esp-hal-components` (via CMake `FetchContent`, at project
  configure time - requires network access from inside the container) for a minimal GPIO/WDT
  HAL, since there's no ESP-IDF here to provide one.

## Usage

```
podman compose up -d esp32c3-directboot-dev
podman exec -it esp32c3-directboot-dev /bin/bash
```

## Building the GPIO LED example

`examples/gpio_led_blink` drives three LEDs in sequence using the vendored `hal/gpio_hal.h`
(the same HAL the upstream `blink` example uses), matching this repo's other
`gpio_led_blink` examples. Build it with CMake, passing the target chip explicitly:

```
cd ~/workspace/examples/gpio_led_blink
mkdir -p build
cmake -S . -B build -D target=esp32c3 -G Ninja
cmake --build build
```

This produces `build/gpio_led_blink.bin`, ready to write straight to flash at offset 0 -
there's no bootloader or partition table to account for. Adjust the `LED_GPIO_*` pin numbers
in `gpio_led_blink.c` to match how you've actually wired the LEDs.

**Build-verified outside Docker**: the steps above were run directly on the host (plain
cmake/ninja, no container) against the same `riscv-none-elf-gcc` 12.2.0-3 release the
Dockerfile installs, and produced a `gpio_led_blink.bin` (7040 bytes text, 132 bytes data).
The container image itself hasn't been built (no Docker daemon in the sandbox this was
drafted in), and flashing/behavior on real hardware haven't been verified either - confirm
those before relying on this container.

## Flashing and monitoring

The ESP32-C3 exposes a native USB-CDC/JTAG serial port (typically `/dev/ttyACM0` on Linux).
Pass it through to the container via the `devices` entry in `compose.yaml`, then write the
binary directly to flash offset 0 with `esptool`:

```
esptool.py --port /dev/ttyACM0 --baud 921600 write_flash 0x0000 build/gpio_led_blink.bin
```

## Debugging using JTAG and GDB

Follow the same approach as upstream's `examples/blink/gdbinit`
(`examples/gpio_led_blink/gdbinit` here is adapted from it): install OpenOCD from
https://github.com/espressif/openocd-esp32/releases, add it to `PATH`, build the example,
then run:

```
riscv-none-elf-gdb -x gdbinit build/gpio_led_blink
```

This launches OpenOCD in pipe mode, flashes the program over JTAG, resets the target, sets a
temporary breakpoint at `main`, and runs until it's hit.

## Firmware output

`compose.yaml` also mounts a separate `firmware/ESP32-C3-DirectBoot` host directory to
`~/firmware` inside the container - copy the built binary there so it's easy to find on the
host:

```
cp build/gpio_led_blink.bin ~/firmware/
```
