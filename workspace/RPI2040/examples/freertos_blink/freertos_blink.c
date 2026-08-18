/* FreeRTOS on the RP2040: two independent blink tasks running at different
 * rates, plus a producer/consumer pair passing tick counts through a queue.
 *
 * Contrast with examples/gpio_led_blink in this repo, which sequences LEDs
 * from a single bare-metal loop with sleep_ms() - here each LED is owned by
 * its own FreeRTOS task with its own period and priority, and vTaskDelay()
 * (not sleep_ms()) is used so the scheduler can run other tasks while a task
 * is waiting, instead of busy-blocking the whole core.
 *
 * Adjust LED_GPIO_FAST/SLOW below to match how the LEDs are actually wired -
 * GPIO 25 is the onboard LED on a Pico/Pico W; the other is a placeholder.
 *
 * Task/config setup (FreeRTOSConfig.h, the xTaskCreate calls, vTaskDelay,
 * queue send/receive) is grounded against the official pico-examples
 * freertos/hello_freertos example (raspberrypi/pico-examples), fetched and
 * cross-checked rather than written from memory - trimmed down to a single
 * core / dynamic-allocation-only build, since this repo's other examples
 * are all single-purpose rather than multi-variant.
 *
 * Two builds share this one source file (see CMakeLists.txt):
 * freertos_blink (single core, the default) and freertos_blink_dual_core
 * (configNUMBER_OF_CORES=2, set via a compiler define rather than editing
 * FreeRTOSConfig.h, the same technique the official example uses for its
 * own one-core/two-core variants). On the dual-core build, the RP2040 SMP
 * port's scheduler brings up core 1 itself inside vTaskStartScheduler() -
 * unlike raw pico_multicore usage, main() below doesn't need to call
 * multicore_launch_core1() itself. Each task prints which core it landed
 * on at startup (portGET_CORE_ID()) so you can see the scheduler actually
 * spread the four tasks across both cores instead of taking it on faith.
 */
#include <stdio.h>
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define LED_GPIO_FAST 25
#define LED_GPIO_SLOW 16

#define FAST_BLINK_PERIOD_MS 200
#define SLOW_BLINK_PERIOD_MS 700
#define PRODUCER_PERIOD_MS 2000

#define BLINK_FAST_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define BLINK_SLOW_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define PRODUCER_PRIORITY   (tskIDLE_PRIORITY + 1UL)
#define CONSUMER_PRIORITY   (tskIDLE_PRIORITY + 1UL)

static QueueHandle_t tick_queue;

static void init_led(uint gpio)
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 0);
}

static void blink_task(void *params)
{
    uint gpio = (uint)(uintptr_t)params;
    TickType_t period = (gpio == LED_GPIO_FAST) ? FAST_BLINK_PERIOD_MS : SLOW_BLINK_PERIOD_MS;
    bool on = false;

    init_led(gpio);
#if configNUMBER_OF_CORES > 1
    printf("%s running on core %d\n", pcTaskGetName(NULL), portGET_CORE_ID());
#endif
    while (true) {
        on = !on;
        gpio_put(gpio, on);
        vTaskDelay(pdMS_TO_TICKS(period));
    }
}

/* Sends an incrementing tick count on a timer - stands in for any sensor
 * poll or other periodic producer a real application would have. */
static void producer_task(void *params)
{
    (void)params;
    uint32_t count = 0;

#if configNUMBER_OF_CORES > 1
    printf("%s running on core %d\n", pcTaskGetName(NULL), portGET_CORE_ID());
#endif
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(PRODUCER_PERIOD_MS));
        count++;
        /* Non-blocking send: if the consumer somehow falls behind, drop the
         * tick rather than stall the producer - fine for a demo counter. */
        xQueueSend(tick_queue, &count, 0);
    }
}

/* Blocks on the queue and prints whatever the producer sends - demonstrates
 * task-to-task communication instead of every task working in isolation. */
static void consumer_task(void *params)
{
    (void)params;
    uint32_t received;

#if configNUMBER_OF_CORES > 1
    printf("%s running on core %d\n", pcTaskGetName(NULL), portGET_CORE_ID());
#endif
    while (true) {
        if (xQueueReceive(tick_queue, &received, portMAX_DELAY) == pdTRUE) {
            printf("consumer_task: producer tick #%u\n", (unsigned)received);
        }
    }
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000); /* give a USB CDC terminal time to attach before the first prints */

    printf("Starting FreeRTOS blink example\n");

    tick_queue = xQueueCreate(4, sizeof(uint32_t));
    hard_assert(tick_queue != NULL);

    xTaskCreate(blink_task, "BlinkFast", configMINIMAL_STACK_SIZE, (void *)(uintptr_t)LED_GPIO_FAST,
                BLINK_FAST_PRIORITY, NULL);
    xTaskCreate(blink_task, "BlinkSlow", configMINIMAL_STACK_SIZE, (void *)(uintptr_t)LED_GPIO_SLOW,
                BLINK_SLOW_PRIORITY, NULL);
    xTaskCreate(producer_task, "Producer", configMINIMAL_STACK_SIZE, NULL, PRODUCER_PRIORITY, NULL);
    xTaskCreate(consumer_task, "Consumer", configMINIMAL_STACK_SIZE, NULL, CONSUMER_PRIORITY, NULL);

    vTaskStartScheduler();

    /* Only reached if vTaskStartScheduler() fails (e.g. out of heap for the
     * idle/timer tasks) - the scheduler itself never returns otherwise. */
    printf("vTaskStartScheduler() returned - out of heap?\n");
    while (true) {
        tight_loop_contents();
    }
}
