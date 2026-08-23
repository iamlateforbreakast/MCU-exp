# Draft ESP32-C3 GPIO driver for RTEMS's `esp32c3db` BSP

**Status: draft, unbuilt, untested.** This is a first cut at the register-level
GPIO driver called out as missing in `../ESP32-C3-RTEMS.md`. It has not been
compiled, linked, or run against real hardware - it hasn't even been dropped
into an RTEMS checkout and built with `waf` yet. Treat every register offset
and bit position below as "needs a second pair of eyes" until that happens.

This directory is not part of the RTEMS build itself - RTEMS isn't vendored
in this repo (`Dockerfile.esp32c3-rtems` clones `github.com/RTEMS/rtems` at
image-build time). The files here mirror their intended final path in the
RTEMS tree (`bsps/riscv/esp32/...`) so they're easy to diff/copy into a real
checkout for testing, and eventually into an upstream merge request the same
way `esp32c3db` itself landed via
[MR !1160](https://gitlab.rtems.org/rtems/rtos/rtems/-/merge_requests/1160).

## What's here

- `bsps/riscv/esp32/include/c3/gpio-regs.h` - GPIO controller and IO_MUX
  register/field definitions, plus `BSP_GPIO_PIN_COUNT` /
  `BSP_GPIO_PINS_PER_BANK`, required by RTEMS's generic GPIO API header.
- `bsps/riscv/esp32/gpio/gpio.c` - implements the sixteen `rtems_gpio_bsp_*`
  callbacks that header requires, against those registers.

## Why the generic framework, not a bespoke API

RTEMS already ships a reusable GPIO driver framework -
`bsps/include/bsp/gpio.h` (application-facing `rtems_gpio_*` API) backed by
`bsps/shared/dev/gpio/gpio-support.c` (pin bookkeeping, interrupt dispatch,
debouncing) - that a BSP plugs into by implementing a fixed set of low-level
`rtems_gpio_bsp_*` callbacks. It's the same framework `arm/beagle`
(BeagleBone Black, from the 2014/2015 GSoC project that introduced it) uses,
and it's what an upstream reviewer would expect rather than a one-off
`esp32c3_gpio_set()`-style API. `gpio.c` here implements all sixteen
required callbacks:

`multi_set` `multi_clear` `multi_read` `specific_group_operation`
`multi_select` `set` `clear` `get_value` `select_input` `select_output`
`select_specific_io` `set_resistor_mode` `interrupt_line` `get_vector`
`enable_interrupt` `disable_interrupt`

`specific_group_operation`, `multi_select`, and `select_specific_io` are
stubbed to fail/return "not defined" - alternate-function pin routing through
the GPIO matrix (`GPIO_FUNCn_IN/OUT_SEL_CFG_REG`, for e.g. muxing a UART or
SPI peripheral onto a non-default pin) is out of scope for this draft, which
only covers plain digital input/output and edge/level interrupts.

## Confidence level of the register values

Checked directly against this BSP's existing
`bsps/riscv/esp32/include/c3/chip_definitions.h` (fetched from RTEMS `main`
while drafting this) and cross-referenced against Espressif's public
`GPIO_PROCPU_INTR = 16` interrupt already defined there:

- **Confirmed**: `GPIO_BASE` (`0x6000_4000`) and `IO_MUX_BASE`
  (`0x6000_9000`) - originally reasoned by adjacency to the
  already-confirmed `SYSTIMER_BASE`/`RTC_CNTL_BASE`/`USB_SERIAL_JTAG_BASE`
  addresses in that same file's memory map, now independently confirmed
  against Espressif's real `reg_base.h` while drafting
  `../upstream-spi-driver/`. The plain GPIO controller register offsets
  (`GPIO_OUT_REG`, `GPIO_ENABLE_REG`, `GPIO_IN_REG`, `GPIO_STATUS_REG`, the
  per-pin `GPIO_PINn_REG` interrupt-config registers and their `INT_TYPE`
  field) follow the standard layout shared across the whole ESP32
  xtensa/RISC-V family.
- **Update, now confirmed**: the `IO_MUX_GPIOn_REG` per-pin offset formula
  and the `FUN_IE`/`FUN_WPU`(`FUN_PU`)/`FUN_WPD`(`FUN_PD`)/`MCU_SEL` bit
  positions in this file were flagged below as unverified when first
  drafted. While drafting `../upstream-spi-driver/` (which also touches
  IO_MUX), they were checked directly against Espressif's real
  `components/soc/esp32c3/register/soc/io_mux_reg.h` and
  `.../include/soc/gpio_sig_map.h` - every bit position here turned out
  correct, including `IO_MUX_FUNCTION_GPIO = 1` (Espressif's own
  `PIN_FUNC_GPIO` constant) as the GPIO alternate-function selector.
  `upstream-spi-driver/README.md` has the fetch details.
  Original unverified note, left for context: these were recalled from the
  general ESP32-family IO_MUX layout, not confirmed against the C3's own
  Technical Reference Manual chapter ("IO MUX and GPIO Matrix") or
  ESP-IDF's `soc/io_mux_reg.h` for `esp32c3`.
- **Not implemented, flagged in `gpio.c`**: `rtems_gpio_interrupt`'s
  `BOTH_LEVELS` has no corresponding hardware trigger mode on this GPIO
  controller (only disabled/rising/falling/any-edge/low-level/high-level
  exist), so `rtems_gpio_bsp_enable_interrupt()` rejects it.

## Integration steps (not yet done here)

To actually build this into the `esp32c3db` BSP, inside an RTEMS checkout
(e.g. the container's `~/kernel`, cloned fresh each image build per
`Dockerfile.esp32c3-rtems` - it isn't persisted, so these changes need to
land upstream or be re-applied per build until then):

1. Copy `bsps/riscv/esp32/gpio/gpio.c` and
   `bsps/riscv/esp32/include/c3/gpio-regs.h` into the matching paths in the
   checkout.
2. In `spec/build/bsps/riscv/esp32/obj.yml`, add to `source:`:
   - `bsps/riscv/esp32/gpio/gpio.c`
   - `bsps/shared/dev/gpio/gpio-support.c` (the generic engine this driver
     plugs into - not yet a build source for this BSP)
   and to `install:`, alongside the existing `bsp/irq.h` entry under
   `${BSP_INCLUDEDIR}/bsp`:
   - `bsps/include/bsp/gpio.h`
3. In `spec/build/bsps/riscv/esp32/bspesp32c3db.yml`, add
   `bsps/riscv/esp32/include/c3/gpio-regs.h` to the existing
   `install:` entry that already installs `c3/chip_definitions.h` to
   `${BSP_INCLUDEDIR}/${ESPRESSIF_CHIP_VARIANT}`.
4. **Also patch `bsps/riscv/esp32/include/bsp.h`** (not part of this
   directory - it's the existing shared BSP header, not vendored in this
   repo): add
   `#define BSP_GPIO_PIN_COUNT 22` / `#define BSP_GPIO_PINS_PER_BANK 32`
   right after the `#include <bsp/default-initial-extension.h>` line, the
   same way `bsps/arm/beagle/include/bsp.h` defines those two macros
   directly rather than via a separate header. This step is required, not
   optional: `bsps/shared/dev/gpio/gpio-support.c` (the framework `gpio.c`
   plugs into) includes only `<bsp/gpio.h>`, which `#error`s out if
   `BSP_GPIO_PIN_COUNT`/`BSP_GPIO_PINS_PER_BANK` aren't already defined by
   the time it's processed - `gpio-regs.h` defining them isn't enough on its
   own since nothing includes `gpio-regs.h` before `gpio-support.c` gets
   compiled. (An earlier attempt at this step tried `#include
   <c3/gpio-regs.h>` / `#include <gpio-regs.h>` from `bsp.h` instead of
   defining the macros inline - don't do that: `${ESPRESSIF_CHIP_VARIANT}`
   in step 3's YAML is referenced nowhere else in the tree, so it resolves
   empty and `gpio-regs.h` installs flat as `<gpio-regs.h>`, not under a
   `c3/` subdirectory, even though the in-tree source lives at
   `bsps/riscv/esp32/include/c3/gpio-regs.h` - the same include line can't
   satisfy both the in-tree build and the installed layout, and
   `chip_definitions.h` has this identical pre-existing flattening quirk.)
5. Rebuild: `./waf configure --prefix=$RTEMS_ROOT --rtems-bsps=riscv/esp32c3db && ./waf && ./waf install`
   (`--prefix=$RTEMS_ROOT` matters - a bare `./waf configure` without it
   resets the install prefix to waf's default `/opt/rtems`, which
   `builder` can't write to; `Dockerfile.esp32c3-rtems`'s original
   configure call passes it explicitly for this reason).
6. An application using it just needs `#include <bsp/gpio.h>` (the
   `BSP_GPIO_PIN_COUNT`/`BSP_GPIO_PINS_PER_BANK` macros come along for free
   via `bsp.h`, per step 4); call `rtems_gpio_initialize()` once during
   `Init`, then `rtems_gpio_request_pin()` / `rtems_gpio_set()` /
   `rtems_gpio_clear()` as usual. `examples/gpio_led_blink/init.c` in this
   repo does exactly this and builds cleanly with these steps applied.

**Confirmed working end-to-end** (2026-08-23, inside `esp32c3-rtems-dev`):
steps 1-6 above produce a clean `./waf` build (`gpio.c` and
`bsps/shared/dev/gpio/gpio-support.c` both compile, `librtemsbsp.a` links)
and `examples/gpio_led_blink` then builds and links to a `.exe` with no
warnings under `-Wall -Wextra`. Still not run against real hardware - see
"Testing plan" below.

## Testing plan once it builds

No ESP32-C3 hardware is available in this sandbox - only the image build and
its sanity check have run for this BSP so far (see `../ESP32-C3-RTEMS.md`).
Before trusting this against a real board:

1. Build clean with no warnings under `-Wall -Wextra`.
2. Bench-test on real hardware: configure one pin as output, verify with a
   multimeter/scope/LED that `rtems_gpio_set`/`rtems_gpio_clear` actually
   toggle it and that the level survives a `rtems_gpio_release_pin()` +
   re-request.
3. Configure a second pin as input with a button/switch, verify
   `rtems_gpio_get_value()` and `rtems_gpio_enable_interrupt()` (rising and
   falling edge) both work, including that `rtems_gpio_bsp_interrupt_line()`
   actually clears the pending bit (a driver that doesn't would fire the
   handler once and then hang the vector).
4. Confirm the IO_MUX assumptions above didn't just "happen to boot" because
   the pin was already in its GPIO-function reset state - test a pin that
   defaults to a non-GPIO function.

Once verified, this belongs in an upstream merge request against
`gitlab.rtems.org/rtems/rtos/rtems`, not just in this repo.
