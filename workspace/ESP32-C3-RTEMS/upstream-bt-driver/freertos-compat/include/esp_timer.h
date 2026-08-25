/*
 * Subset of ESP-IDF's esp_timer.h needed by bt.c at IDF v5.3.1. Signatures
 * (esp_timer_create_args_t's fields, esp_timer_create/start_once/stop/delete)
 * confirmed against real IDF v5.3.1 source
 * (components/esp_timer/include/esp_timer.h) this session - see
 * ../../README.md. Backed by RTEMS's timer-server API (src/esp_timer.c),
 * not plain rtems_timer_fire_after - see README for why.
 */
#ifndef FREERTOS_COMPAT_ESP_TIMER_H
#define FREERTOS_COMPAT_ESP_TIMER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct esp_timer *esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void *arg);

typedef enum {
    ESP_TIMER_TASK,
    ESP_TIMER_MAX,
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t       callback;
    void                *arg;
    esp_timer_dispatch_t dispatch_method;
    const char           *name;
    bool                 skip_unhandled_events;
} esp_timer_create_args_t;

esp_err_t esp_timer_create(const esp_timer_create_args_t *create_args, esp_timer_handle_t *out_handle);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);

/*
 * `esp_timer_get_time` - needed by `esp_phy/src/phy_common.c` (vendored
 * 2026-08-25), real signature confirmed against IDF v5.3.1
 * `components/esp_timer/include/esp_timer.h`: microseconds elapsed since
 * boot, monotonic. Backed (src/esp_timer.c) by
 * `rtems_clock_get_uptime()`, RTEMS's own boot-relative monotonic clock -
 * not `rtems_clock_get_ticks_since_boot()` + a tick-rate multiply, since
 * `get_uptime` already returns real (not tick-quantized) time and avoids
 * assuming a fixed tick rate.
 */
int64_t esp_timer_get_time(void);

#endif /* FREERTOS_COMPAT_ESP_TIMER_H */
