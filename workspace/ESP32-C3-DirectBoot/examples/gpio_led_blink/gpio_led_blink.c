/* Drives three LEDs on the ESP32-C3, one after another, running directly from
 * flash via Espressif's "direct boot" feature (see ../../ESP32-C3-DirectBoot.md)
 * instead of ESP-IDF/FreeRTOS - no 2nd stage bootloader, so there's no framework
 * around app_main() to disable the watchdogs or init the GPIO driver for us.
 * Adapted from the upstream esp32c3-direct-boot-example blink example
 * (https://github.com/espressif/esp32c3-direct-boot-example/blob/main/examples/blink/blink.c),
 * driving three GPIOs in sequence to match this repo's gpio_led_blink convention
 * instead of a single LED.
 *
 * Adjust LED_GPIO_0/1/2 below to match how the LEDs are actually wired - these
 * are placeholders, not a specific dev board's onboard LEDs. */
#include "hal/gpio_hal.h"
#include "hal/wdt_hal.h"

#define LED_GPIO_0 2
#define LED_GPIO_1 3
#define LED_GPIO_2 4

static const int leds[] = {LED_GPIO_0, LED_GPIO_1, LED_GPIO_2};
static const int num_leds = sizeof(leds) / sizeof(leds[0]);

static void delay(void);

int main(void)
{
    /* Disable the watchdogs. In ESP-IDF this is done by the 2nd stage
     * bootloader before the app ever runs; direct boot skips that stage, so
     * we have to do it ourselves before the flashboot RWDT/MWDT timeouts fire
     * and reset the chip mid-blink. */
    wdt_hal_context_t rwdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rwdt_ctx);
    wdt_hal_disable(&rwdt_ctx);
    wdt_hal_set_flashboot_en(&rwdt_ctx, false);
    wdt_hal_context_t mwdt_ctx = {.inst = WDT_MWDT0, .mwdt_dev = &TIMERG0};
    wdt_hal_write_protect_disable(&mwdt_ctx);
    wdt_hal_disable(&mwdt_ctx);
    wdt_hal_set_flashboot_en(&mwdt_ctx, false);
    /* Super WDT is still enabled; no HAL API for it yet (same caveat as upstream). */

    gpio_hal_context_t gpio_hal = {
        .dev = GPIO_HAL_GET_HW(GPIO_PORT_0)
    };
    for (int i = 0; i < num_leds; i++) {
        gpio_hal_func_sel(&gpio_hal, leds[i], PIN_FUNC_GPIO);
        gpio_hal_output_enable(&gpio_hal, leds[i]);
        gpio_hal_set_level(&gpio_hal, leds[i], 0);
    }

    int active = 0;
    while (1) {
        for (int i = 0; i < num_leds; i++) {
            gpio_hal_set_level(&gpio_hal, leds[i], i == active ? 1 : 0);
        }
        active = (active + 1) % num_leds;
        delay();
    }

    return 0;
}

static void delay(void)
{
    /* No timer HAL wired up here, so this is a busy-loop like upstream's -
     * around 160ms per step at typical clock speeds. */
    for (int i = 0; i < 300000; i++) {
        asm volatile ("nop");
    }
}
