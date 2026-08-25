/*
 * Minimal esp_err.h subset - `ESP_OK`/`ESP_FAIL` plus the `ESP_ERR_*`
 * values bt.c actually checks/returns (grepped, 2026-08-25, while
 * compiling bt.c for the first time). Values copied verbatim from real
 * ESP-IDF v5.3.1 `components/esp_common/include/esp_err.h` - matching
 * the real numbers matters here even though bt.c only ever compares them
 * symbolically, since a real caller further up the stack (NimBLE, an
 * application) might not. Not a full port of IDF's real esp_err.h (which
 * also declares `esp_err_to_name()` etc.); extend as more of bt.c's
 * actual error-code checks are found to matter.
 */
#ifndef FREERTOS_COMPAT_ESP_ERR_H
#define FREERTOS_COMPAT_ESP_ERR_H

typedef int esp_err_t;

#define ESP_OK   0
#define ESP_FAIL (-1)

#define ESP_ERR_NO_MEM         0x101
#define ESP_ERR_INVALID_ARG    0x102
#define ESP_ERR_INVALID_STATE  0x103
#define ESP_ERR_INVALID_SIZE   0x104
#define ESP_ERR_NOT_SUPPORTED  0x106

#endif /* FREERTOS_COMPAT_ESP_ERR_H */
