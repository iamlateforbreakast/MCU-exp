/*
 * xSemaphoreCreate{Mutex,Counting}/Take/Give(+FromISR) on top of RTEMS
 * semaphores (rtems_semaphore_create/obtain/release - signatures confirmed
 * against real RTEMS `main` source, see ../../README.md).
 *
 * RTEMS only allows RTEMS_COUNTING_SEMAPHORE and
 * RTEMS_SIMPLE_BINARY_SEMAPHORE to be touched from interrupt context - a
 * priority-inheritance mutex (RTEMS_BINARY_SEMAPHORE|RTEMS_INHERIT_PRIORITY)
 * may not be. This matches how bt.c actually calls these (grepped: its
 * xSemaphoreCreateMutex is never paired with a *FromISR call, only
 * xSemaphoreCreateCounting is) - so the mutex path here doesn't need to be
 * ISR-safe, and isn't.
 *
 * SemaphoreHandle_t is `rtems_id *` (heap-allocated) - see semphr.h's
 * header comment for why a bare rtems_id doesn't work once bt.c is
 * actually compiled against it.
 */
#include "freertos/semphr.h"
#include <rtems/rtems/sem.h>
#include <stdlib.h>

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    rtems_id *handle = malloc(sizeof(*handle));
    if (handle == NULL) {
        return NULL;
    }
    rtems_status_code sc = rtems_semaphore_create(
        rtems_build_name('f', 'r', 'm', 'x'),
        1,
        RTEMS_BINARY_SEMAPHORE | RTEMS_INHERIT_PRIORITY | RTEMS_PRIORITY,
        0,
        handle
    );
    if (sc != RTEMS_SUCCESSFUL) {
        free(handle);
        return NULL;
    }
    return handle;
}

SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount)
{
    /* RTEMS counting semaphores have no separate ceiling - uxMaxCount is
     * unused (the initial count is all that's needed). */
    (void) uxMaxCount;

    rtems_id *handle = malloc(sizeof(*handle));
    if (handle == NULL) {
        return NULL;
    }
    rtems_status_code sc = rtems_semaphore_create(
        rtems_build_name('f', 'r', 'c', 't'),
        (uint32_t) uxInitialCount,
        RTEMS_COUNTING_SEMAPHORE,
        0,
        handle
    );
    if (sc != RTEMS_SUCCESSFUL) {
        free(handle);
        return NULL;
    }
    return handle;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait)
{
    rtems_option option = (xTicksToWait == 0) ? RTEMS_NO_WAIT : RTEMS_WAIT;
    rtems_interval timeout = (xTicksToWait == portMAX_DELAY) ? RTEMS_NO_TIMEOUT : (rtems_interval) xTicksToWait;

    rtems_status_code sc = rtems_semaphore_obtain(*xSemaphore, option, timeout);
    return (sc == RTEMS_SUCCESSFUL) ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    rtems_status_code sc = rtems_semaphore_release(*xSemaphore);
    return (sc == RTEMS_SUCCESSFUL) ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken)
{
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    /* Non-blocking, same as FreeRTOS's own TakeFromISR - caller must only
     * pass a counting semaphore here (see file header). */
    rtems_status_code sc = rtems_semaphore_obtain(*xSemaphore, RTEMS_NO_WAIT, RTEMS_NO_TIMEOUT);
    return (sc == RTEMS_SUCCESSFUL) ? pdTRUE : pdFALSE;
}

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken)
{
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    rtems_status_code sc = rtems_semaphore_release(*xSemaphore);
    return (sc == RTEMS_SUCCESSFUL) ? pdTRUE : pdFALSE;
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
    if (xSemaphore == NULL) {
        return;
    }
    rtems_semaphore_delete(*xSemaphore);
    free(xSemaphore);
}
