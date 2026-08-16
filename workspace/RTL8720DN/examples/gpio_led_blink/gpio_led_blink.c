/* Drives three LEDs on the RTL8720DN (BW16), one after another, using the raw GPIO
 * API from ameba-rtos's example/peripheral/raw/GPIO/raw_gpio_rw example.
 *
 * Adjust LED_PIN_0/1/2 below to match how the LEDs are actually wired - these are
 * placeholders. Per the SDK: "PA_12 map to GPIOA_12, PB_8 map to GPIOB_8 and etc".
 *
 * See ../../RTL8720DN.md for how to drop this into the KM4 (High Power) core's
 * project_hp/src_hp/main.c and build it.
 */
#include "ameba_soc.h"

#define LED_PIN_0 PA_20
#define LED_PIN_1 PA_21
#define LED_PIN_2 PA_22

static const u32 leds[] = {LED_PIN_0, LED_PIN_1, LED_PIN_2};
static const int num_leds = sizeof(leds) / sizeof(leds[0]);

void gpio_led_blink(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    for (int i = 0; i < num_leds; i++) {
        GPIO_InitStruct.GPIO_Pin = leds[i];
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
        GPIO_Init(&GPIO_InitStruct);
        GPIO_WriteBit(leds[i], 0);
    }

    int active = 0;
    while (1) {
        for (int i = 0; i < num_leds; i++) {
            GPIO_WriteBit(leds[i], i == active ? 1 : 0);
        }
        active = (active + 1) % num_leds;
        DelayMs(300);
    }
}
