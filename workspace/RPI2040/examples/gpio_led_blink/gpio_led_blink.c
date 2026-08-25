/* Drives three LEDs on the RP2040, one after another.
 * Adjust LED_GPIO_0/1/2 below to match how the LEDs are actually wired -
 * GPIO 25 is the onboard LED on a Pico/Pico W; the other two are placeholders. */
#include "pico/stdlib.h"

#define LED_GPIO_0 25
#define LED_GPIO_1 14
#define LED_GPIO_2 15

static const uint leds[] = {LED_GPIO_0, LED_GPIO_1, LED_GPIO_2};
static const int num_leds = sizeof(leds) / sizeof(leds[0]);

int main(void)
{
    for (int i = 0; i < num_leds; i++) {
        gpio_init(leds[i]);
        gpio_set_dir(leds[i], GPIO_OUT);
        gpio_put(leds[i], 0);
    }

    int active = 0;
    while (1) {
        for (int i = 0; i < num_leds; i++) {
            gpio_put(leds[i], i == active ? 1 : 0);
        }
        active = (active + 1) % num_leds;
        sleep_ms(300);
    }
}
