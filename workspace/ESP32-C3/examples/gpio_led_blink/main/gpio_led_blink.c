/* Drives three LEDs on the ESP32-C3, one after another.
 * Adjust LED_GPIO_0/1/2 below to match how the LEDs are actually wired -
 * these are placeholders, not a specific dev board's onboard LEDs. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_GPIO_0 GPIO_NUM_4
#define LED_GPIO_1 GPIO_NUM_5
#define LED_GPIO_2 GPIO_NUM_6

static const gpio_num_t leds[] = {LED_GPIO_0, LED_GPIO_1, LED_GPIO_2};
static const int num_leds = sizeof(leds) / sizeof(leds[0]);

void app_main(void)
{
    for (int i = 0; i < num_leds; i++) {
        gpio_reset_pin(leds[i]);
        gpio_set_direction(leds[i], GPIO_MODE_OUTPUT);
        gpio_set_level(leds[i], 0);
    }

    int active = 0;
    while (1) {
        for (int i = 0; i < num_leds; i++) {
            gpio_set_level(leds[i], i == active ? 1 : 0);
        }
        active = (active + 1) % num_leds;
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
