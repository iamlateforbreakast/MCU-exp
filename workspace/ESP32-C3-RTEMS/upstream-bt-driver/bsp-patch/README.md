# BT/BLE interrupt-vector patch for `esp32c3db`

**Status: build-confirmed 2026-08-25.** Both fragments were applied to a real
RTEMS `main` checkout (`~/kernel` in the `esp32c3-rtems-dev` container) -
the seven `#define`s into `chip_definitions.h` next to the existing
`UHCI0_INTR`/`GPIO_PROCPU_INTR` block, the `irq_mappings[]` entry into
`irq_c3.c` sharing `cpu_int=7` with `EFUSE_INTR`/`LEDC_INTR` exactly as
planned - and `./waf configure --prefix=$RTEMS_ROOT --rtems-bsps=riscv/esp32c3db
&& ./waf && ./waf install` completed with no errors or warnings on
`irq_c3.c`, producing a working `librtemsbsp.a`/installed BSP. This only
confirms the patch integrates and builds clean; the guessed interrupt source
(`RWBLE_INTR`) is still unconfirmed at runtime - see "Testing plan" below,
still gated on real hardware + a vendored `bt.c`. Not part of the RTEMS
build itself; the two files here are fragments showing exactly what to add
to two existing upstream RTEMS files, not full copies of them (this repo
doesn't vendor RTEMS - see `../../ESP32-C3-RTEMS.md`).

Without this patch, `../freertos-compat/src/esp_intr_alloc.c`'s
`esp_intr_alloc()` is generic and compiles fine, but fails at runtime with
an invalid-vector error the moment `bt.c` actually calls it - RTEMS
currently has zero knowledge of any BT/Wi-Fi interrupt source.

## What's here

- `bsps/riscv/esp32/include/c3/chip_definitions-bt-additions.h` - the
  `#define`s to merge into the existing `#define` block in
  `bsps/riscv/esp32/include/c3/chip_definitions.h` (real ESP32-C3
  interrupt-matrix source numbers 4-10, confirmed against real ESP-IDF
  v5.3.1 source - see `../README.md`'s Phase 2 section for the full
  cross-check).
- `bsps/riscv/esp32/irq/irq_mappings-bt-addition.c` - the one
  `irq_mappings[]` entry to append in `bsps/riscv/esp32/irq/irq_c3.c`.

## Why all 4-10 are defined but only one is wired in

`bt.c` never names its interrupt source constant in open source - the
closed `libbtdm_app.a` blob supplies it at runtime through a callback
(`osi_funcs_t.interrupt_alloc`, see `../README.md`'s `osi_funcs_t`
correction). `RWBLE_INTR` (8) is a reasoned best guess (BLE-only chip,
"RWBLE" = RivieraWaves BLE IP, no deprecation caveat unlike `BT_MAC_INTR`'s
"will be cancelled"), not a confirmed fact. Defining the whole 4-10 range
costs nothing and makes correcting the `irq_mappings[]` entry a one-line
fix if Phase 3's hardware test (send an HCI Reset over VHCI, see
`../README.md`) shows a different source number is actually requested.

## Integration steps (not yet done here)

1. Add the seven `#define` lines from
   `chip_definitions-bt-additions.h` into the real
   `bsps/riscv/esp32/include/c3/chip_definitions.h`, next to its existing
   interrupt-source `#define`s.
2. Add the one `irq_mappings[]` entry from
   `irq_mappings-bt-addition.c` into the real `irq_mappings[]` array in
   `bsps/riscv/esp32/irq/irq_c3.c`.
3. Rebuild: `./waf configure --prefix=$RTEMS_ROOT --rtems-bsps=riscv/esp32c3db && ./waf && ./waf install`
   (same as every other driver here - see `../upstream-gpio-driver/README.md`
   for why `--prefix=$RTEMS_ROOT` matters).
4. No application-facing header changes needed beyond this - once linked,
   `esp_intr_alloc(RWBLE_INTR, ...)` (or whichever constant `bt.c`'s blob
   actually requests) should successfully install a handler instead of
   failing at runtime.

## Testing plan once it builds

No ESP32-C3 hardware or RTEMS checkout available in this sandbox - this
hasn't been compiled or run. Before trusting it:

1. Build clean with no warnings under `-Wall -Wextra`, same bar as every
   other driver here.
2. Confirmed working precondition for Phase 3 (`../README.md`): after this
   patch, `../freertos-compat/src/esp_intr_alloc.c`'s `esp_intr_alloc()`
   call from `bt.c`'s `interrupt_alloc_wrapper()` should return success
   instead of failing at `rtems_interrupt_handler_install()`.
3. If it fails, log the `source` value `bt.c`'s blob actually passes and
   compare against the guessed `RWBLE_INTR` (8) - correcting
   `irq_mappings-bt-addition.c`'s entry is a one-line fix once the real
   value is known.
