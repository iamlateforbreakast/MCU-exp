/*
 * `ESP_LOGE/W/I/D` - real ESP-IDF backs these with a configurable,
 * level-filtered vprintf-based logging subsystem
 * (`components/log/include/esp_log.h`, not vendored here). This shim
 * just maps them straight to `printf` over the BSP's console UART - no
 * runtime level filtering, no tag-based control, just enough for `bt.c`'s
 * own diagnostic messages to reach the console during hardware bring-up.
 */
#ifndef _FREERTOS_COMPAT_ESP_LOG_H_
#define _FREERTOS_COMPAT_ESP_LOG_H_

#include <stdio.h>

#define ESP_LOGE(tag, fmt, ...) printf("E (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("W (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("I (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) printf("D (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) printf("V (%s) " fmt "\n", tag, ##__VA_ARGS__)

/*
 * `ESP_EARLY_LOG*` - real IDF's pre-scheduler-startup log variants (write
 * straight to the console UART instead of going through the full logging
 * subsystem's buffering). No such distinction needed here - both this
 * shim's normal and "early" macros are already a direct `printf`.
 */
#define ESP_EARLY_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define ESP_EARLY_LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define ESP_EARLY_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define ESP_EARLY_LOGD(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)

#endif /* _FREERTOS_COMPAT_ESP_LOG_H_ */
