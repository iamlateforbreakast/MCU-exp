/*
 * portENTER_CRITICAL/portEXIT_CRITICAL(+_ISR) backed by RTEMS's
 * rtems_interrupt_disable/rtems_interrupt_enable. These two are RTEMS's
 * oldest, most stable interrupt-level API (unchanged across RTEMS
 * releases) and weren't re-verified against real source this session the
 * way task/queue/sem/timer/intr were - low risk, but flagged per this
 * repo's own convention of only marking things Confirmed once actually
 * checked.
 *
 * Nests per-spinlock: IDF's critical sections can be entered recursively
 * on the same portMUX_TYPE from the same task, and only the outermost
 * enter/exit should actually toggle interrupts. ESP32-C3 is single-core,
 * so unlike real IDF this never needs an actual spinlock loop for
 * cross-core exclusion - see portmacro.h.
 */
#include "freertos/portmacro.h"
#include <rtems/rtems/intr.h>
#include <stdint.h>

#ifndef DIAG_INTR_ALLOC
#define DIAG_INTR_ALLOC 0
#endif

#if DIAG_INTR_ALLOC
/*
 * DIAG (2026-09-14): measures how long interrupts stay globally masked.
 *
 * rtems_interrupt_disable() clears mstatus.MIE, so it blocks EVERYTHING -
 * unlike real IDF, whose RISC-V portENTER_CRITICAL raises mintthresh and so
 * still lets higher-priority interrupts through. If some critical section
 * here holds interrupts off for a long time, it delays the START of the BT
 * controller ISR, which is exactly what makes r_sch_prog_ble_push_hack miss
 * its ~940 us programming deadline and emit BLE_ERR.
 *
 * Task-context and ISR-context windows are tracked separately on purpose:
 * only the task-context ones can delay the BT ISR starting. The caller
 * address of the worst offender is recorded so it can be resolved with
 * addr2line afterwards.
 *
 * LIMITATION: this covers only critical sections taken through THIS shim
 * (bt.c and the closed blob). RTEMS's own internal interrupt-disable windows
 * - inside the scheduler, object allocation, message queues - are not
 * visible here, so a low reading does not fully exonerate masking.
 */
volatile uint32_t diag_crit_max_task_cycles;
volatile uint32_t diag_crit_max_isr_cycles;
volatile uint32_t diag_crit_max_task_caller;
volatile uint32_t diag_crit_max_isr_caller;
volatile uint32_t diag_crit_count;

static inline uint32_t diag_crit_cycles(void)
{
    uint32_t v;
    /* ESP32-C3 performance-counter CSR; enabled by diag_cycle_counter_enable(). */
    __asm__ volatile ("csrr %0, 0x7e2" : "=r" (v));
    return v;
}
#endif

void freertos_compat_enter_critical(portMUX_TYPE *mux)
{
    if (mux->nesting == 0) {
        rtems_interrupt_disable(mux->level);
#if DIAG_INTR_ALLOC
        mux->diag_t0 = diag_crit_cycles();
        mux->diag_caller = (unsigned int) (uintptr_t) __builtin_return_address(0);
#endif
    }
    mux->nesting++;
}

void freertos_compat_exit_critical(portMUX_TYPE *mux)
{
    mux->nesting--;
    if (mux->nesting == 0) {
#if DIAG_INTR_ALLOC
        uint32_t held = diag_crit_cycles() - mux->diag_t0;

        diag_crit_count++;
        if (rtems_interrupt_is_in_progress()) {
            if (held > diag_crit_max_isr_cycles) {
                diag_crit_max_isr_cycles = held;
                diag_crit_max_isr_caller = mux->diag_caller;
            }
        } else if (held > diag_crit_max_task_cycles) {
            diag_crit_max_task_cycles = held;
            diag_crit_max_task_caller = mux->diag_caller;
        }
#endif
        rtems_interrupt_enable(mux->level);
    }
}

BaseType_t xPortInIsrContext(void)
{
    return rtems_interrupt_is_in_progress() ? pdTRUE : pdFALSE;
}
