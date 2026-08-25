/*
 * Minimal esp_sleep.h subset - only esp_deep_sleep_register_phy_hook,
 * confirmed this session as the one deep-sleep call phy_init.c actually
 * makes (components/esp_phy/src/phy_init.c:897/901, real ESP-IDF v5.3.1).
 * Real implementation (components/esp_hw_support/sleep_modes.c) just
 * appends the callback to a fixed-size array so it can be invoked when
 * deep sleep is entered - safe to no-op here since this RTEMS port has no
 * deep-sleep subsystem at all and isn't attempting one for BLE bring-up.
 */
#ifndef FREERTOS_COMPAT_ESP_SLEEP_H
#define FREERTOS_COMPAT_ESP_SLEEP_H

#include "esp_err.h"

typedef void (*esp_deep_sleep_cb_t)(void);

esp_err_t esp_deep_sleep_register_phy_hook(esp_deep_sleep_cb_t new_dslp_cb);

#endif /* FREERTOS_COMPAT_ESP_SLEEP_H */
