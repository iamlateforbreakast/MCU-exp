# Draft ESP32-C3 TIMG0 timer + watchdog driver for RTEMS's `esp32c3db` BSP

**Status: draft, unbuilt, untested.** Like the other four drivers in this
repo, this has not been compiled, linked, or run against real hardware.

This directory mirrors its intended final path in the RTEMS tree
(`bsps/riscv/esp32/...`), for the same reason as the others: RTEMS isn't
vendored in this repo, and this is meant to be easy to diff/copy into a
real checkout and eventually into an upstream merge request.

## Why this one: bsp_start() already touches this peripheral

This driver started from a direct question: what state does `bsp_start()`
(`bspstart.c`) leave the watchdogs in? Checked directly against that file:
it disables both the RTC watchdog and TIMG0's watchdog at boot (plus sets
the RTC "super watchdog" to auto-feed), which means **no RTEMS application
on this BSP has ever had a real, configurable reset watchdog available** -
`bsp_start()`'s job is specifically to get out of the way of the ROM's
boot-time protection, not to leave anything usable behind. This driver is
what turns TIMG0's watchdog back into something an application can
actually configure and feed, alongside its general-purpose timer half.

One finding from that investigation worth documenting here since it isn't
written down anywhere else: `bsp_start()`'s watchdog-disable code masks
each `WDTCONFIG0` register down to just its `EN` bit
(`wdt_config0 & RTC_CNTL_WDTCONFIG0_EN`, and the equivalent for TIMG0)
rather than clearing that bit (`& ~EN`). This looks inverted at first
read, but it works: the same mask also zeros the `STG0`-`STG3`
stage-action fields to `0` ("off"), and Espressif's register header
confirms a stage set to "off" never triggers regardless of `EN`'s state -
so the watchdogs do end up fully inert, just via an incidental side effect
of the masking rather than a direct expression of "disable". Worth knowing
before touching that code, and specifically *not* copied into this driver's
own `esp32c3_timg0_wdt_disable()`, which just does the plain, direct
`&= ~TIMG_WDT_EN` instead.

## Why an original API, like the ADC driver

Same situation as `../upstream-adc-driver/`: RTEMS has no generic
hardware-timer or watchdog driver framework to target (checked the same
way: no `cpukit/include/dev/{timer,watchdog}`, nothing comparable
anywhere in the tree). `bsps/riscv/esp32/include/c3/timg.h` is this
driver's own small API, not an implementation of an existing convention.

## What's here

- `bsps/riscv/esp32/include/c3/timg-regs.h` - internal TIMG0 register/field
  definitions.
- `bsps/riscv/esp32/include/c3/timg.h` - the public API (six functions:
  timer init/read/set_alarm/alarm_fired, watchdog enable/feed/disable).
- `bsps/riscv/esp32/timg/esp32c3-timg.c` - the implementation.

## Confidence level

High, and for once without needing a second cross-check source: unlike
`apb_saradc_reg.h` (see `../upstream-adc-driver/README.md`), Espressif's
`components/soc/esp32c3/register/soc/timer_group_reg.h` has full prose
descriptions for every field in this peripheral, including the ones that
matter most for getting the sequence right (`WDT_STG0`-`WDT_STG3`'s
"0: off, 1: interrupt, 2: reset CPU, 3: reset system" values, and
`WDT_WKEY`'s "if the register contains a different value than its reset
value, write protection is enabled" - both used directly, not inferred).

That confidence in the *register facts* didn't stop a real design bug from
creeping into an earlier version of this driver's own logic, caught and
fixed before committing: the watchdog is configured to run off the 40MHz
crystal (`TIMG_WDT_USE_XTAL`) rather than `APB_CLK`, specifically to avoid
depending on this BSP's unconfirmed boot-time APB frequency (same reason
the general timer uses `T0_USE_XTAL`) - see
`../upstream-spi-driver/README.md`'s `SPI2_SOURCE_CLK_HZ` caveat for why
that's unconfirmed. But `TIMG_WDTCONFIG1_REG`'s own field description
gives the MWDT clock period as "12.5ns * prescale value", a figure that
describes the clock at APB_CLK's commonly-quoted 80MHz - not the ~40MHz
crystal this driver actually selects. An earlier draft used both facts
without reconciling them: selecting XTAL while computing cycle counts
against the 80MHz-derived 12.5ns figure, which would have made every
configured watchdog timeout run roughly 2x longer than requested - a
watchdog that fires late is close to as bad as one that never fires at
all. Fixed by deriving `TIMG_WDT_REF_CLK_HZ` from the ~40MHz crystal
consistently (see `timg-regs.h`'s watchdog clock-source comment for the
full note).

What's still genuinely unverified, beyond the general "not run on hardware
yet":

- **The 40MHz crystal figure itself** - same "typical ESP32-C3 board"
  assumption tier as the other drivers' XTAL-based reasoning, not
  independently confirmed for any specific board.
- **`TIMG_WDT_WKEY`'s value** (`0x50d83aa1`) was checked by direct
  arithmetic against this register's own documented reset-default
  (`1356348065` decimal, which is exactly `0x50D83AA1`) rather than
  assumed to match `bspstart.c`'s `RTC_CNTL_WDTWPROTECT_KEY` by name
  similarity alone - but that bspstart.c constant is itself unverified
  independently in this session (inherited as-is from the existing BSP
  source, not re-derived here).
- **Whether `TIMG_WDTWPROTECT_REG` is actually unlocked** when this
  driver's functions run. `bsp_start()`'s own TIMG0 watchdog-disable code
  writes to `WDTCONFIG0` *without* unlocking it first, which only works
  because protection hadn't been engaged yet at that point in boot (the
  key register was still at its own reset-default value). This driver
  doesn't rely on that assumption - every function that touches
  `WDTCONFIG*`/`WDTFEED` unlocks first - but it's worth knowing the
  existing boot code takes a shortcut this driver deliberately doesn't.

## Scope of this first draft

- TIMG0 only - see `timg-regs.h` for why TIMG1 (a straightforward
  extension) isn't drafted here.
- General timer: polled only, no interrupt handler installed for the
  alarm - `esp32c3_timg0_timer_alarm_fired()` polls and clears the raw
  status bit. RTEMS applications wanting a plain periodic/one-shot
  software callback should generally prefer the Classic API's Timer
  Manager (`rtems_timer_fire_after()` and friends), which already sits on
  the SYSTIMER-driven tick - this hardware timer is for cases that
  specifically need direct hardware access.
- Watchdog: single-stage only (stage 0's action is configurable; stages
  1-3 are always left "off") with a fixed `/1` MWDT prescaler. No
  interrupt handler is installed for `ESP32C3_WDT_INTERRUPT` - the raw
  `TIMG_WDT_INT` status bit is left set for the caller to notice, not
  dispatched anywhere by this driver. A more complete driver might
  escalate through multiple stages (e.g. interrupt first, reset later if
  still not fed) - deliberately out of scope here for the same
  simplicity-first reasons as the other drivers' scoping choices.
- No internal locking/mutex, unlike the ADC driver - watchdog and timer
  configuration are treated as infrequent, largely single-task operations
  here rather than a per-transaction hot path. A caller using these
  functions from multiple tasks concurrently needs to serialize that
  itself.

## Integration steps (not yet done here)

Inside an RTEMS checkout:

1. Copy `bsps/riscv/esp32/timg/esp32c3-timg.c` and both new headers into
   the matching paths.
2. In `spec/build/bsps/riscv/esp32/obj.yml`, add
   `bsps/riscv/esp32/timg/esp32c3-timg.c` to `source:`.
3. In `spec/build/bsps/riscv/esp32/bspesp32c3db.yml`, add
   `bsps/riscv/esp32/include/c3/timg.h` and
   `bsps/riscv/esp32/include/c3/timg-regs.h` to the `install:` entry that
   already installs `c3/chip_definitions.h`.
4. Rebuild: `./waf configure --rtems-bsps=riscv/esp32c3db && ./waf && ./waf install`.
5. An application needs `#include <c3/timg.h>`. For the watchdog:
   `esp32c3_timg0_wdt_enable(timeout_ms, action)` once, then
   `esp32c3_timg0_wdt_feed()` more often than `timeout_ms` from wherever
   the application considers itself "healthy". For the timer:
   `esp32c3_timg0_timer_init(divider)` once, then `_read()`/`_set_alarm()`/
   `_alarm_fired()` as needed.

## Testing plan once it builds

No ESP32-C3 hardware is available in this sandbox. Before trusting this
against a real board, in roughly this order:

1. Build clean with no warnings under `-Wall -Wextra`.
2. General timer: call `esp32c3_timg0_timer_init()` and read
   `esp32c3_timg0_timer_read()` in a loop against a known wall-clock
   interval (or against the SYSTIMER-driven RTOS tick), and confirm the
   count rate matches the requested divider against the assumed 40MHz -
   this is what would catch the crystal-frequency assumption being wrong
   for a specific board.
3. Set a one-shot alarm and confirm `esp32c3_timg0_timer_alarm_fired()`
   returns true at roughly the right time and not before; then an
   auto-reload alarm and confirm it fires repeatedly at the expected
   interval.
4. Watchdog: enable with a short timeout (a few seconds) and a `RESET_CPU`
   or `RESET_SYSTEM` action, then *don't* feed it, and confirm the board
   actually resets at approximately the configured time - this is the
   test that validates the crystal-frequency correction above, since a
   watchdog firing at roughly 2x the requested timeout would be exactly
   the bug that got caught and fixed while drafting this.
5. Enable the watchdog, feed it regularly from a lower-priority task, and
   confirm normal operation continues indefinitely; then stop feeding and
   confirm the timeout still fires - this is what validates
   `esp32c3_timg0_wdt_feed()` under realistic use rather than just at
   startup.
6. Confirm `esp32c3_timg0_wdt_disable()` actually stops a previously-armed
   watchdog from firing.

Once verified, this belongs in an upstream merge request against
`gitlab.rtems.org/rtems/rtos/rtems`, alongside the GPIO/SPI/I2C/ADC
drivers - though, like the ADC driver, that conversation would likely also
cover whether this driver's own small API is the right shape for a
generic RTEMS timer/watchdog interface that doesn't currently exist.
