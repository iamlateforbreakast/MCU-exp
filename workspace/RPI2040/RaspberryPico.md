
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

## BMP280 sensor example

`examples/bmp280_i2c` reads temperature and pressure from a Bosch BMP280 over I2C
(`hardware/i2c.h`) and prints them over USB serial every 500ms:

```
cd ~/workspace/examples/bmp280_i2c
cmake -S . -B build -G Ninja
cmake --build build
```

Wire SDA to GPIO4, SCL to GPIO5, plus 3V3/GND (matching the official
`pico-examples/i2c/bmp280_i2c` pinout). The default I2C address (`BMP280_I2C_ADDR` in
`bmp280_i2c.c`) is `0x76`; change it to `0x77` if your breakout board ties the sensor's
SDO pin high instead of low. After flashing, connect at any baud rate (e.g.
`minicom -D /dev/ttyACM0`) to see the readings - USB CDC ignores the actual baud
setting.

**If you see `0.00`/`0.00` (or nothing) instead of real readings:** the sensor isn't
ACKing on the I2C bus. The example now checks every I2C transaction and retries with a
loud, repeating error - `BMP280 not responding at I2C address 0x76 ...` - instead of
silently continuing with uninitialized data, so reconnect your terminal and check for
that message. Most commonly this means the address is actually `0x77` (try changing
`BMP280_I2C_ADDR`), or SDA/SCL need external ~4.7k pull-ups.

## Flashing

Hold BOOTSEL while plugging in the board (or while resetting it) so it mounts as a USB mass
storage device, then copy the built `.uf2` file onto it:

```
cp build/hello_world.uf2 /media/$USER/RPI-RP2/
```

## Firmware output

`compose.yaml` also mounts a separate `firmware/RPI2040` host directory to `~/firmware`
inside the container - copy a built `.uf2` there so it's easy to find on the host, e.g.:

```
cp build/gpio_led_blink.uf2 ~/firmware/
cp build/bmp280_i2c.uf2 ~/firmware/
```

# Install SDK (manual, outside the container)

`git clone https://github.com/raspberrypi/pico-sdk.git`
