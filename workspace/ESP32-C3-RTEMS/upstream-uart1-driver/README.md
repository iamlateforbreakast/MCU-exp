# Draft ESP32-C3 UART1 driver for RTEMS's `esp32c3db` BSP

**Status: draft, unbuilt, untested.** Like the other drivers in this repo,
this has not been compiled, linked, or run against real hardware.

This directory mirrors its intended final path in the RTEMS tree
(`bsps/riscv/esp32/...`), for the same reason as the others: RTEMS isn't
vendored in this repo, and this is meant to be easy to diff/copy into a
real checkout and eventually into an upstream merge request.

## Different from every other driver so far: this one uses a real framework

GPIO/SPI/I2C each implemented an existing RTEMS driver API. ADC and TIMG
had no such framework, so they got small original APIs of their own. UART1
is back to the first category, but with a twist: RTEMS's real framework
for this - **Termios device drivers**
(`rtems_termios_device_install()`, `<rtems/termiosdevice.h>`) - is *not*
what this BSP's own console driver (`console-config.c`) uses. That file
takes a simpler shortcut (`<bsp/console-polled.h>`'s `BSP_output_char`/
`BSP_poll_char`) appropriate for a single, fixed boot console. UART1 is a
second, independently `open()`-able serial port, which needs the real
Termios framework to behave like a normal device file (`read()`/`write()`/
`tcsetattr()` for baud rate, etc.) - so this driver is the first thing in
this whole set that plugs into that framework rather than either an
existing bus-object API (GPIO/SPI/I2C) or a from-scratch one (ADC/TIMG).

## What's here

- `bsps/riscv/esp32/include/c3/uart1-regs.h` - internal UART1 register/field
  definitions.
- `bsps/riscv/esp32/include/c3/uart1.h` - the public API: one function,
  `esp32c3_uart1_install()`.
- `bsps/riscv/esp32/console/esp32c3-uart1.c` - the implementation (placed
  under `console/`, alongside `console-config.c`, since it's the same
  peripheral family - a second UART, not a second peripheral kind).

## Confidence level

Register offsets and bit positions were checked directly against
Espressif's public `components/soc/esp32c3/register/soc/uart_reg.h` while
drafting this - fully documented for this peripheral, like `timg_reg.h`
and unlike `apb_saradc_reg.h`. The Termios framework side (exact
`rtems_termios_device_handler` field names/signatures,
`rtems_termios_device_install()`, `rtems_termios_baud_to_number()`) came
from RTEMS's real `<rtems/termiosdevice.h>` and `<rtems/termiostypes.h>`,
fetched directly, not recalled - matching the confirmed
`TERMIOS_POLLED` handler shape (`first_open`/`poll_read`/`write`/
`set_attributes`/`ioctl`/`mode`) documented in RTEMS's own BSP-howto.

One nice self-consistency check found while drafting the baud-rate divider
math: `UART_CLKDIV_REG`'s documented reset-default value is `0x2B6` (694
decimal) for its integer part. Independently computing the divider this
driver would produce for 115200 baud (ESP-IDF's near-universal UART
default) against the assumed 80MHz `APB_CLK` gives exactly 694 too - a
real, if indirect, corroborating data point that both the divider formula
and the 80MHz assumption are pointed in the right direction, not just
internally self-consistent.

What's still unverified:

- **`UART1_APB_CLK_HZ` (80MHz)** - same unconfirmed-boot-time-APB-frequency
  caveat as the SPI2/I2C drivers (see their READMEs); this BSP's
  `bspstart.c` does no clock-tree setup. Unlike TIMG's watchdog/timer, no
  XTAL/APB clock-source-select bit was found in UART1's own register block
  during this drafting session, so there was no way to sidestep this the
  way the TIMG driver did.
- **`UART_PARITY`'s value convention** (assumed: `0` = even, `1` = odd,
  mapped directly from termios' `PARODD` flag) - Espressif's register
  header names the field ("configure the parity check mode") without
  stating which value means which, unlike most other fields in this same
  header.
- **`UART_FIFO_DEPTH` (128 bytes)**, used only to bound the polling loop
  in `esp32c3_uart1_write()` - a commonly cited figure for this UART IP
  block across the ESP32 family, not confirmed in the fetched register
  header (which doesn't state FIFO depth directly) or independently
  cross-checked the way the ADC/TIMG drivers' uncertain constants were.
  Wrong in either direction would show up as either overly conservative
  polling (harmless, just slower) or, if the real depth were *smaller*
  than assumed, potentially writing past what the hardware can actually
  buffer before the next byte is accepted - worth confirming before
  relying on high-throughput writes.
- **Whether the Termios core tolerates `last_close`/`ioctl` left `NULL`** -
  inferred from other minimal reference drivers following the same
  pattern, not confirmed by reading the core's dispatch code itself.

## Scope of this first draft

- `TERMIOS_POLLED` mode only - no interrupt handler installed, despite
  `UART1_INTR` already being defined in this BSP's `chip_definitions.h`.
- Honors baud rate, data bits (5/6/7/8 via `CSIZE`), one or two stop bits
  (`CSTOPB`), and parity (`PARENB`/`PARODD`) from `struct termios`. Does
  not implement hardware flow control (RTS/CTS), break signaling, or IrDA
  mode - all of which `UART_CONF0_REG`/`UART_CONF1_REG`/
  `UART_FLOW_CONF_REG` support in principle, per the fetched register
  header.
- Single instance (UART1 only) with caller-chosen TX/RX GPIO pins, routed
  through the GPIO matrix the same way the SPI/I2C drivers' pins are
  (`U1TXD_OUT_IDX`/`U1RXD_IN_IDX` = 9, confirmed against
  `gpio_sig_map.h`).

## Integration steps (not yet done here)

Inside an RTEMS checkout:

1. Copy `bsps/riscv/esp32/console/esp32c3-uart1.c` and both new headers
   into the matching paths.
2. In `spec/build/bsps/riscv/esp32/obj.yml`, add
   `bsps/riscv/esp32/console/esp32c3-uart1.c` to `source:`.
3. In `spec/build/bsps/riscv/esp32/bspesp32c3db.yml`, add
   `bsps/riscv/esp32/include/c3/uart1.h` and
   `bsps/riscv/esp32/include/c3/uart1-regs.h` to the `install:` entry that
   already installs `c3/chip_definitions.h`.
4. Rebuild: `./waf configure --rtems-bsps=riscv/esp32c3db && ./waf && ./waf install`.
5. An application needs `#include <c3/uart1.h>`, a call to
   `esp32c3_uart1_install("/dev/ttyS1", tx_pin, rx_pin)` during `Init`,
   then `open("/dev/ttyS1", O_RDWR)` + `read()`/`write()`, with
   `tcsetattr()` beforehand for anything other than the RTEMS Termios
   default settings. `rtems_termios_device_install()` returns
   `RTEMS_INCORRECT_STATE` if Termios itself isn't initialized yet - any
   application that also configures the console (`CONFIGURE_APPLICATION_
   NEEDS_CONSOLE_DRIVER`) already gets that as a side effect, but a
   console-less application would need to arrange it first.

## Testing plan once it builds

No ESP32-C3 hardware is available in this sandbox. Before trusting this
against a real board, in roughly this order:

1. Build clean with no warnings under `-Wall -Wextra`.
2. Loop UART1's TX back to its own RX on the bench (a single jumper wire)
   and confirm bytes written come back read - this alone validates the
   GPIO-matrix routing, FIFO push/pop, and the polling loops without
   needing a second device.
3. Connect to a real USB-serial adapter or second board and verify the
   baud rate is actually correct at the receiving end for a few different
   rates (9600, 115200, and something in between/odd like 57600) - this is
   what would catch the `UART1_APB_CLK_HZ` assumption being wrong.
4. Test each parity setting (`none`/`even`/`odd`) against a known-good
   second UART and confirm frames are accepted (not flagged as
   parity-errored on the other end) - this validates the `UART_PARITY`
   value-convention assumption.
5. Send a burst of more than `UART_FIFO_DEPTH` bytes in one `write()` call
   at a low baud rate (so the FIFO genuinely fills faster than it drains)
   and confirm no bytes are dropped or corrupted - this validates the FIFO
   depth assumption under actual back-pressure.

Once verified, this belongs in an upstream merge request against
`gitlab.rtems.org/rtems/rtos/rtems`, alongside the other drivers in this
repo.
