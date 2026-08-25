/*
 * Real ESP-IDF splits `heap_caps_add_region` into this header from the
 * plain `esp_heap_caps.h` - collapsed into one file here since this shim
 * declares both together. See esp_heap_caps.h for the real rationale.
 */
#ifndef _FREERTOS_COMPAT_ESP_HEAP_CAPS_INIT_H_
#define _FREERTOS_COMPAT_ESP_HEAP_CAPS_INIT_H_

#include "esp_heap_caps.h"

#endif /* _FREERTOS_COMPAT_ESP_HEAP_CAPS_INIT_H_ */
