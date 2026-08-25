/*
 * Minimal stand-in for ESP-IDF's real `driver/gpio.h` (the
 * `esp_driver_gpio` component - a full GPIO driver with ISR management,
 * glitch filters, etc., not vendored here). `phy_common.c`'s only use
 * (`phy_ant_set_gpio_output`, antenna-switch GPIO config for external RF
 * switches) sets exactly 4 fields of `gpio_config_t` and calls
 * `gpio_config()` once - confirmed by reading the real, unmodified call
 * site, not guessed. Not exercised by anything this BLE-only,
 * single-antenna profile's own call graph reaches (`esp_phy_set_ant_gpio`
 * is a public antenna-switch API bt.c itself never calls) - `gpio_config`
 * is a real no-op stub rather than backed by this repo's own real
 * `upstream-gpio-driver/` register-level driver, since nothing in this
 * profile depends on it actually configuring a pin.
 */
#ifndef FREERTOS_COMPAT_DRIVER_GPIO_H
#define FREERTOS_COMPAT_DRIVER_GPIO_H

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    GPIO_INTR_DISABLE = 0,
} gpio_int_type_t;

typedef enum {
    GPIO_MODE_OUTPUT = 2,
} gpio_mode_t;

typedef enum {
    GPIO_PULLUP_DISABLE = 0,
} gpio_pullup_t;

typedef enum {
    GPIO_PULLDOWN_DISABLE = 0,
} gpio_pulldown_t;

typedef struct {
    uint64_t         pin_bit_mask;
    gpio_mode_t      mode;
    gpio_pullup_t    pull_up_en;
    gpio_pulldown_t  pull_down_en;
    gpio_int_type_t  intr_type;
} gpio_config_t;

esp_err_t gpio_config(const gpio_config_t *pGPIOConfig);

#endif /* FREERTOS_COMPAT_DRIVER_GPIO_H */
