/*
 * Subset of FreeRTOS's semphr.h needed by bt.c/npl_os_freertos.c at IDF
 * v5.3.1. Backed by RTEMS semaphores (src/semphr.c): mutexes map to a
 * priority-inheritance binary semaphore, counting semaphores map to a
 * plain RTEMS counting semaphore - RTEMS only allows the latter to be
 * touched from ISR context (see ../../README.md), which matches how
 * bt.c actually calls these (grepped: xSemaphoreCreateMutex never paired
 * with a *FromISR call, only xSemaphoreCreateCounting is).
 *
 * `SemaphoreHandle_t` is a pointer (`rtems_id *`), not a bare `rtems_id` -
 * confirmed necessary, not stylistic, once bt.c was actually vendored and
 * compiled (2026-08-25): real FreeRTOS's `SemaphoreHandle_t` is itself a
 * pointer type (`struct QueueDefinition *`), and bt.c relies on that,
 * e.g. `semphr->handle = (void *)xSemaphoreCreateCounting(...)`
 * (`bt.c:555`) then passing that `void *` field back into
 * `xSemaphoreTakeFromISR`/`GiveFromISR` - an implicit pointer-typed
 * round-trip a bare integer handle can't satisfy without a cast this
 * shim isn't allowed to add (bt.c is vendored unmodified). `QueueHandle_t`
 * in `queue.h` already got this right; this header didn't, until now -
 * mirrors that file's own pattern instead of introducing a second one.
 */
#ifndef FREERTOS_COMPAT_SEMPHR_H
#define FREERTOS_COMPAT_SEMPHR_H

#include "freertos/FreeRTOS.h"
#include <rtems.h>

typedef rtems_id *SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount);

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);

BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken);
void vSemaphoreDelete(SemaphoreHandle_t xSemaphore);

#endif /* FREERTOS_COMPAT_SEMPHR_H */
