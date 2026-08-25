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

#endif /* _FREERTOS_COMPAT_ESP_LOG_H_ */
