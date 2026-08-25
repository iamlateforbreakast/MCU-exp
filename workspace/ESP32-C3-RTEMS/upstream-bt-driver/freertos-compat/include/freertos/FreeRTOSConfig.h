/*
 * Real ESP-IDF generates this from Kconfig
 * (`CONFIG_FREERTOS_MAX_PRIORITIES`, etc.) - `esp_task.h`'s
 * `ESP_TASK_PRIO_MAX` is the only thing this shim needs it for
 * (`#define ESP_TASK_PRIO_MAX (configMAX_PRIORITIES)`). Reuses this
 * shim's own `FREERTOS_COMPAT_MAX_PRIORITIES` (`../FreeRTOS.h`) instead
 * of a second, possibly-inconsistent value.
 */
#ifndef _FREERTOS_COMPAT_FREERTOSCONFIG_H_
#define _FREERTOS_COMPAT_FREERTOSCONFIG_H_

#include "freertos/FreeRTOS.h"

#define configMAX_PRIORITIES FREERTOS_COMPAT_MAX_PRIORITIES

#endif /* _FREERTOS_COMPAT_FREERTOSCONFIG_H_ */
