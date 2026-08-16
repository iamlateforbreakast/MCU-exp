/* Drives three LEDs on the AI Thinker M62-M2-I-Kit, one after another, using the
 * bflb_gpio API from the SDK's own examples/peripherals/gpio/gpio_input_output
 * example (the M6X modules are built on Bouffalo Lab BL61x/BL60x RISC-V SoCs).
 *
 * Adjust LED_PIN_0/1/2 below to match how the LEDs are actually wired - these are
 * placeholders. Build like the other examples under examples/peripherals/, e.g.
 * `make CHIP=bl616 BOARD=bl616dk` from this directory.
 */
#include "bflb_gpio.h"
#include "board.h"

#define LED_PIN_0 GPIO_PIN_0
#define LED_PIN_1 GPIO_PIN_1
#define LED_PIN_2 GPIO_PIN_2

static struct bflb_device_s *gpio;
static const uint8_t leds[] = {LED_PIN_0, LED_PIN_1, LED_PIN_2};
static const int num_leds = sizeof(leds) / sizeof(leds[0]);

int main(void)
{
    board_init();

    gpio = bflb_device_get_by_name("gpio");
    for (int i = 0; i < num_leds; i++) {
        bflb_gpio_init(gpio, leds[i], GPIO_OUTPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_0);
        bflb_gpio_reset(gpio, leds[i]);
    }

    int active = 0;
    while (1) {
        for (int i = 0; i < num_leds; i++) {
            if (i == active) {
                bflb_gpio_set(gpio, leds[i]);
            } else {
                bflb_gpio_reset(gpio, leds[i]);
            }
        }
        active = (active + 1) % num_leds;
        bflb_mtimer_delay_ms(300);
    }
}
