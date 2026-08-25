/*
 * Deliberately empty. `bt.c` `#include`s this unconditionally, but every
 * symbol it actually uses from it (`esp_pm_lock_handle_t`,
 * `esp_pm_lock_create/acquire/release/delete`) is inside
 * `#ifdef CONFIG_PM_ENABLED` blocks (confirmed by grep) - and
 * `sdkconfig-compat.h` deliberately leaves `CONFIG_PM_ENABLE` undefined
 * for this minimal no-power-management profile, so none of it is ever
 * compiled. If a future profile turns PM_ENABLE on, this header needs
 * real declarations and freertos-compat needs a real esp_pm_lock_*
 * implementation - neither exists yet.
 */
#ifndef _FREERTOS_COMPAT_ESP_PM_H_
#define _FREERTOS_COMPAT_ESP_PM_H_
#endif /* _FREERTOS_COMPAT_ESP_PM_H_ */
