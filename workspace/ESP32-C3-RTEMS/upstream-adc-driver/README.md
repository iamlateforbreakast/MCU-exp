# Draft ESP32-C3 ADC driver for RTEMS's `esp32c3db` BSP

**Status: draft, unbuilt, untested.** Like the other three drivers in this
repo, this has not been compiled, linked, or run against real hardware.

This directory mirrors its intended final path in the RTEMS tree
(`bsps/riscv/esp32/...`), for the same reason as the others: RTEMS isn't
vendored in this repo, and this is meant to be easy to diff/copy into a
real checkout and eventually into an upstream merge request.

## Important difference from the GPIO/SPI/I2C drivers: there's no framework

GPIO, SPI, and I2C all had an established RTEMS driver API to implement
(`bsp/gpio.h`, `dev/spi/spi.h`, `dev/i2c/i2c.h`) - a fixed contract from an
existing, reviewed design. **RTEMS has no such thing for ADC.** Checked
directly against RTEMS `main` while drafting this: no
`cpukit/include/dev/adc`, no `bsps/include/dev/adc`, and no BSP anywhere in
the tree currently has an `adc` subdirectory (an old GSoC 2015 BeagleBoard
ADC effort and a 4.11-era "Analog Driver" BSP-howto chapter exist, but
nothing from that landed in - or survives in - `main`).

So `bsps/riscv/esp32/include/c3/adc.h`'s `esp32c3_adc1_init()` /
`esp32c3_adc1_read_raw()` API is **this driver's own invention**, not an
implementation of an existing RTEMS convention. It's deliberately small and
unopinionated (no bus/device-file abstraction, since there's nothing
established to match) so it's easy to wrap in a real framework later if
RTEMS ever gains one, or to fold into whatever shape an upstream reviewer
would actually want.

## What's here

- `bsps/riscv/esp32/include/c3/adc-regs.h` - internal SAR ADC register/field
  definitions.
- `bsps/riscv/esp32/include/c3/adc.h` - the public API (channel/attenuation
  constants, the two functions above) applications and other driver code
  should include.
- `bsps/riscv/esp32/adc/esp32c3-adc.c` - the implementation.

## Confidence level - two different sources, used for two different things

Register *offsets and bit positions* were checked directly against
Espressif's public `components/soc/esp32c3/register/soc/apb_saradc_reg.h`
while drafting this - same method as the other three drivers.

But that header's field *descriptions* are blank for almost this entire
peripheral (unlike GPIO/SPI/I2C's headers, which had prose descriptions for
most fields) - it names the bits but not what sequence of operations
actually produces a working reading. For that, this driver instead follows
a second, independent, real source: Espressif's own `esp-hal` Rust driver
(`esp-hal/src/analog/adc/riscv.rs` in `github.com/esp-rs/esp-hal`, fetched
from GitHub while drafting this) - an actively maintained, presumably
hardware-validated implementation. Specifically taken from there rather
than re-derived from the bare register header:

- The `APB_SARADC_CTRL_REG` initialization sequence in
  `esp32c3_adc1_init()` (`START_FORCE`/`START`/`SAR_CLK_GATED`/
  `XPD_SAR_FORCE` all set together) - not obviously derivable from the
  blank-description register header alone.
- That configuring the channel/attenuation and triggering the conversion
  are two *separate* register writes to `ONETIME_SAMPLE_REG`, not one
  combined write.
- That the result comes from `APB_SARADC_1_DATA_STATUS_REG` (offset
  `0x02C`) masked to its low 12 bits - not, as an earlier version of this
  driver assumed before checking, from `APB_SARADC_SAR1_STATUS_REG`
  (`0x010`), a different, unrelated-looking register that turned out not
  to be the right one.
- The completion signal (`APB_SARADC_ADC1_DONE_INT_RAW`, bit 31 of
  `INT_RAW_REG`) and how to clear it (write-1-to-clear via `INT_CLR_REG`,
  then clear `ONETIME_START`) after reading.

What's still genuinely unverified:

- **The ADC1 channel-to-GPIO mapping** (`ADC1_CHANNEL_GPIO0`..`GPIO4` = 1:1
  to GPIO0-GPIO4) - well-known public ESP32-C3 pinout information, not
  re-derived from either fetched source above, and not cross-checked
  against either of them either.
- **Calibration is entirely out of scope**, not just unverified - see
  `esp32c3-adc.c`'s file header. `esp32c3_adc1_read_raw()` returns a raw
  12-bit code (0-4095), not millivolts. Approximate, uncalibrated
  full-scale voltage per attenuation setting (documented in `adc.h`,
  same values ESP-IDF's `adc_atten_t` uses as nominal figures):

  | Attenuation | Approx. full-scale |
  |---|---|
  | `ADC_ATTEN_0DB` | ~1.1V |
  | `ADC_ATTEN_2_5DB` | ~1.5V |
  | `ADC_ATTEN_6DB` | ~2.2V |
  | `ADC_ATTEN_11DB` | ~3.9V (clamped below Vdd in practice) |

  Real accuracy requires per-device eFuse calibration trim values applied
  through an internal "REGI2C" analog-register-access mechanism (unrelated
  to the external I2C bus in `../upstream-i2c-driver/`) - a genuinely
  separate, more involved piece of work than this draft.
- **ADC2 is not implemented at all**, deliberately: on the wider ESP32
  family ADC2 shares hardware with the WiFi radio and is documented as
  unreliable while WiFi is active, so it didn't seem worth the surface
  area for a BSP that will likely want WiFi eventually.

## Integration steps (not yet done here)

Inside an RTEMS checkout:

1. Copy `bsps/riscv/esp32/adc/esp32c3-adc.c` and both new headers into the
   matching paths.
2. In `spec/build/bsps/riscv/esp32/obj.yml`, add
   `bsps/riscv/esp32/adc/esp32c3-adc.c` to `source:`.
3. In `spec/build/bsps/riscv/esp32/bspesp32c3db.yml`, add
   `bsps/riscv/esp32/include/c3/adc.h` and
   `bsps/riscv/esp32/include/c3/adc-regs.h` to the `install:` entry that
   already installs `c3/chip_definitions.h`.
4. Rebuild: `./waf configure --rtems-bsps=riscv/esp32c3db && ./waf && ./waf install`.
5. An application needs `#include <c3/adc.h>`, a call to
   `esp32c3_adc1_init()` during `Init`, then
   `esp32c3_adc1_read_raw(channel, atten, &raw)` as needed. No GPIO-matrix
   routing or pin configuration call is needed first - ADC1's analog input
   path is separate from the digital IO_MUX/GPIO-matrix path the other
   three drivers use, and reading a channel implicitly uses its pin in
   analog mode.

## Testing plan once it builds

No ESP32-C3 hardware is available in this sandbox. Before trusting this
against a real board, in roughly this order:

1. Build clean with no warnings under `-Wall -Wextra`.
2. Read a channel tied to a known fixed voltage (GND, or 3.3V through a
   safe divider) at each of the four attenuation settings, and sanity
   check the raw codes move in the expected direction and rough magnitude
   against the nominal full-scale table above - this alone would catch a
   wrong data-register offset or bit mask.
3. Read a channel connected to a potentiometer or variable supply and
   confirm the raw code tracks the input smoothly (no discontinuities,
   stuck values, or the `ADC1_DONE_INT_RAW` polling loop hanging) across
   its full range.
4. Call `esp32c3_adc1_read_raw()` back-to-back from two tasks concurrently
   and confirm the mutex actually serializes them rather than producing
   torn/corrupted readings.
5. If accuracy (not just "a number that moves the right way") turns out to
   matter for a real use case, that's the point to implement the eFuse/
   REGI2C calibration path this draft deliberately skipped.

Once verified, this belongs in an upstream merge request against
`gitlab.rtems.org/rtems/rtos/rtems` - though since there's no existing
RTEMS ADC framework to plug into, that conversation would likely also be
about whether this driver's own small API is the right shape, or whether
it's worth proposing a generic one alongside it.
