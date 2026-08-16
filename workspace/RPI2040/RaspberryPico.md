
# RP2040 development

The `rp2040-dev` container (see `Dockerfile.rp2040` / `compose.yaml`) provides the
arm-none-eabi GCC toolchain and the Pico SDK (cloned to `$PICO_SDK_PATH`) for the RP2040.

## Usage

```
podman compose up -d rp2040-dev
podman exec -it rp2040-dev /bin/bash
```

## Building a project

```
cp -r $PICO_SDK_PATH/../pico-examples/hello_world ~/workspace/hello_world 2>/dev/null || true
cd ~/workspace/hello_world
cmake -S . -B build -G Ninja
cmake --build build
```

## GPIO LED example

`examples/gpio_led_blink` drives three LEDs in sequence using `pico/stdlib.h`:

```
cd ~/workspace/examples/gpio_led_blink
cmake -S . -B build -G Ninja
cmake --build build
```

Adjust the `LED_GPIO_*` pin numbers in `gpio_led_blink.c` to match your wiring (GPIO 25
is the onboard LED on a stock Pico/Pico W).

## Flashing

Hold BOOTSEL while plugging in the board (or while resetting it) so it mounts as a USB mass
storage device, then copy the built `.uf2` file onto it:

```
cp build/hello_world.uf2 /media/$USER/RPI-RP2/
```

## Firmware output

`compose.yaml` also mounts a separate `firmware/RPI2040` host directory to `~/firmware`
inside the container - copy the built `.uf2` there so it's easy to find on the host:

```
cp build/gpio_led_blink.uf2 ~/firmware/
```

# Install SDK (manual, outside the container)

`git clone https://github.com/raspberrypi/pico-sdk.git`
