/*
 * Minimal portmacro.h subset needed by ESP-IDF's esp32c3 BLE controller
 * (components/bt/controller/esp32c3/bt.c) and NimBLE's FreeRTOS NPL port
 * (components/bt/porting/npl/freertos/src/npl_os_freertos.c) at IDF v5.3.1.
 * See ../../README.md for what was actually cross-checked against real
 * ESP-IDF/RTEMS source vs. what's still a design assumption.
 */
#ifndef FREERTOS_COMPAT_PORTMACRO_H
#define FREERTOS_COMPAT_PORTMACRO_H

#include <stdint.h>
#include <rtems.h>

typedef uint32_t TickType_t;
typedef long     BaseType_t;
typedef unsigned long UBaseType_t;

#define pdFALSE ((BaseType_t) 0)
#define pdTRUE  ((BaseType_t) 1)
#define pdFAIL   pdFALSE
#define pdPASS   pdTRUE

#define portMAX_DELAY ((TickType_t) 0xffffffffUL)

/*
 * ESP-IDF's portENTER_CRITICAL/portEXIT_CRITICAL take a portMUX_TYPE*
 * spinlock (SMP FreeRTOS). This session's recon confirmed bt.c calls
 * portENTER_CRITICAL/portEXIT_CRITICAL/_ISR by name (grep) but did NOT
 * independently confirm their real argument signature against IDF's own
 * portmacro.h - modeled here as taking a portMUX_TYPE* per IDF's SMP
 * convention; re-check against real ESP-IDF source before trusting this.
 *
 * ESP32-C3 is single-core, so no real spinlock algorithm is needed for
 * cross-core exclusion - src/critical.c just disables/restores interrupts.
 * What portMUX_TYPE does need to carry: the saved interrupt level (so
 * separate enter/exit calls can restore the right state) and a nesting
 * count (IDF's critical sections nest recursively on the same spinlock,
 * only actually toggling interrupts at the outermost level).
 */
typedef struct {
    rtems_interrupt_level level;
    unsigned int          nesting;
} portMUX_TYPE;

#define portMUX_INITIALIZER_UNLOCKED { 0, 0 }

/*
 * FreeRTOS's *FromISR calls conventionally end with
 * portYIELD_FROM_ISR(xHigherPriorityTaskWoken) to request a reschedule.
 * RTEMS reschedules automatically on interrupt exit when a
 * higher-priority task was unblocked, so this is a no-op here.
 * UNVERIFIED against this BSP's actual interrupt-exit path - re-check once
 * Phase 3 (VHCI smoke test) can exercise it on real hardware.
 */
#define portYIELD_FROM_ISR(x) ((void) (x))

void freertos_compat_enter_critical(portMUX_TYPE *mux);
void freertos_compat_exit_critical(portMUX_TYPE *mux);

#define portENTER_CRITICAL(mux)     freertos_compat_enter_critical(mux)
#define portEXIT_CRITICAL(mux)      freertos_compat_exit_critical(mux)
#define portENTER_CRITICAL_ISR(mux) freertos_compat_enter_critical(mux)
#define portEXIT_CRITICAL_ISR(mux)  freertos_compat_exit_critical(mux)

#endif /* FREERTOS_COMPAT_PORTMACRO_H */
