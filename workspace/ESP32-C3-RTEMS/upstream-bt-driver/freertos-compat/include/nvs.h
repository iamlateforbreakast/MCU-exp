/*
 * Stub for ESP-IDF's real NVS (non-volatile storage) API. Real
 * `esp_phy/src/phy_init.c` `#include`s this unconditionally and defines
 * (not just declares) `esp_phy_load_cal_data_from_nvs`/
 * `esp_phy_store_cal_data_to_nvs`/`esp_phy_erase_cal_data_in_nvs`, which
 * call real `nvs_open`/`nvs_get_blob`/etc. by name - those three
 * functions are never actually CALLED anywhere in this build (real IDF
 * gates every call site behind `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE`,
 * which `../sdkconfig-compat.h` deliberately leaves undefined - full RF
 * calibration every boot, no NVS/partition dependency, see that file's
 * own header comment), but they still need to *compile*, so real
 * signatures are declared here.
 *
 * Real values (`nvs_handle_t`, `NVS_READONLY`/`_READWRITE`,
 * `ESP_ERR_NVS_NOT_INITIALIZED`, all function signatures) confirmed
 * against ESP-IDF v5.3.1 `components/nvs_flash/include/nvs.h`. Backed by
 * `src/nvs_stub.c`: every function always returns
 * `ESP_ERR_NVS_NOT_INITIALIZED` (or is a no-op) - exactly the error
 * `esp_phy_load_cal_data_from_nvs`'s own real code already explicitly
 * handles by logging a friendly message and returning early
 * (`phy_init.c`), so this isn't inventing new error-handling, just
 * ensuring the path real IDF already wrote for "NVS isn't available" is
 * what actually executes if any of this ever were called.
 */
#ifndef FREERTOS_COMPAT_NVS_H
#define FREERTOS_COMPAT_NVS_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef uint32_t nvs_handle_t;

typedef enum {
    NVS_READONLY,
    NVS_READWRITE
} nvs_open_mode_t;

#define ESP_ERR_NVS_BASE             0x1100
#define ESP_ERR_NVS_NOT_INITIALIZED  (ESP_ERR_NVS_BASE + 0x01)
#define ESP_ERR_NVS_NOT_FOUND        (ESP_ERR_NVS_BASE + 0x02)

esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length);
esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length);
esp_err_t nvs_erase_all(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);

#endif /* FREERTOS_COMPAT_NVS_H */
