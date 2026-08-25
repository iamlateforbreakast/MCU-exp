/*
 * xQueueCreate/Send/Receive(+FromISR) on top of RTEMS message queues
 * (rtems_message_queue_create/send/receive - signatures confirmed against
 * real RTEMS `main` source, see ../../README.md).
 *
 * KNOWN SEMANTIC MISMATCH (flagged in the plan, not silently papered
 * over): RTEMS's rtems_message_queue_send() never blocks - if the queue
 * is full it returns RTEMS_TOO_MANY immediately. FreeRTOS's xQueueSend()
 * blocks up to xTicksToWait for space to free up. This shim does NOT
 * implement that blocking-send behavior (xTicksToWait is accepted but
 * ignored on the send path) - if bt.c is ever found to depend on a
 * blocking send actually blocking, this needs an auxiliary counting
 * semaphore for space-available signaling. Unverified whether bt.c's real
 * call sites ever hit this - check once the code actually builds/links.
 */
#include "freertos/queue.h"
#include <rtems/rtems/message.h>
#include <stdlib.h>

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
    QueueHandle_t q = malloc(sizeof(*q));
    if (q == NULL) {
        return NULL;
    }

    rtems_status_code sc = rtems_message_queue_create(
        rtems_build_name('f', 'r', 't', 'q'),
        (uint32_t) uxQueueLength,
        (size_t) uxItemSize,
        RTEMS_DEFAULT_ATTRIBUTES,
        &q->id
    );
    if (sc != RTEMS_SUCCESSFUL) {
        free(q);
        return NULL;
    }

    q->item_size = (size_t) uxItemSize;
    return q;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    (void) xTicksToWait; /* see file header: RTEMS send never blocks */
    rtems_status_code sc = rtems_message_queue_send(xQueue->id, pvItemToQueue, xQueue->item_size);
    return (sc == RTEMS_SUCCESSFUL) ? pdTRUE : pdFALSE;
}

BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken)
{
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE; /* see portmacro.h: RTEMS reschedules on interrupt exit automatically */
    }
    rtems_status_code sc = rtems_message_queue_send(xQueue->id, pvItemToQueue, xQueue->item_size);
    return (sc == RTEMS_SUCCESSFUL) ? pdTRUE : pdFALSE;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    rtems_option option = (xTicksToWait == 0) ? RTEMS_NO_WAIT : RTEMS_WAIT;
    rtems_interval timeout = (xTicksToWait == portMAX_DELAY) ? RTEMS_NO_TIMEOUT : (rtems_interval) xTicksToWait;
    size_t size = 0;

    rtems_status_code sc = rtems_message_queue_receive(xQueue->id, pvBuffer, &size, option, timeout);
    return (sc == RTEMS_SUCCESSFUL) ? pdTRUE : pdFALSE;
}

BaseType_t xQueueReceiveFromISR(QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxHigherPriorityTaskWoken)
{
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    size_t size = 0;
    /* ISR-safe only with RTEMS_NO_WAIT (confirmed against real RTEMS docs) - matches FreeRTOS's own non-blocking ReceiveFromISR semantics anyway. */
    rtems_status_code sc = rtems_message_queue_receive(xQueue->id, pvBuffer, &size, RTEMS_NO_WAIT, RTEMS_NO_TIMEOUT);
    return (sc == RTEMS_SUCCESSFUL) ? pdTRUE : pdFALSE;
}
