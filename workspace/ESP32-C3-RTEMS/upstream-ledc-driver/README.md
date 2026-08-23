# Draft ESP32-C3 LEDC (PWM) driver for RTEMS's `esp32c3db` BSP

**Status: draft, unbuilt, untested.** Like the other drivers in this repo,
this has not been compiled, linked, or run against real hardware.

This directory mirrors its intended final path in the RTEMS tree
(`bsps/riscv/esp32/...`), for the same reason as the others: RTEMS isn't
vendored in this repo, and this is meant to be easy to diff/copy into a
real checkout and eventually into an upstream merge request.

## Original API, like ADC/TIMG

Same situation as `../upstream-adc-driver/` and `../upstream-timg-driver/`:
RTEMS has no generic PWM driver framework (checked the same way: no
`cpukit/include/dev/pwm`; other BSPs each define their own bespoke PWM API
- e.g. `arm/atsam`'s `libchip/include/pwmc.h` - rather than a shared one).
`bsps/riscv/esp32/include/c3/ledc.h` is this driver's own small API: up to
4 independent timers (frequency + duty resolution) and up to 6 channels,
each bound to one timer and one GPIO pin with its own duty cycle - several
channels can share a timer, the normal way to drive something like an RGB
LED's three channels in sync.

## What's here

- `bsps/riscv/esp32/include/c3/ledc-regs.h` - internal LEDC register/field
  definitions.
- `bsps/riscv/esp32/include/c3/ledc.h` - the public API (three functions:
  timer_config, channel_config, set_duty).
- `bsps/riscv/esp32/ledc/esp32c3-ledc.c` - the implementation.

## Confidence level - register facts blank, sequence confirmed by real code

Register offsets and bit positions were checked directly against
Espressif's public `components/soc/esp32c3/register/soc/ledc_reg.h` while
drafting this - but like `apb_saradc_reg.h` (see
`../upstream-adc-driver/README.md`), that header's field *descriptions*
are blank for this peripheral, naming each bit without saying what
sequence of writes actually produces a working PWM output.

For that, this driver again went to a second, independent, real source:
Espressif's `esp-hal` Rust driver
(`esp-hal/src/ledc/low_level/v1.rs` in `github.com/esp-rs/esp-hal` -
"v1" being the LEDC hardware generation the ESP32-C3 uses, confirmed by
checking the module's version-selection `cfg` attributes rather than
assumed). That source is used directly, not just consulted for
inspiration, in `esp32c3-ledc.c`'s `esp32c3_ledc_apply_duty()` and
`esp32c3_ledc_configure_clock()`:

- The `DUTY << 4` encoding when writing the duty register (4 fractional
  bits beyond the timer's configured resolution).
- That applying a duty change *at all* - even a plain, non-fading one -
  goes through the hardware's fade-engine registers
  (`DUTY_START`/`DUTY_INC`/`DUTY_NUM`/`DUTY_CYCLE`/`DUTY_SCALE`), using
  the specific degenerate values `DUTY_NUM=1, DUTY_CYCLE=1, DUTY_SCALE=0`
  to make it apply immediately rather than actually fading - not
  something guessable from the bare field names alone.
- That the timer's `RST` bit, which defaults to `1` (held in reset) at
  power-on, must be explicitly cleared as part of configuring a timer -
  easy to have missed, since a first read of `ls_configure_hw()`'s field
  list could look like it's just setting values rather than also clearing
  something that starts non-zero.
- The two-step global-clock-select sequence (write `LEDC_CONF_REG`, then
  latch it via *timer 0's* `PARA_UP` specifically, not a global one) -
  a genuinely quirky, non-obvious detail of the real implementation.

What's still unverified:

- **`LEDC_APB_CLK_HZ` (80MHz)** - same unconfirmed-boot-time-APB-frequency
  caveat as the SPI2/I2C/UART1 drivers.
- **The clock-divider fixed-point format** (assumed: 18-bit field, 8
  fractional bits, i.e. `tick_freq = APB_CLK / (integer + fraction/256)`)
  - a well-established convention across the ESP32 LEDC family from
    general knowledge, but esp-hal's `ls_configure_hw()` takes an
    already-computed `divisor` value without exposing the bit-width
    breakdown, so this wasn't cross-checked against real code the way the
    duty-update sequence was.
- **`LEDC_APB_CLK_SEL`'s value convention.** The register header shows
  this as a 2-bit field, but esp-hal's `.apb_clk_sel().set_bit()` call
  reads like it's treating it as a single bit - most likely `set_bit()`
  there is svd2rust sugar for writing the field to its named "APB_CLK"
  enum variant (which this driver assumes is numeric value `1`), not
  literally bit 0 alone, but that numeric value itself wasn't confirmed
  from a source that states it explicitly.
- **`LEDC_MAX_DUTY_RES` (14 bits)** - a commonly cited figure for this
  chip, matching the register field widths (`HPOINT` is 14 bits; `DUTY`
  is 19 bits, consistent with 14 resolution bits + the 4 fractional bits
  above, with a little headroom), not independently confirmed by a source
  that states the maximum directly.

## Scope of this first draft

- Duty changes always apply immediately - this driver always programs the
  hardware fade engine with the degenerate one-step values described
  above rather than exposing real multi-step fading
  (`DUTY_NUM`/`DUTY_CYCLE`/`DUTY_SCALE` as an actual ramp).
- No interrupt handling - duty/fade-done interrupts exist
  (`LEDC_INT_RAW_REG`) but aren't used; every call in this driver is a
  fire-and-forget register write, not something you wait on completion
  for (a plain, non-fading duty update is fast enough on this hardware
  that this shouldn't matter for the common case).
- Global slow-clock source is always `APB_CLK` - the alternative
  `REF_TICK`/`RC_FAST`/`XTAL` sources `LEDC_CONF_REG` supports aren't
  exposed.
- No internal locking, matching the TIMG driver's precedent - PWM
  configuration is treated as an infrequent, largely single-task
  operation rather than a per-transaction hot path.

## Integration steps (not yet done here)

Inside an RTEMS checkout:

1. Copy `bsps/riscv/esp32/ledc/esp32c3-ledc.c` and both new headers into
   the matching paths.
2. In `spec/build/bsps/riscv/esp32/obj.yml`, add
   `bsps/riscv/esp32/ledc/esp32c3-ledc.c` to `source:`.
3. In `spec/build/bsps/riscv/esp32/bspesp32c3db.yml`, add
   `bsps/riscv/esp32/include/c3/ledc.h` and
   `bsps/riscv/esp32/include/c3/ledc-regs.h` to the `install:` entry that
   already installs `c3/chip_definitions.h`.
4. Rebuild: `./waf configure --rtems-bsps=riscv/esp32c3db && ./waf && ./waf install`.
5. An application needs `#include <c3/ledc.h>`, then
   `esp32c3_ledc_timer_config(timer, freq_hz, duty_bits)` once per timer,
   `esp32c3_ledc_channel_config(channel, timer, gpio)` once per channel,
   and `esp32c3_ledc_set_duty(channel, duty)` as often as needed.

## Testing plan once it builds

No ESP32-C3 hardware is available in this sandbox. Before trusting this
against a real board, in roughly this order:

1. Build clean with no warnings under `-Wall -Wextra`.
2. Drive an LED (with a current-limiting resistor) or an oscilloscope
   probe from one channel, sweep duty from 0 to max, and confirm
   brightness/duty tracks smoothly and monotonically - this alone
   validates the duty-update sequence and the `<< 4` encoding.
3. Measure the actual PWM frequency on a scope against a few different
   requested `freq_hz`/`duty_resolution_bits` combinations, and confirm
   it matches - this is what would catch the `LEDC_APB_CLK_HZ` assumption
   or the clock-divider fixed-point format being wrong.
4. Configure two channels on the same timer and confirm they run at
   identical frequency with independently correct duty cycles.
5. Reconfigure a channel's duty repeatedly at a fast rate (e.g. a task
   loop) and confirm no glitches or dropped updates - this exercises
   `esp32c3_ledc_apply_duty()`'s degenerate-fade path under realistic,
   repeated use rather than a single one-off call.

Once verified, this belongs in an upstream merge request against
`gitlab.rtems.org/rtems/rtos/rtems`, alongside the other drivers in this
repo - though, like the ADC/TIMG drivers, that conversation would likely
also cover whether this driver's own small API is the right shape for a
generic RTEMS PWM interface that doesn't currently exist.
