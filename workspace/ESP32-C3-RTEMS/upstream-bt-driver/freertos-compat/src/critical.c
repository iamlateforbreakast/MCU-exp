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

void freertos_compat_enter_critical(portMUX_TYPE *mux)
{
    if (mux->nesting == 0) {
        rtems_interrupt_disable(mux->level);
    }
    mux->nesting++;
}

void freertos_compat_exit_critical(portMUX_TYPE *mux)
{
    mux->nesting--;
    if (mux->nesting == 0) {
        rtems_interrupt_enable(mux->level);
    }
}
