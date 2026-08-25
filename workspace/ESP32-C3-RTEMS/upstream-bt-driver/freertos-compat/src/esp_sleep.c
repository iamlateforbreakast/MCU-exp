/*
 * esp_deep_sleep_register_phy_hook - deliberate no-op. See header comment:
 * nothing in this RTEMS port ever enters deep sleep, so a registered hook
 * would never fire either way - just accept the registration and succeed.
 */
#include "esp_sleep.h"

esp_err_t esp_deep_sleep_register_phy_hook(esp_deep_sleep_cb_t new_dslp_cb)
{
    (void) new_dslp_cb;
    return ESP_OK;
}
