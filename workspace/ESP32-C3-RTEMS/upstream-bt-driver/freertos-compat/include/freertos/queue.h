/*
 * Subset of FreeRTOS's queue.h needed by bt.c at IDF v5.3.1. Backed by
 * RTEMS message queues (src/queue.c) - see ../../README.md for the
 * blocking-send semantics mismatch this doesn't yet resolve.
 */
#ifndef FREERTOS_COMPAT_QUEUE_H
#define FREERTOS_COMPAT_QUEUE_H

#include "freertos/FreeRTOS.h"
#include <rtems.h>

/*
 * rtems_message_queue_send() takes an explicit per-call message size
 * (RTEMS messages aren't fixed-size the way a FreeRTOS queue's items are),
 * so the handle needs to carry the item size fixed at xQueueCreate() time
 * alongside the RTEMS queue id - a bare rtems_id isn't enough.
 */
typedef struct {
    rtems_id id;
    size_t   item_size;
} *QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);

BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken);

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);
BaseType_t xQueueReceiveFromISR(QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxHigherPriorityTaskWoken);

#endif /* FREERTOS_COMPAT_QUEUE_H */
