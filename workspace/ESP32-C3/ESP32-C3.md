# ESP32-C3 development

The `esp32c3-dev` container (see `Containerfile.esp32c3` / `compose.yaml`) provides a RISC-V
ESP-IDF toolchain scoped to the ESP32-C3 target.

## Usage

```
podman compose up -d esp32c3-dev
podman exec -it esp32c3-dev /bin/bash
```

`esp-idf/export.sh` is sourced automatically for interactive shells, so `idf.py` is available
directly.

## Creating and building a project

```
cp -r $IDF_PATH/examples/get-started/hello_world ~/workspace/hello_world
cd ~/workspace/hello_world
idf.py set-target esp32c3
idf.py build
```

## GPIO LED example

`examples/gpio_led_blink` drives three LEDs in sequence using `driver/gpio.h`. Build and
flash it like any ESP-IDF project:

```
cd ~/workspace/examples/gpio_led_blink
idf.py set-target esp32c3
idf.py -p /dev/ttyACM0 flash monitor
```

Adjust the `LED_GPIO_*` pin numbers in `main/gpio_led_blink.c` to match your wiring.

## SH1106 OLED example

`examples/oled_1in3_sh1106_spi` drives a 1.3" 128x64 monochrome SH1106 OLED over SPI
(`driver/spi_master.h`), drawing a border plus a small square scanning back and forth.
Confirmed working on real hardware. Wiring and pin assignments are documented in that
example's own file header - build and flash the same way as `gpio_led_blink` above.

This is also the known-working counterpart to
`../ESP32-C3-RTEMS/examples/oled_1in3_sh1106_spi`: the draft RTEMS GP-SPI2 driver deadlocks
on every real SPI transaction on this same hardware (see
`../ESP32-C3-RTEMS/upstream-spi-driver/README.md`), so this ESP-IDF version exists to
actually drive the display while that's unresolved.

## Flashing and monitoring

The ESP32-C3 exposes a native USB-CDC serial port (typically `/dev/ttyACM0` on Linux).
`compose.yaml`'s `devices` entry passes it through to the container, and `idf.py build`
inside the container works fine - but `idf.py flash` from *inside* the container has been
seen to fail with a permission-denied error on `/dev/ttyACM0` despite the passthrough
(likely an SELinux device-labeling issue with raw device nodes, unlike bind-mounted
directories which get relabeled via the `:z`/`:U` volume flags already in use elsewhere in
this compose file). The reliable path: build inside the container, then flash from the
host directly with the plain `esptool.py` command `idf.py build` prints at the end of its
output (three files - bootloader, partition table, and the app image - each at its own flash
offset), since the project directory is bind-mounted and so the `build/` output is visible
on the host too:

```
cd workspace/ESP32-C3/examples/<example>/build
esptool.py --chip esp32c3 -p /dev/ttyACM0 -b 460800 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 2MB \
  0x0 bootloader/bootloader.bin 0x10000 <example>.bin 0x8000 partition_table/partition-table.bin
```

`idf.py monitor` (run from inside the container) still works fine for viewing serial output,
since that's a read path with no analogous SELinux friction observed.

## BLE controller probe

`ble_controller_probe/` is a real ESP-IDF v5.3.1 project used as a control experiment for
`../ESP32-C3-RTEMS/upstream-bt-driver/`'s BLE controller port - not a general-purpose
example. It confirmed a specific closed-blob commit works correctly on real hardware,
isolating a since-fixed assert to the RTEMS port's platform layer rather than the blob.
See its own `README.md` and `../ESP32-C3-RTEMS`'s `ESP32-C3-RTEMS.md` for the full story.

## Firmware output

`compose.yaml` also mounts a separate `firmware/ESP32-C3` host directory to `~/firmware`
inside the container - copy the built binary there so it's easy to find on the host:

```
cp build/gpio_led_blink.bin ~/firmware/
```
