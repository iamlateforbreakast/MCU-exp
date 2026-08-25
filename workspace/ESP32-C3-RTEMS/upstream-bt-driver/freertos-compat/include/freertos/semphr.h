/*
 * Subset of FreeRTOS's semphr.h needed by bt.c/npl_os_freertos.c at IDF
 * v5.3.1. Backed by RTEMS semaphores (src/semphr.c): mutexes map to a
 * priority-inheritance binary semaphore, counting semaphores map to a
 * plain RTEMS counting semaphore - RTEMS only allows the latter to be
 * touched from ISR context (see ../../README.md), which matches how
 * bt.c actually calls these (grepped: xSemaphoreCreateMutex never paired
 * with a *FromISR call, only xSemaphoreCreateCounting is).
 */
#ifndef FREERTOS_COMPAT_SEMPHR_H
#define FREERTOS_COMPAT_SEMPHR_H

#include "freertos/FreeRTOS.h"
#include <rtems.h>

typedef rtems_id SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount);

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);

BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore, BaseType_t *pxHigherPriorityTaskWoken);

#endif /* FREERTOS_COMPAT_SEMPHR_H */
