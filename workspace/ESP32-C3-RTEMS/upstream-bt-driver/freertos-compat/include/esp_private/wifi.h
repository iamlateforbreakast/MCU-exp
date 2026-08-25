/*
 * Minimal stand-in for ESP-IDF's real `esp_private/wifi.h` (part of the
 * `esp_wifi` component - a whole WiFi type/event subsystem
 * `#include`ing `esp_wifi_types.h`/`esp_event.h`/`esp_wifi.h`/
 * `esp_smartconfig.h`/`wifi_types.h`) - this BLE-only profile has no WiFi
 * component vendored at all. `phy_init.c` only needs one real declared
 * type from it, `wifi_mac_time_update_cb_t` (confirmed by grep,
 * `phy_init.c:58`), used as `extern wifi_mac_time_update_cb_t
 * s_wifi_mac_time_update_cb;` - a callback that's declared but never
 * actually registered/invoked without a real WiFi stack, so only the
 * type itself needs to exist, not the real subsystem behind it.
 */
#ifndef FREERTOS_COMPAT_ESP_PRIVATE_WIFI_H
#define FREERTOS_COMPAT_ESP_PRIVATE_WIFI_H

#include <stdint.h>
#include "esp_err.h"

typedef esp_err_t (* wifi_mac_time_update_cb_t)(uint32_t time_delta);

#endif /* FREERTOS_COMPAT_ESP_PRIVATE_WIFI_H */
