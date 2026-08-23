# Draft ESP32-C3 I2C driver for RTEMS's `esp32c3db` BSP

**Status: draft, unbuilt, untested.** Like `../upstream-gpio-driver/` and
`../upstream-spi-driver/`, this has not been compiled, linked, or run
against real hardware.

This directory mirrors its intended final path in the RTEMS tree
(`bsps/riscv/esp32/...`), for the same reason as the other two: RTEMS isn't
vendored in this repo, and this is meant to be easy to diff/copy into a
real checkout and eventually into an upstream merge request.

## What's here

- `bsps/riscv/esp32/include/c3/i2c-regs.h` - I2C_EXT0 controller and
  GPIO-matrix pin-routing register/field definitions.
- `bsps/riscv/esp32/i2c/esp32c3-i2c.c` - a polled master driver against
  those registers, implementing RTEMS's generic I2C bus API
  (`cpukit/include/dev/i2c/i2c.h` - an `i2c_bus` struct with
  `transfer`/`set_clock`/`destroy` callbacks and Linux-compatible
  `i2c_msg` messages, the same framework `arm/raspberrypi`'s
  `rpi_i2c_register_bus()` uses).

## Why this one is structurally different from the SPI/GPIO drivers

The ESP32-C3's I2C controller ("I2C_EXT0") isn't a simple byte-shift-register
peripheral like GP-SPI2 or a level/edge-register peripheral like GPIO - it's
a small command-queue state machine. Firmware writes up to 8 instructions
(`I2C_COMD0_REG`..`I2C_COMD7_REG`, each one of RESTART/WRITE/READ/STOP/END)
describing an entire bus transaction, then triggers it with
`I2C_TRANS_START` and the hardware executes the whole sequence - generating
start/repeated-start/stop conditions, clocking bytes in/out of the TX/RX
FIFOs, and checking ACKs - on its own. This directly maps onto
`i2c_msg[]`'s semantics (a repeated start between messages, a stop only
after the last one), which is convenient, but it also means "how many
messages/bytes fit in one hardware pass" is a real, fixed constraint this
driver has to enforce explicitly - see Scope below.

## Confidence level

Every register offset and bit position was checked directly against
Espressif's public `components/soc/esp32c3/register/soc/i2c_reg.h` while
drafting this (not recalled from memory) - including a useful
self-consistency check: the 14-bit `I2C_COMMAND0` field width exactly
matches `byte_num(8) + ack_en(1) + ack_exp(1) + ack_value(1) + op_code(3)`,
confirming the command-word sub-field layout used here. The GPIO-matrix
pin-routing registers and `I2CEXT0_SCL_IDX`/`I2CEXT0_SDA_IDX` (53/54, from
`gpio_sig_map.h`) reuse the same confirmed facts established while drafting
`../upstream-spi-driver/`.

What is **not** verified:

- **`I2C_TXFIFO_REG`/`I2C_RXFIFO_REG`'s byte stride**, called out in
  `i2c-regs.h`. Espressif's register header gives only a single
  `_START_ADDR` offset each for TX/RX, not a per-byte layout. This driver
  assumes a 4-byte-aligned slot per FIFO byte (mirroring GP-SPI2's
  `SPI_W0_REG`..`SPI_W15_REG` array on this same SoC), not a single
  fixed auto-incrementing address (the older, pre-S2/S3/C3-generation ESP32
  I2C style). If this assumption is wrong, the driver's control flow (start/
  stop conditions, ACK/NACK detection, command sequencing) should still be
  correct, but no actual data would move - the most likely observable
  symptom is transfers "completing" with garbage or all-zero data.
- **The FIFO depth (`I2C_FIFO_DEPTH = 32`)**, used to bound how much a
  single hardware pass can move. A commonly-quoted figure for this
  controller generation, not confirmed against the register header (which
  doesn't state it directly) or the TRM.
- **`I2C_SOURCE_CLK_HZ` (40MHz)** - the same caveat as GP-SPI2's
  `SPI2_SOURCE_CLK_HZ` in `../upstream-spi-driver/`: this BSP's
  `bspstart.c` does no clock-tree configuration, so the actual running
  frequency is unconfirmed.
- **The SCL timing-parameter formula** in `esp32c3_i2c_set_clock()`. Every
  setup/hold register this controller exposes
  (`START_HOLD`/`RSTART_SETUP`/`STOP_HOLD`/`STOP_SETUP`/`SDA_HOLD`/
  `SDA_SAMPLE`) is set to a value derived from the bit half-period rather
  than computed against the I2C specification's actual timing margins for
  the requested mode (standard/fast/fast-plus) - it should produce a
  functioning bus, not a spec-compliant one at every declared speed. Note
  that this *is* necessary, not optional: three of those hold/setup
  registers reset to a fixed 8-cycle default, which is far too short once
  the source clock is divided down for a real I2C bus rate (confirmed
  arithmetically: 8 cycles at 40MHz is 0.2us, well under standard mode's
  4.0us start-hold-time minimum) - so leaving them at their power-on reset
  values, rather than scaling them, would have been the actual bug here.

## Scope of this first draft

- Master mode only, standard 7-bit addressing - `i2c_msg.flags & I2C_M_TEN`
  is rejected.
- Polled - no interrupts.
- **The whole `i2c_msg[]` array passed to one `transfer()` call must fit in
  a single hardware pass**: at most 8 queued commands and (assumed) 32
  bytes each of TX and RX FIFO space, shared across every message in the
  call. Concretely: each message costs 2 commands (`RESTART` +
  `WRITE`(address)) plus 1 more for a non-empty write, 1 more for a 1-byte
  read, or 2 more for a longer read, plus a final shared `STOP`. All limits
  are checked explicitly (`-EINVAL`) before anything touches hardware -
  never silently truncated. This comfortably covers the overwhelmingly
  common pattern (e.g. "write a 1-byte register address, repeated start,
  read a handful of bytes" - most real sensor/EEPROM traffic) but not
  arbitrarily long or many-message transfers. Lifting it means implementing
  the hardware's `END`-opcode pause/resume streaming mechanism (queue a
  batch ending in `END` instead of `STOP`, wait for
  `I2C_END_DETECT_INT_RAW`, drain/refill the FIFOs, queue the next batch),
  deliberately left out of this draft for simplicity.
- `i2c_bus.default_address`/`.ten_bit_address`/`.use_pec`/`.retries` are
  left unused, matching `rpi_i2c_transfer()`'s approach - every message
  carries its own address and this draft doesn't implement SMBus PEC or
  automatic retry.

## Integration steps (not yet done here)

Inside an RTEMS checkout:

1. Copy `bsps/riscv/esp32/i2c/esp32c3-i2c.c` and
   `bsps/riscv/esp32/include/c3/i2c-regs.h` into the matching paths, and
   make sure `upstream-gpio-driver/`'s `gpio-regs.h` (now including the
   `GPIO_PIN_PAD_DRIVER` bit this driver added there) is present at
   `bsps/riscv/esp32/include/c3/gpio-regs.h`.
2. In `spec/build/bsps/riscv/esp32/obj.yml`, add
   `bsps/riscv/esp32/i2c/esp32c3-i2c.c` to `source:`.
3. In `spec/build/bsps/riscv/esp32/bspesp32c3db.yml`, add
   `bsps/riscv/esp32/include/c3/i2c-regs.h` to the `install:` entry that
   already installs `c3/chip_definitions.h`.
4. Confirm whether `cpukit/dev/i2c/i2c-bus.c` (the generic engine behind
   `i2c_bus_init`/`i2c_bus_alloc_and_init`/`i2c_bus_register`) is already
   part of the default cpukit build - same open question as
   `cpukit/dev/spi/spi-bus.c` in `../upstream-spi-driver/README.md`.
5. Rebuild: `./waf configure --rtems-bsps=riscv/esp32c3db && ./waf && ./waf install`.
6. An application needs `#include <dev/i2c/i2c.h>`, a call to
   `i2c_bus_register_esp32c3("/dev/i2c-0", scl_pin, sda_pin)` during `Init`,
   then `open("/dev/i2c-0", O_RDWR)` +
   `ioctl(fd, I2C_RDWR, &(struct i2c_rdwr_ioctl_data){...})` per the Linux
   i2c-dev convention this framework follows.

## Testing plan once it builds

No ESP32-C3 hardware is available in this sandbox. Before trusting this
against a real board, in roughly this order:

1. Build clean with no warnings under `-Wall -Wextra`.
2. Scope SCL with a logic analyzer against a known `set_clock()` request to
   pin down `I2C_SOURCE_CLK_HZ` for real, exactly as for GP-SPI2.
3. Probe a known-present device address with a zero-length read
   (`msg.len == 0`, `I2C_M_RD` set) and confirm it ACKs while a
   known-absent address correctly returns `-EIO` - this alone validates
   RSTART/address/ACK-checking/STOP without needing FIFO data movement to
   be correct yet.
4. Write then read back from a real EEPROM or sensor register in one
   `transfer()` call (two messages, repeated start) and confirm the data
   round-trips - this is what actually validates the FIFO byte-stride
   assumption above.
5. Deliberately trigger a NACK (write to a bad register/address mid-bus)
   and confirm `-EIO` comes back promptly and the bus recovers for the
   next transfer (i.e. `I2C_FSM_RST` in the error path actually works) -
   an I2C driver that wedges the bus after the first error is much less
   useful than one that returns an error code.
6. Exercise the 8-command/32-byte boundary: a transfer just inside the
   limit should succeed, one just outside should return `-EINVAL` before
   touching hardware (not corrupt a shorter, previously-valid transfer).

Once verified, this belongs in an upstream merge request against
`gitlab.rtems.org/rtems/rtos/rtems`, alongside the GPIO and SPI drivers.
