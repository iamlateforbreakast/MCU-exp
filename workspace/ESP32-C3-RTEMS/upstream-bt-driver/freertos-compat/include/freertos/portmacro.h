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
 * Real FreeRTOS: `portTICK_PERIOD_MS = 1000 / configTICK_RATE_HZ`. Fixed
 * at 10 (100Hz ticks) rather than reading RTEMS's actual configured tick
 * rate at runtime - matches both this BSP's own default
 * (`CONFIGURE_MICROSECONDS_PER_TICK` = 10000us, confirmed in Phase 1's
 * recon, see ../../README.md) and real IDF's own default
 * `CONFIG_FREERTOS_HZ` = 100. If either default is ever overridden this
 * needs to become a real variable read at runtime instead of a macro.
 */
#define portTICK_PERIOD_MS ((TickType_t) 10)

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
 *
 * Variadic (accepts zero or one argument): bt.c's only call site
 * (`bt.c:546`) uses the older, non-SMP zero-argument form
 * (`portYIELD_FROM_ISR();`), confirmed by actually compiling bt.c
 * 2026-08-25 - not the SMP-kernel `(xHigherPriorityTaskWoken)` form this
 * macro originally assumed.
 */
#define portYIELD_FROM_ISR(...) ((void) 0)

void freertos_compat_enter_critical(portMUX_TYPE *mux);
void freertos_compat_exit_critical(portMUX_TYPE *mux);

#define portENTER_CRITICAL(mux)     freertos_compat_enter_critical(mux)
#define portEXIT_CRITICAL(mux)      freertos_compat_exit_critical(mux)
#define portENTER_CRITICAL_ISR(mux) freertos_compat_enter_critical(mux)
#define portEXIT_CRITICAL_ISR(mux)  freertos_compat_exit_critical(mux)

/*
 * portENTER_CRITICAL_SAFE/portEXIT_CRITICAL_SAFE - real IDF's "call this
 * from either task or ISR context, it figures out which" variant (used by
 * PHY init's real esp_hw_support/periph_ctrl.c, confirmed this session:
 * wifi_bt_common_module_enable() - esp_phy_common_clock_enable()'s
 * dependency - uses this, not the plain portENTER_CRITICAL). Since this
 * shim's enter/exit already just disables/restores interrupts regardless
 * of caller context (no separate ISR-context code path the way real IDF's
 * SMP FreeRTOS has), the plain and _SAFE variants are identical here.
 */
#define portENTER_CRITICAL_SAFE(mux) freertos_compat_enter_critical(mux)
#define portEXIT_CRITICAL_SAFE(mux)  freertos_compat_exit_critical(mux)

/*
 * `vPortYield`/`xPortInIsrContext` - real FreeRTOS port functions
 * (`freertos/portable.h` in real IDF), not declared anywhere bt.c
 * `#include`s on its own (confirmed 2026-08-25: neither is declared in
 * ESP-IDF v5.3.1's real esp32c3/bt.c include chain either - same
 * situation as `esp_intr_alloc`, see ../../README.md's note on
 * force-including esp_intr_alloc.h). `bt.c` uses `vPortYield` once, as
 * `osi_funcs_t`'s `_task_yield` callback (`bt.c:357`) - implemented
 * (src/task.c) via `rtems_task_wake_after(RTEMS_YIELD_PROCESSOR)`,
 * RTEMS's standard Classic API yield primitive. `xPortInIsrContext`
 * detects whether the caller is in ISR context - implemented (src/
 * critical.c) via `rtems_interrupt_is_in_progress()`.
 */
void vPortYield(void);
BaseType_t xPortInIsrContext(void);

#endif /* FREERTOS_COMPAT_PORTMACRO_H */
