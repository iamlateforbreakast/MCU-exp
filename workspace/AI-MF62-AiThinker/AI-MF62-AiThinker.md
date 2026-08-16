# Using the AI Thinker M62-M2-I-Kit
---------------------------------

# AI-M62 development container

The `aim62-dev` container (see `Dockerfile.aim62` / `compose.yaml`) provides a baseline
riscv64-unknown-elf toolchain plus the Ai-M6X SDK (cloned to `$AIM62_SDK_PATH`, submodules
initialized under `tools/`). Per the SDK's own convention, projects should use the cross
compiler from `$AIM62_SDK_PATH/tools` rather than the distro-installed one.

## Usage

```
podman compose up -d aim62-dev
podman exec -it aim62-dev /bin/bash
cd $AIM62_SDK_PATH/examples/helloworld
```

## GPIO LED example

`examples/gpio_led_blink` drives three LEDs in sequence using the `bflb_gpio` API
(following the SDK's own `examples/peripherals/gpio/gpio_input_output`):

```
cd ~/workspace/examples/gpio_led_blink
make BL_SDK_BASE=$AIM62_SDK_PATH CHIP=bl616 BOARD=bl616dk
```

`BL_SDK_BASE` is passed explicitly since this example lives outside the SDK tree,
unlike the SDK's own `examples/peripherals/...` (whose Makefile finds it via a
relative path). Adjust `CHIP`/`BOARD` to match your module, and `LED_PIN_*` in
`main.c` to match your wiring.

## Flashing

Pass the board's USB-serial device through via the commented `devices` entry in
`compose.yaml`.

## Firmware output

`compose.yaml` also mounts a separate `firmware/AI-MF62-AiThinker` host directory to
`~/firmware` inside the container - copy the built binary there so it's easy to find
on the host (the SDK's `make` puts output under `build/build_out/` next to the
project, e.g. `helloworld_bl616.bin` for the `helloworld` example):

```
cp build/build_out/*.bin ~/firmware/
```

---

# Manual setup notes (outside the container)

## Install toolchain

`sudo dnf install gcc-riscv64-linux-gnu-gnu`

## Install SDK

`git clone https://github.com/Ai-Thinker-Open/aithinker_Ai-M6X_SDK`

`cd tools`
`git submodule init`
`git submodule update .`

## Install tools

## Compile example

`cd exampls/helloworld`

cmake points to the tools directory insted of distro installed
cross compiler is riscv64-unknown-elf-gnu-gcc instead of distro



