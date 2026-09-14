/*
 * esp_intr_alloc/free/enable/disable on top of RTEMS's generic interrupt
 * API. `rtems_interrupt_handler_install`/`_remove` signatures were
 * confirmed against real RTEMS `main` source in Phase 2's recon (see
 * ../../README.md); `rtems_interrupt_vector_enable`/`_disable` (used below
 * for esp_intr_enable/disable) were NOT independently re-confirmed this
 * session - they're RTEMS's well-known public vector-enable API
 * (`cpukit/include/rtems/irq-extension.h`) but re-check the exact
 * signature before trusting this file.
 *
 * `source` (the ESP32-C3 interrupt-matrix number) is used directly as the
 * RTEMS vector number, matching how this BSP's own irq_mappings[] table
 * already works (e.g. GPIO_PROCPU_INTR=16 is both the matrix source number
 * and the RTEMS vector). This only actually succeeds once the BSP's
 * chip_definitions.h/irq_mappings[] table has an entry for whatever source
 * bt.c is allocating - not yet added (see README's Phase 2 section) - RTEMS
 * will simply reject unrecognized vectors at install time until it is.
 */
#include "esp_intr_alloc.h"
#include <rtems/rtems/intr.h>
#include <rtems/bspIo.h>
#include <stdlib.h>

/*
 * DIAG (2026-09-14): which interrupt-matrix sources the closed blob
 * actually requests has never been observed at runtime - bsp-patch/'s
 * RWBLE_INTR=8 -> cpu_int=7 mapping is documented in its own README as "a
 * reasoned best guess, not a confirmed fact", and an earlier session's JTAG
 * breakpoint on this function never fired (inconclusive: that round also
 * had multi-breakpoint-arming problems). This matters because
 * rtems_interrupt_handler_install() accepts ANY nonzero vector, so a source
 * that is missing from irq_c3.c's irq_mappings[] installs successfully,
 * gets int_periph_to_cpu() == 0, and is then routed to CPU interrupt 0 -
 * never delivered, with no error anywhere. printk (not printf) because this
 * can be called from the BT task with interrupts in an unknown state.
 */
#ifndef DIAG_INTR_ALLOC
#define DIAG_INTR_ALLOC 0
#endif

struct intr_handle_data_s {
    rtems_vector_number vector;
    rtems_interrupt_handler handler;
    void *arg;
};

#if DIAG_INTR_ALLOC
/*
 * DIAG (2026-09-14): the crash this port is chasing restores `s2` from a
 * stack slot that still holds RTEMS's stack-checker virgin fill pattern
 * (0xa5a5a5a5) while `ra` comes back as stale garbage - the signature of an
 * epilogue reading from a DIFFERENT sp than its prologue wrote to. The
 * obvious thing that can shift sp under a running task is interrupt
 * entry/exit, and this port's interrupt path (a hand-written BSP
 * irq_mappings[] entry plus this shim) is exactly the part that differs
 * from real ESP-IDF. So wrap the blob's handler: record sp on entry and on
 * exit, count invocations, and latch the first mismatch. This also answers
 * a question nothing has answered yet - whether the BT interrupt fires at
 * ALL on this port.
 */
volatile uint32_t diag_bt_isr_count;
volatile uint32_t diag_bt_isr_max_cycles;   /* longest single handler run */
volatile uint32_t diag_bt_isr_last_cycles;  /* most recent handler run */
volatile uint32_t diag_bt_isr_total_cycles; /* sum, for a mean duration */
volatile uint32_t diag_bt_isr_slow_count;   /* runs longer than 500 us */
volatile uint32_t diag_bt_isr_sp_in;
volatile uint32_t diag_bt_isr_sp_out;
volatile uint32_t diag_bt_isr_sp_mismatch;

static struct intr_handle_data_s *diag_wrapped[4];
static unsigned diag_wrapped_count;

/* ESP32-C3 has SOC_CPU_HAS_CSR_PC, so the cycle counter is the custom
 * performance-counter CSR 0x7e2, not the standard mcycle - and it only runs
 * once PCER/PCMR are enabled, which real IDF does at startup and this RTEMS
 * port never has. At 160 MHz one cycle is 6.25 ns, so the BLE scheduler's
 * ~940 us budget is ~150k cycles: plenty of resolution. */
#define DIAG_CSR_PCER 0x7e0
#define DIAG_CSR_PCMR 0x7e1
#define DIAG_CSR_PCCR 0x7e2

void diag_cycle_counter_enable(void)
{
    __asm__ volatile ("csrw %0, %1" :: "i" (DIAG_CSR_PCER), "r" (1));
    __asm__ volatile ("csrw %0, %1" :: "i" (DIAG_CSR_PCMR), "r" (1));
}

static inline uint32_t diag_cycles(void)
{
    uint32_t v;
    __asm__ volatile ("csrr %0, %1" : "=r" (v) : "i" (DIAG_CSR_PCCR));
    return v;
}

static inline uint32_t diag_sp(void)
{
    uint32_t v;
    __asm__ volatile ("mv %0, sp" : "=r" (v));
    return v;
}

static void diag_isr_trampoline(void *arg)
{
    struct intr_handle_data_s *h = arg;
    uint32_t before = diag_sp();

    uint32_t c0 = diag_cycles();

    diag_bt_isr_count++;
    diag_bt_isr_sp_in = before;

    h->handler(h->arg);

    uint32_t elapsed = diag_cycles() - c0;
    diag_bt_isr_last_cycles = elapsed;
    diag_bt_isr_total_cycles += elapsed;
    if (elapsed > 500u * 160u) {
        diag_bt_isr_slow_count++;
    }
    if (elapsed > diag_bt_isr_max_cycles) {
        diag_bt_isr_max_cycles = elapsed;
    }

    uint32_t after = diag_sp();
    diag_bt_isr_sp_out = after;
    if (after != before && diag_bt_isr_sp_mismatch == 0u) {
        diag_bt_isr_sp_mismatch = before;
    }
}
#endif

esp_err_t esp_intr_alloc(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t *ret_handle)
{
    (void) flags; /* see header: not yet mapped onto anything RTEMS-side */

    intr_handle_t h = malloc(sizeof(*h));
    if (h == NULL) {
        return ESP_FAIL;
    }
    h->vector  = (rtems_vector_number) source;
    h->handler = (rtems_interrupt_handler) handler;
    h->arg     = arg;

#if DIAG_INTR_ALLOC
    /* Install the trampoline instead, with the real handler carried in `h`. */
    rtems_status_code sc = RTEMS_TOO_MANY;
    if (diag_wrapped_count < sizeof(diag_wrapped) / sizeof(diag_wrapped[0])) {
        diag_wrapped[diag_wrapped_count++] = h;
        sc = rtems_interrupt_handler_install(
            h->vector, "BT", RTEMS_INTERRUPT_UNIQUE, diag_isr_trampoline, h
        );
    }
#else
    rtems_status_code sc = rtems_interrupt_handler_install(
        h->vector, "BT", RTEMS_INTERRUPT_UNIQUE, h->handler, h->arg
    );
#endif
#if DIAG_INTR_ALLOC
    printk("DIAG intr_alloc: source=%d flags=0x%x handler=0x%08x arg=0x%08x -> sc=%d\n",
           source, flags, (unsigned) (uintptr_t) handler, (unsigned) (uintptr_t) arg,
           (int) sc);
#endif
    if (sc != RTEMS_SUCCESSFUL) {
        free(h);
        return ESP_FAIL;
    }

    if (ret_handle != NULL) {
        *ret_handle = h;
    }
    return ESP_OK;
}

esp_err_t esp_intr_free(intr_handle_t handle)
{
    if (handle == NULL) {
        return ESP_FAIL;
    }
#if DIAG_INTR_ALLOC
    rtems_status_code sc = rtems_interrupt_handler_remove(handle->vector, diag_isr_trampoline, handle);
#else
    rtems_status_code sc = rtems_interrupt_handler_remove(handle->vector, handle->handler, handle->arg);
#endif
    free(handle);
    return (sc == RTEMS_SUCCESSFUL) ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_intr_enable(intr_handle_t handle)
{
    if (handle == NULL) {
        return ESP_FAIL;
    }
    rtems_status_code sc = rtems_interrupt_vector_enable(handle->vector);
#if DIAG_INTR_ALLOC
    printk("DIAG intr_enable: vector=%d -> sc=%d\n", (int) handle->vector, (int) sc);
#endif
    return (sc == RTEMS_SUCCESSFUL) ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_intr_disable(intr_handle_t handle)
{
    if (handle == NULL) {
        return ESP_FAIL;
    }
    rtems_status_code sc = rtems_interrupt_vector_disable(handle->vector);
    return (sc == RTEMS_SUCCESSFUL) ? ESP_OK : ESP_FAIL;
}
