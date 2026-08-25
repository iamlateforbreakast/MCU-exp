#include "nvs.h"

esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
    (void) namespace_name;
    (void) open_mode;
    (void) out_handle;
    return ESP_ERR_NVS_NOT_INITIALIZED;
}

esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value)
{
    (void) handle;
    (void) key;
    (void) value;
    return ESP_ERR_NVS_NOT_INITIALIZED;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    (void) handle;
    (void) key;
    (void) value;
    (void) length;
    return ESP_ERR_NVS_NOT_INITIALIZED;
}

esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value)
{
    (void) handle;
    (void) key;
    (void) out_value;
    return ESP_ERR_NVS_NOT_INITIALIZED;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length)
{
    (void) handle;
    (void) key;
    (void) out_value;
    (void) length;
    return ESP_ERR_NVS_NOT_INITIALIZED;
}

esp_err_t nvs_erase_all(nvs_handle_t handle)
{
    (void) handle;
    return ESP_ERR_NVS_NOT_INITIALIZED;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void) handle;
    return ESP_ERR_NVS_NOT_INITIALIZED;
}

void nvs_close(nvs_handle_t handle)
{
    (void) handle;
}
