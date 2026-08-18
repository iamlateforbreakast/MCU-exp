
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

## BMP280 sensor example (SPI)

`examples/bmp280_spi` is the SPI equivalent of `bmp280_i2c` - same sensor, same
compensation math, different transport (`hardware/spi.h`):

```
cd ~/workspace/examples/bmp280_spi
cmake -S . -B build -G Ninja
cmake --build build
```

Wire SCK to GPIO18, MOSI/SDI to GPIO19, MISO/SDO to GPIO16, CS to GPIO17, plus 3V3/GND
(matching the official `pico-examples/spi/bme280_spi` pinout - BME280 uses the same SPI
protocol and register map as BMP280). Unlike I2C, SPI has no ACK/NACK, so bad wiring
won't cause a clean failure - it'll just shift garbage bits - which is why the chip-id
readback is checked in the same loud, retrying loop as the I2C example: watch for
`BMP280 not responding over SPI ...` on the serial console if you're not seeing real
readings, and double-check the four SPI pins plus CS.

## WeAct 1.54" e-paper example (SPI)

`examples/epaper_1in54_spi` drives a WeAct Studio 1.54" e-paper module (SSD1681
controller, 200x200 monochrome) - draws two test patterns a few seconds apart, then
puts the panel to sleep:

```
cd ~/workspace/examples/epaper_1in54_spi
cmake -S . -B build -G Ninja
cmake --build build
```

Wire DIN to GPIO3, CLK to GPIO2, CS to GPIO1, DC to GPIO4, RST to GPIO5, BUSY to
GPIO6, plus **3.3V only** (5V will damage the panel, per WeAct's own module docs) and
GND. The init/update/sleep command sequence and BUSY pin polarity come from WeAct's own
reference driver for this exact module, not a generic e-paper guess.

Confirmed on real hardware: the RAM bit polarity (1=white/0=black) is correct as-is,
and WeAct's Y-axis inversion in `epd_set_pos` lands top-as-top correctly - but the X
axis came out mirrored, fixed in `fb_set_pixel` rather than in the SSD1681 command
sequence itself. If nothing draws at all, watch for the
`epd_init: timed out waiting for BUSY` message and check wiring first.

## FreeRTOS example

`examples/freertos_blink` runs FreeRTOS on the RP2040 (single core, dynamic
allocation): two independent tasks blink two LEDs at different rates, and a
producer/consumer task pair passes an incrementing tick count through a
queue, printed over USB serial:

```
cd ~/workspace/examples/freertos_blink
cmake -S . -B build -G Ninja
cmake --build build
```

Contrast with `examples/gpio_led_blink`: that example sequences LEDs from a
single bare-metal loop using `sleep_ms()`; here each LED is owned by its own
FreeRTOS task with its own period and priority, using `vTaskDelay()` so the
scheduler can run other tasks while one is waiting.

The `rp2040-dev` container clones the FreeRTOS kernel (including its RP2040
SMP port) to `$FREERTOS_KERNEL_PATH` at build time (see `Dockerfile.rp2040`)
- no extra setup needed inside the container. `FreeRTOSConfig.h` and the
task/queue setup in `freertos_blink.c` are grounded against the official
`pico-examples/freertos/hello_freertos` example, trimmed down to a single
core and dynamic-allocation-only build since this repo's other examples are
single-purpose rather than multi-variant.

Adjust `LED_GPIO_FAST`/`LED_GPIO_SLOW` in `freertos_blink.c` to match your
wiring (GPIO 25 is the onboard LED on a stock Pico/Pico W).

## 1.3" SH1106 OLED example (SPI)

`examples/oled_1in3_sh1106_spi` drives a genuine 1.3" 128x64 monochrome OLED
module (SH1106 controller) - draws a border, then animates a small square
scanning back and forth:

```
cd ~/workspace/examples/oled_1in3_sh1106_spi
cmake -S . -B build -G Ninja
cmake --build build
```

**Not the same product as the GMG12864-06D LCD example below** - an earlier
"1.3in OLED" AliExpress listing turned out on inspection to actually be that
ST7565R-driven LCD, a completely different chip/protocol, rather than a true
OLED. This example is for an actual SH1106 OLED module; check your board's
own part number/silkscreen if unsure which you have, since the two are easy
to conflate from a seller listing alone.

Wire (this module's own labeled pinout) SI (MOSI) to GPIO3, SCL (SCK) to
GPIO2, CS to GPIO1, RS (DC) to GPIO0, RSE (RST) to GPIO4, VDD to 3V3, plus
GND. The init sequence, the SH1106's +2 column RAM offset, and the page/
column addressing commands are taken from the u8g2 graphics library's actual
SH1106 driver, fetched and cross-checked rather than written from memory.
SPI clock is deliberately lowered to 1MHz (u8g2's own maintainers reduced
their default from 8MHz after reliability reports).

**Unverified:** the segment-remap (0xA1) and COM-scan-direction (0xC8)
commands are a guess for generic/clone SH1106 boards, not confirmed against
this specific module (u8g2's own reference sequence omits both). If the
border/animation appears mirrored or flipped, try toggling 0xA1<->0xA0
and/or 0xC8<->0xC0 in `oled_init()`. Unlike the e-paper example, there's no
BUSY pin to detect bad wiring - if nothing lights up, it's wiring/power, not
something the code can diagnose.

## GMG12864-06D LCD example (ST7565R, SPI)

`examples/lcd_12864_st7565_spi` drives a GMG12864-06D 128x64 monochrome graphic LCD
(ST7565R controller) - draws a border, then animates a small square scanning back and
forth:

```
cd ~/workspace/examples/lcd_12864_st7565_spi
cmake -S . -B build -G Ninja
cmake --build build
```

**This was originally built as an SH1106 OLED example** based on the seller listing
alone, and didn't work - the module turned out to be an ST7565R-driven LCD instead (a
different chip family, identified from the board's own part number), which is why: the
command set an OLED understands is meaningless to this chip, and ST7565-family LCDs
need a staged internal voltage-regulator/booster power-up sequence with real delays
between stages, absent from OLED init entirely, that produces a blank screen if missing
or wrong. The lesson: a seller's product title ("OLED") isn't the same as the actual
driver chip - when in doubt, check the board's own silkscreen/part number.

Wire (using this module's own 13-pin header labels) CS to GPIO1, RSE (reset) to GPIO4,
RS (DC) to GPIO0, SCL to GPIO2, SI (MOSI) to GPIO3, VDD to 3V3, VSS to GND. Backlight: A
(anode) to 3V3 through a current-limiting resistor (~100-220ohm), K (cathode) to GND -
wired directly, not through a GPIO. The four C_* pins on the header are internal
factory/test pins and aren't connected. The init sequence (LCD bias, ADC/COM direction,
the three-stage power control ramp, resistor ratio, contrast, display-on) and page/
column addressing come from Adafruit's actual ST7565 driver, not written from memory.
Unlike the other SPI examples here, there's no BUSY pin to detect bad wiring - if
nothing lights up, it's wiring/power, not something the code can diagnose.

**Contrast** (`LCD_CONTRAST`, valid range 0-0x3F) is confirmed on real hardware at
`0x08` for this panel - notably low, and well below both the reference driver's own
default (0, undocumented) and this file's first guess (0x24, which looked like a
uniform gray field with no visible shapes). ST7565-family LCDs are panel-specific
about contrast, so a different physical panel may still need retuning - adjust the
value if the display looks all-dark or all-blank once wiring is confirmed good.

**Column offset** (`LCD_COL_OFFSET`) is confirmed on real hardware at `4` - without
it, the border rendered 4 pixels off from the true left edge. The reference driver
applies no offset at all, so this is panel-specific, same class of quirk as the
SH1106 OLED example's +2 (a different chip/panel, different value).

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
