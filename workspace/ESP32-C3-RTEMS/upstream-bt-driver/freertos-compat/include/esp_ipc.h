/*
 * Deliberately empty. `bt.c` `#include`s this unconditionally, but its
 * only call (`esp_ipc_call_blocking`, `bt.c:506`) is inside the `#else`
 * branch of `#if CONFIG_FREERTOS_UNICORE` - and `sdkconfig-compat.h`
 * defines `CONFIG_FREERTOS_UNICORE=1` for this single-core ESP32-C3
 * profile (confirmed forced via the root Kconfig's `select
 * FREERTOS_UNICORE` for `IDF_TARGET_ESP32C3`), so that branch, and this
 * header's only needed symbol, is never compiled.
 */
#ifndef _FREERTOS_COMPAT_ESP_IPC_H_
#define _FREERTOS_COMPAT_ESP_IPC_H_
#endif /* _FREERTOS_COMPAT_ESP_IPC_H_ */
