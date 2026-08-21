/* Drives the five onboard LEDs of the Ai-Thinker M62-12F Kit (Zephyr board
 * "ai_m62_12f_kit") in sequence, one at a time.
 *
 * Unlike this repo's other gpio_led_blink examples, these aren't placeholder
 * pins: they're the node labels from the board's own devicetree upstream in
 * Zephyr (boards/aithinker/ai_m62_12f_kit/ai_m62_12f_kit.dts), so this drives
 * the kit's real onboard LEDs with no wiring or overlay needed.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define DELAY_MS 300

static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(green_led), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(red_led), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(white_led), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(warmwhite_led), gpios),
};

#define NUM_LEDS (sizeof(leds) / sizeof(leds[0]))

int main(void)
{
	for (size_t i = 0; i < NUM_LEDS; i++) {
		if (!gpio_is_ready_dt(&leds[i])) {
			return 0;
		}
		gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
	}

	size_t active = 0;

	while (1) {
		for (size_t i = 0; i < NUM_LEDS; i++) {
			gpio_pin_set_dt(&leds[i], i == active ? 1 : 0);
		}
		active = (active + 1) % NUM_LEDS;
		k_msleep(DELAY_MS);
	}
	return 0;
}
