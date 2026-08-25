/*
 * xTaskCreatePinnedToCore / vTaskDelete on top of RTEMS Classic API
 * (rtems_task_create/start/delete - signatures confirmed against real
 * RTEMS `main` source, see ../../README.md).
 */
#include "freertos/task.h"
#include <rtems/rtems/tasks.h>
#include <stdlib.h>
#include <string.h>

/*
 * FreeRTOS priorities run 0 (lowest) .. FREERTOS_COMPAT_MAX_PRIORITIES-1
 * (highest). RTEMS priorities run 1 (highest) .. higher-number-is-lower,
 * with no fixed compile-time ceiling (RTEMS_MAXIMUM_PRIORITY is
 * scheduler-dependent at runtime). Invert into RTEMS's numbering and clamp
 * to RTEMS_MINIMUM_PRIORITY at the top end; the bottom end isn't clamped
 * since exceeding FREERTOS_COMPAT_MAX_PRIORITIES just yields a very low
 * (but still valid on any real RTEMS scheduler) RTEMS priority.
 */
static rtems_task_priority freertos_compat_invert_priority(UBaseType_t uxPriority)
{
    UBaseType_t inverted;

    if (uxPriority >= FREERTOS_COMPAT_MAX_PRIORITIES) {
        uxPriority = FREERTOS_COMPAT_MAX_PRIORITIES - 1;
    }

    inverted = (FREERTOS_COMPAT_MAX_PRIORITIES - uxPriority);

    if (inverted < RTEMS_MINIMUM_PRIORITY) {
        inverted = RTEMS_MINIMUM_PRIORITY;
    }

    return (rtems_task_priority) inverted;
}

/*
 * RTEMS task names are packed 4-character rtems_name values, not retained
 * strings - pcName is truncated to its first 4 characters (space-padded);
 * unlike FreeRTOS, nothing here supports looking a task up by full name
 * later. Not believed to matter for bt.c/NimBLE, which only use the name
 * for debug logging.
 */
static rtems_name freertos_compat_task_name(const char *pcName)
{
    char c[4] = { ' ', ' ', ' ', ' ' };
    size_t i;

    for (i = 0; i < 4 && pcName != NULL && pcName[i] != '\0'; ++i) {
        c[i] = pcName[i];
    }

    return rtems_build_name(c[0], c[1], c[2], c[3]);
}

struct freertos_compat_task_trampoline_args {
    TaskFunction_t pxTaskCode;
    void          *pvParameters;
};

static rtems_task freertos_compat_task_trampoline(rtems_task_argument arg)
{
    struct freertos_compat_task_trampoline_args *args =
        (struct freertos_compat_task_trampoline_args *) arg;
    TaskFunction_t pxTaskCode  = args->pxTaskCode;
    void          *pvParameters = args->pvParameters;

    free(args);
    pxTaskCode(pvParameters);

    /*
     * bt.c's tasks are expected to run forever (or call vTaskDelete(NULL)
     * themselves); if one ever does return, delete self rather than fall
     * off the end of an RTEMS task body.
     */
    rtems_task_delete(RTEMS_SELF);
}

BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t                pxTaskCode,
    const char * const            pcName,
    const configSTACK_DEPTH_TYPE  usStackDepth,
    void * const                  pvParameters,
    UBaseType_t                   uxPriority,
    TaskHandle_t * const          pxCreatedTask,
    const BaseType_t              xCoreID
)
{
    /* ESP32-C3 is single-core - xCoreID (incl. tskNO_AFFINITY) is moot. */
    (void) xCoreID;

    struct freertos_compat_task_trampoline_args *args =
        malloc(sizeof(*args));
    if (args == NULL) {
        return pdFAIL;
    }
    args->pxTaskCode   = pxTaskCode;
    args->pvParameters = pvParameters;

    rtems_id id;
    rtems_status_code sc = rtems_task_create(
        freertos_compat_task_name(pcName),
        freertos_compat_invert_priority(uxPriority),
        (size_t) usStackDepth, /* ESP-IDF's usStackDepth is bytes, same unit RTEMS expects - unlike vanilla FreeRTOS, which counts words */
        RTEMS_DEFAULT_MODES,
        RTEMS_DEFAULT_ATTRIBUTES | RTEMS_FLOATING_POINT,
        &id
    );
    if (sc != RTEMS_SUCCESSFUL) {
        free(args);
        return pdFAIL;
    }

    sc = rtems_task_start(id, freertos_compat_task_trampoline, (rtems_task_argument) args);
    if (sc != RTEMS_SUCCESSFUL) {
        rtems_task_delete(id);
        free(args);
        return pdFAIL;
    }

    if (pxCreatedTask != NULL) {
        *pxCreatedTask = id;
    }
    return pdPASS;
}

void vTaskDelete(TaskHandle_t xTaskToDelete)
{
    rtems_task_delete(xTaskToDelete == 0 ? RTEMS_SELF : xTaskToDelete);
}
