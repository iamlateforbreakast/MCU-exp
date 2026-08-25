/*
 * Subset of FreeRTOS's task.h needed by bt.c/npl_os_freertos.c at IDF
 * v5.3.1: task creation/deletion only (no notifications, no scheduler
 * control - neither is used by the 31-function surface this shim targets,
 * see ../../README.md).
 *
 * `TaskHandle_t` is a pointer (`rtems_id *`), not a bare `rtems_id` - same
 * reasoning as `SemaphoreHandle_t` in semphr.h: real FreeRTOS's
 * `TaskHandle_t` is itself a pointer type, and bt.c's `task_delete_wrapper`
 * (`bt.c:678-680`) receives it as a plain `void *` and passes it straight
 * to `vTaskDelete` - confirmed necessary by actually compiling bt.c
 * 2026-08-25, not stylistic.
 */
#ifndef FREERTOS_COMPAT_TASK_H
#define FREERTOS_COMPAT_TASK_H

#include "freertos/FreeRTOS.h"
#include <rtems.h>

typedef rtems_id *TaskHandle_t;
typedef void (* TaskFunction_t)(void *pvParameters);

#define tskNO_AFFINITY ((BaseType_t) -1)

BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t                pxTaskCode,
    const char * const            pcName,
    const configSTACK_DEPTH_TYPE  usStackDepth,
    void * const                  pvParameters,
    UBaseType_t                   uxPriority,
    TaskHandle_t * const          pxCreatedTask,
    const BaseType_t              xCoreID
);

void vTaskDelete(TaskHandle_t xTaskToDelete);

#endif /* FREERTOS_COMPAT_TASK_H */
