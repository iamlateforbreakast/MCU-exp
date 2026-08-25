/*
 * Minimal esp_err.h subset - only what esp_intr_alloc.h/esp_timer.h below
 * actually need as a return type. Not a full port of IDF's real esp_err.h
 * (which has a much larger ESP_ERR_* table); extend as more of bt.c's
 * actual error-code checks are found to matter.
 */
#ifndef FREERTOS_COMPAT_ESP_ERR_H
#define FREERTOS_COMPAT_ESP_ERR_H

typedef int esp_err_t;

#define ESP_OK   0
#define ESP_FAIL (-1)

#endif /* FREERTOS_COMPAT_ESP_ERR_H */
