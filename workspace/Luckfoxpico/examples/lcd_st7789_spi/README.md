# ST7789 SPI LCD (Luckfox Pico Mini B)

Drives a 1.14" 135x240 ST7789 IPS SPI TFT (the "AITEXM ... ST7789 LCD Board SPI" module
from AliExpress) from Linux userspace via `/dev/spidev0.0` plus sysfs GPIO for DC/RESET.
Same panel as `../../ESP32-C3-RTEMS/examples/lcd_st7789_spi/`, ported to Linux userspace.

## Prerequisite: enable spi0

`spi0` ships `status = "disabled"` in the stock board dts
(`luckfox-pico/sysdrv/source/kernel/arch/arm/boot/dts/rv1103g-luckfox-pico-mini.dts`).
Flip it to `"okay"` (and disable the base dtsi's `fbtft@0` node so `spidev@0` owns the bus
without a chip-select conflict), then rebuild the kernel and reflash:

```
podman compose up -d luckfoxpico-dev
podman exec luckfoxpico-dev bash -lc 'cd ~/workspace/luckfox-pico && ./build.sh kernel'
podman exec luckfoxpico-dev bash -lc 'cd ~/workspace/luckfox-pico && ./build.sh updateimg'
```

Then put the board in maskrom mode (hold BOOT to GND while plugging in USB power) and, on
the host (`tools/linux/Linux_Upgrade_Tool/upgrade_tool` is a native x86-64 binary, no
container needed - one-time udev rule: `sudo cp .../88-rockusb.rules
/etc/udev/rules.d/ && sudo udevadm control --reload-rules && sudo udevadm trigger`):

```
tools/linux/Linux_Upgrade_Tool/upgrade_tool UF output/image/update.img
```

Always flash the full `update.img`, not a partial `boot.img`-only write - a partial flash
of just the kernel left this board stuck re-entering a rescue/loader state on every power
cycle instead of booting.

## Wiring

No silkscreen labels on this board's header - physical pin numbers below are from this
board's own pinout reference, not just the SoC pin name (which doesn't tell you where it's
physically broken out).

| Display pin | Board pin | Signal |
|---|---|---|
| VCC | PIN3 | 3.3V |
| GND | PIN2 | - |
| SCL | PIN7 | SPI0_CLK_M0 |
| SDA | PIN8 | SPI0_MOSI_M0 |
| CS | PIN6 | SPI0_CS0_M0 |
| RES | PIN17 | GPIO1_C7 (sysfs 55) |
| DC | PIN12 | GPIO1_D0 (sysfs 56) |
| BLK | PIN3 | tied straight to 3.3V - no GPIO needed (always-on backlight) |

RES/DC intentionally reuse the same physical pins `../gpio_led_blink/` already proved are
present on this header (as LED1/LED2), rather than the vendor's `fbtft@0` reference wiring
(GPIO1_A2/GPIO1_C3) - those two were never confirmed to be broken out on this board's
header, and GPIO1_C3 is also spi0's MISO pinmux pin so it can't be repurposed as a plain
GPIO without excluding it from spi0's `pinctrl-0` (another kernel rebuild).

## Build and run

Runs on the board itself, not in the `luckfoxpico-dev` container (that's for cross-building
only). No `ssh`/`scp` on this image - only telnet - so cross-compile a static binary and
transfer it as base64 over telnet:

```
podman exec luckfoxpico-dev bash -lc '
  TOOLCHAIN=~/workspace/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin
  $TOOLCHAIN/arm-rockchip830-linux-uclibcgnueabihf-gcc -static -O2 -Wall -Wextra \
    -o ~/workspace/examples/lcd_st7789_spi/lcd_st7789_spi \
    ~/workspace/examples/lcd_st7789_spi/lcd_st7789_spi.c
  $TOOLCHAIN/arm-rockchip830-linux-uclibcgnueabihf-strip ~/workspace/examples/lcd_st7789_spi/lcd_st7789_spi
'
```

Then telnet in (root/luckfox), transfer the binary, `chmod +x`, and run it. Ctrl-C (or a
plain `kill`, not `-9`) stops it and unexports the GPIOs cleanly. It cycles the whole
screen through solid red/green/blue so it's easy to confirm the SPI link and controller
init are both working.
