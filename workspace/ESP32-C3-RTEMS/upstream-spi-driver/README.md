# Draft ESP32-C3 SPI driver for RTEMS's `esp32c3db` BSP

**Status: builds clean, but does not work on real hardware.** Unlike when
this was first drafted, this has now been built, integrated into the BSP,
and run against a real ESP32-C3 (2026-08-23, driving both an ST7789 LCD
and an SH1106 OLED - two independent devices, same result). Every single SPI transaction deadlocks the calling task forever:
`esp32c3_spi_do_chunk()`'s `SPI_POLL_WHILE` loop after setting
`SPI_CMD_REG.usr` never exits, because that bit never self-clears, and
`SPI_DMA_INT_RAW_REG.trans_done` (bit 12) never fires either - this is a
genuine hardware-level stall (confirmed with a bounded retry loop
substituted for the real unbounded one, purely for diagnosis), not a
status-bit-misread bug or something specific to the first transaction only.

Ruled out via live register reads against the real chip, each cross-checked
against Espressif's actual `esp32c3` headers (`spi_reg.h`, `spi_struct.h`,
`system_reg.h`, `reg_base.h`) fetched fresh rather than recalled from
memory:
- Every register offset in `spi-regs.h` (`SPI_CMD_REG` 0x00 through
  `SPI_SLAVE_REG` 0xE0) matches `spi_reg.h` exactly.
- `SPI_USR_MOSI`/`SPI_USR_MISO` (bits 27/28) match `spi_struct.h`'s
  `usr_mosi`/`usr_miso` fields exactly.
- `SPI_CMD_REG.update` (bit 23) does self-clear correctly, both during
  `esp32c3_spi_configure()` and again at the top of every
  `esp32c3_spi_do_chunk()` call - so basic APB register access and *that*
  handshake both work.
- GP-SPI2's peripheral clock-enable and reset bits
  (`SYSTEM_PERIP_CLK_EN0_REG`/`SYSTEM_PERIP_RST_EN0_REG`, bit 6 in each,
  base `0x600c0000`) were already in the correct state (clocked, not held
  in reset) before this driver ever touches SPI2 - not a missing
  clock-gating step.

Also tried: the "Testing plan" section's own suggested first hardware
test - wiring MOSI (GPIO5) directly to MISO (GPIO3) and sending a known
byte full-duplex. The received byte did match the sent byte (`0xa5` in,
`0xa5` out) - but this is a false positive, not confirmation: `SPI_W0_REG`
is a shared TX/RX buffer, pre-loaded with the TX byte before the
transaction starts. A live register dump across the whole (bounded, for
diagnosis) wait showed `SPI_W0_REG` frozen at the exact TX value the
entire time, for every command byte tried, not just this one - meaning
nothing ever touched it, not that a loopback exchange occurred. This
actually strengthens the "hardware genuinely never runs the transaction"
conclusion rather than weakening it: real bit-shifting, even a failed or
partial exchange, would be expected to change the register's contents at
some point.

Not yet checked: an oscilloscope/logic analyzer on SCLK/MOSI/CS, which
would show directly whether the GPIO-matrix routing is actually getting a
clock onto the pin at all, or whether the peripheral is generating clock
edges internally but never latching completion - this needs that hardware
to make progress, per the "Testing plan" section below, which called this
out as necessary before trusting the driver even before this round of
testing. Until this is root-caused, none of the examples that depend on
this driver (`examples/lcd_st7789_spi`, `examples/oled_1in3_sh1106_spi`)
will actually produce output on their displays, even though they build and
run without crashing.

This directory mirrors its intended final path in the RTEMS tree
(`bsps/riscv/esp32/...`), for the same reason as `upstream-gpio-driver/`:
RTEMS isn't vendored in this repo, and this is meant to be easy to
diff/copy into a real checkout and eventually into an upstream merge
request.

## What's here

- `bsps/riscv/esp32/include/c3/spi-regs.h` - GP-SPI2 controller and
  GPIO-matrix pin-routing register/field definitions.
- `bsps/riscv/esp32/spi/esp32c3-spi.c` - a polled master driver against
  those registers, implementing RTEMS's generic SPI bus API
  (`cpukit/include/dev/spi/spi.h` - a `spi_bus` struct with
  `transfer`/`setup`/`destroy` callbacks, the same Linux-spidev-compatible
  framework `arm/atsam`'s `spi_bus_register_atsam()` uses).

## Confidence level - much higher than the GPIO driver's

Unlike `upstream-gpio-driver/`, where the IO_MUX bit positions were flagged
as recalled-from-memory and unverified, every register offset and bit
position used here was checked directly against Espressif's public ESP-IDF
headers while drafting this (not recalled from memory):

- `components/soc/esp32c3/register/soc/spi_reg.h` - GP-SPI2 registers
- `components/soc/esp32c3/register/soc/gpio_reg.h` - GPIO-matrix
  `FUNCn_IN/OUT_SEL_CFG` registers
- `components/soc/esp32c3/register/soc/io_mux_reg.h` - confirms
  `FUN_IE`/`FUN_PU`/`FUN_PD`/`MCU_SEL` bit positions exactly match what
  `upstream-gpio-driver/`'s `gpio-regs.h` had already guessed (that
  driver's IO_MUX assumptions turned out correct - worth updating its
  README's confidence note accordingly)
- `components/soc/esp32c3/include/soc/gpio_sig_map.h` - confirms the
  `FSPICLK`/`FSPIQ`/`FSPID` GPIO-matrix signal indices (63/64/65) used to
  route GP-SPI2 onto arbitrary GPIO pins
- `components/soc/esp32c3/register/soc/reg_base.h` - confirms
  `DR_REG_SPI2_BASE = 0x6002_4000`, and incidentally re-confirms
  `DR_REG_GPIO_BASE`/`DR_REG_IO_MUX_BASE` from the GPIO driver

So the **register layout** is high confidence. What is **not** verified:

- **The driver logic built on top of those registers** - clock-divider
  arithmetic, the full-duplex CPU-buffer (`SPI_W0..W15`) sequencing, and
  the GPIO-matrix input-vs-output routing-direction asymmetry (see the
  comment above `GPIO_FUNC_IN_SEL_CFG_REG`/`GPIO_FUNC_OUT_SEL_CFG_REG` in
  `spi-regs.h`) - these are standard patterns for this SPI controller
  generation but have not been run against silicon here.
- **The mode -> `(ck_idle_edge, ck_out_edge)` mapping** in
  `esp32c3_spi_configure()`. This reproduces a specific, well-known mapping
  from Espressif's own `spi_ll_master_set_mode()` for this register
  generation, but it's still a recollection of that function's logic, not
  a fetched copy of it.
- **`SPI2_SOURCE_CLK_HZ` (currently `40_000_000`, see `spi-regs.h`)** -
  genuinely unknown, not just unverified. This BSP's `bspstart.c` does no
  clock-tree configuration at all (it's a direct-boot BSP with no
  2nd-stage bootloader), so GP-SPI2's actual source clock frequency in
  this configuration has not been established. 40MHz (the typical ESP32-C3
  crystal) is a guess, not a measurement. Every computed SPI bit rate is
  wrong by whatever ratio this guess is off by, until this is confirmed
  (oscilloscope on SCLK is the simplest check).

## Scope of this first draft

- Master mode only, 8 bits per word, full duplex.
- Polled (CPU-buffer) transfers only - no DMA, no interrupts. Each
  `spi_ioc_transfer` is chunked into <=64-byte USR transactions, since the
  `SPI_W0_REG..SPI_W15_REG` buffer is 16 x 32-bit words and there's no DMA
  path in this draft to move more than that in one hardware transaction.
- Single device: chip select is a plain GPIO (supplied to
  `spi_bus_register_esp32c3()`) toggled manually around each transfer,
  rather than GP-SPI2's own hardware CS0/1/2 lines routed through the
  matrix. This sidesteps `SPI_CS_SETUP`/`SPI_CS_HOLD` auto-timing and
  multi-device chip-select decode entirely, at the cost of one device per
  bus instance in this draft - `spi_ioc_transfer.cs` must be `0`.
- `spi_bus.ioctl` is left unset - `SPI_IOC_RD/WR_MODE` etc. aren't wired
  up, only the `SPI_IOC_MESSAGE` transfer-array path (which goes through
  `transfer()`, not `ioctl()`).

## Integration steps (not yet done here)

Inside an RTEMS checkout (e.g. the container's `~/kernel` - see
`../upstream-gpio-driver/README.md` for why this isn't persisted across
image builds):

1. Copy `bsps/riscv/esp32/spi/esp32c3-spi.c` and
   `bsps/riscv/esp32/include/c3/spi-regs.h` into the matching paths, and
   make sure `upstream-gpio-driver/`'s `gpio-regs.h` is also present at
   `bsps/riscv/esp32/include/c3/gpio-regs.h` - this driver depends on it
   for `GPIO_REG`/`IO_MUX_REG`/`IO_MUX_MCU_SEL`/etc.
2. In `spec/build/bsps/riscv/esp32/obj.yml`, add
   `bsps/riscv/esp32/spi/esp32c3-spi.c` to `source:` (alongside whatever
   `upstream-gpio-driver/README.md` already calls for to build the GPIO
   driver, since `spi-regs.h` includes `gpio-regs.h`).
3. In `spec/build/bsps/riscv/esp32/bspesp32c3db.yml`, add
   `bsps/riscv/esp32/include/c3/spi-regs.h` to the `install:` entry that
   already installs `c3/chip_definitions.h`.
4. Confirm whether `cpukit/dev/spi/spi-bus.c` (the generic engine behind
   `spi_bus_init`/`spi_bus_alloc_and_init`/`spi_bus_register` - this
   driver's other dependency, besides the GPIO one above) is already part
   of the default cpukit build for every BSP, or needs enabling somewhere
   under `spec/build/cpukit/`. It lives under `cpukit/`, not
   `bsps/shared/`, unlike the GPIO framework's `gpio-support.c`, which
   suggests (but doesn't confirm) it's already built-in.
5. Rebuild: `./waf configure --rtems-bsps=riscv/esp32c3db && ./waf && ./waf install`.
6. An application needs `#include <dev/spi/spi.h>`, a call to
   `spi_bus_register_esp32c3("/dev/spi0", sclk_pin, mosi_pin, miso_pin, cs_pin)`
   during `Init`, then `open("/dev/spi0", O_RDWR)` +
   `ioctl(fd, SPI_IOC_MESSAGE(n), transfers)` per the Linux spidev
   convention this framework follows.

## Testing plan once it builds

No ESP32-C3 hardware is available in this sandbox. Before trusting this
against a real board, in roughly this order:

1. Build clean with no warnings under `-Wall -Wextra`.
2. Scope SCLK with a logic analyzer or oscilloscope against a known
   `speed_hz` request to pin down `SPI2_SOURCE_CLK_HZ` for real (see
   above) - everything else here is downstream of getting that right.
3. Loop MOSI back to MISO on the bench (no slave device needed) and verify
   a full-duplex transfer echoes correctly - this alone validates the
   GPIO-matrix routing, the CPU-buffer load/unload byte ordering, and the
   `SPI_USR`/`SPI_UPDATE` sequencing without needing real slave hardware.
4. Test against a real SPI device (e.g. an SPI flash chip or sensor) for
   all four modes and both bit orders, and confirm CS timing (setup/hold
   relative to the first/last clock edge) is adequate for that device -
   this draft's software CS has no configurable delay at all.
5. Test a transfer spanning multiple 64-byte chunks and multiple
   `spi_ioc_transfer` messages with `cs_change` set/unset, to validate the
   chunking and CS-hold-across-messages logic in `esp32c3_spi_transfer()`.

Once verified, this belongs in an upstream merge request against
`gitlab.rtems.org/rtems/rtos/rtems`, alongside (or after) the GPIO driver.
