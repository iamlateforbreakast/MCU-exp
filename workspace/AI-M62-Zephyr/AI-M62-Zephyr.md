# AI-Thinker M62 Zephyr development

The `aim62-zephyr-dev` container (see `Dockerfile.aim62-zephyr` / `compose.yaml`) provides
Zephyr RTOS (pinned to `v4.4.0`, the first tagged release containing `ai_m62_12f_kit` board
support) and the `riscv64-zephyr-elf` Zephyr SDK toolchain, targeting
the Zephyr board `ai_m62_12f_kit` - the Ai-Thinker M62-12F Kit (BL616, RISC-V). This is a
separate container from `aim62-dev` (`Dockerfile.aim62`), which targets the same MCU family
through the vendor's own Ai-M6X SDK instead of Zephyr.

`west`, its Python venv, and the full Zephyr workspace (`~/zephyrproject`) live inside the
image, not in the mounted `workspace/` directory. Only application sources (like the example
below) go under `~/workspace`.

## Usage

```
podman compose up -d aim62-zephyr-dev
podman exec -it aim62-zephyr-dev /bin/bash
```

The Zephyr venv is activated automatically for interactive shells, so `west` is available
directly.

## Building an application

`west build` needs to run from inside the west workspace (`~/zephyrproject`), with the
mounted application passed as the source directory and its build output directed back
under the mounted workspace so it's visible on the host:

```
cd ~/zephyrproject
west build -b ai_m62_12f_kit -d ~/workspace/examples/gpio_led_blink/build ~/workspace/examples/gpio_led_blink
```

## GPIO LED example

`examples/gpio_led_blink` drives the kit's five onboard LEDs (blue/green/red/white/warm
white) in sequence, using Zephyr's `gpio_dt_spec` API against the board's own devicetree
node labels (`blue_led`, `green_led`, etc., from `ai_m62_12f_kit.dts` upstream in Zephyr) -
grounded against the actual onboard wiring of this exact kit, not placeholder pins:

```
cd ~/zephyrproject
west build -b ai_m62_12f_kit -d ~/workspace/examples/gpio_led_blink/build ~/workspace/examples/gpio_led_blink
```

## Flashing

Pass the board's USB-serial device through via the commented `devices` entry in
`compose.yaml`, then flash from the build directory:

```
west flash -d ~/workspace/examples/gpio_led_blink/build
```

Flashing uses the `bflb_mcu_tool` runner (installed into the venv via pip) talking to the
BL616's UART bootloader over `/dev/ttyUSB0`. If `west flash` can't trigger the bootloader
automatically, put the board in download mode manually (hold BOOT, tap RESET, release BOOT)
before running it.

Monitor over the same port once it's running:

```
screen /dev/ttyUSB0 115200
```

## Firmware output

`compose.yaml` also mounts a separate `firmware/AI-M62-Zephyr` host directory to
`~/firmware` inside the container - copy the built image there so it's easy to find on the
host:

```
cp ~/workspace/examples/gpio_led_blink/build/zephyr/zephyr.bin ~/firmware/
```
