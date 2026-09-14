/*
 * Serial-only diagnostics for the ESP-IDF BLE controller running on RTEMS.
 *
 * These are the instruments that root-caused the long-standing
 * illegal-instruction crash on 2026-09-14 (see ../../upstream-bt-driver/
 * vendor/README.md's Bluetooth section). They exist because RTEMS's own
 * RISC-V exception printer, cpukit/score/cpu/riscv/riscv-exception-frame-
 * print.c, prints mstatus/mcause/mepc and the registers but never `mtval`,
 * never the ISR nest level, and never any stack - and because the closed
 * blob's state lives in globals and heap vtables that nothing else reports.
 *
 * Everything here is printk/printf over the console: no JTAG, no power
 * cycle per data point. That turned out to matter enormously - three
 * sessions of JTAG breakpoint-hunting inside the blob produced less than one
 * afternoon of this.
 *
 * OFF BY DEFAULT. Build with `make BLE_DIAG=1` to enable; the Makefile
 * passes the same value to the ISR-counter instrumentation in
 * ../../upstream-bt-driver/freertos-compat/src/esp_intr_alloc.c, since the
 * counters below are defined there.
 */
#ifndef BLE_DIAG_H
#define BLE_DIAG_H

#include <rtems.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef BLE_DIAG
#define BLE_DIAG 0
#endif

#if BLE_DIAG

/* Interrupt-matrix routing: which peripheral source reaches which CPU
 * interrupt, and at what priority. Used to confirm at runtime that
 * bsp-patch/'s RWBLE_INTR=8 -> cpu_int 7 entry - documented in its own
 * README as "a reasoned best guess, not a confirmed fact" - is correct. */
void ble_diag_dump_intr_matrix(const char *tag);

/* The blob's pointer globals at the top of DRAM (above RTEMS's RamEnd) and
 * the scheduler timing parameters next to them. */
void ble_diag_dump_blob_globals(const char *tag);

/* Raw word dump, for pulling a whole blob dispatch table out over serial and
 * resolving its entries offline with addr2line. */
void ble_diag_dump_words(const char *name, uint32_t base, unsigned bytes);

/* Fatal extension: prints mtval, ISR nest level, interrupt-stack bounds,
 * where sp sits relative to them, a wide window of stack either side of sp,
 * and RTEMS's per-task stack high-water report. Install via
 * CONFIGURE_INITIAL_EXTENSIONS - confdefs/extensions.h concatenates rather
 * than replaces BSP_INITIAL_EXTENSION, so RTEMS's own register dump still
 * prints alongside this one. */
void ble_diag_fatal_extension(
    rtems_fatal_source source,
    bool always_set_to_false,
    rtems_fatal_code code
);

/* Invocation count for the BT controller's own ISR, from the trampoline in
 * esp_intr_alloc.c. Zero until the radio is actually transmitting. */
uint32_t ble_diag_bt_isr_count(void);

#else /* !BLE_DIAG - no-op stubs so call sites need no #ifdef */

static inline void ble_diag_dump_intr_matrix(const char *tag) { (void) tag; }
static inline void ble_diag_dump_blob_globals(const char *tag) { (void) tag; }
static inline void ble_diag_dump_words(const char *name, uint32_t base,
                                       unsigned bytes)
{
    (void) name; (void) base; (void) bytes;
}
static inline uint32_t ble_diag_bt_isr_count(void) { return 0; }

#endif /* BLE_DIAG */

#endif /* BLE_DIAG_H */
