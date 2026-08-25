/*
 * Real ESP-IDF splits `nvs_flash_init`/`nvs_flash_erase` into this header
 * from the plain `nvs.h`. `phy_init.c` `#include`s this but never calls
 * anything from it directly (only mentions `nvs_flash_init` inside a log
 * message string) - see `nvs.h`'s own header comment for the full
 * rationale.
 */
#ifndef FREERTOS_COMPAT_NVS_FLASH_H
#define FREERTOS_COMPAT_NVS_FLASH_H

#include "nvs.h"

#endif /* FREERTOS_COMPAT_NVS_FLASH_H */
