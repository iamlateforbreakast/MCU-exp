/*
 * SPDX-FileCopyrightText: 2020-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Extracted (not the whole file - see below) from real ESP-IDF v5.3.1
 * components/bootloader_support/src/esp32c3/bootloader_esp32c3.c's
 * bootloader_hardware_init()/bootloader_ana_reset_config()/
 * bootloader_super_wdt_auto_feed() (lines 84-124 of that file), real
 * chip-safety hardware bring-up real ESP-IDF's 2nd-stage bootloader runs
 * before any app code - which this RTEMS port never runs, since it uses
 * the ESP32-C3's direct-boot header instead of a 2nd-stage bootloader
 * (see ../../../../../ESP32-C3-RTEMS.md's "Flashing and monitoring"
 * section). The full real file has ~15 more `#include`s this port has no
 * use for (flash/image-format/console/MMU/cache bring-up, all handled by
 * RTEMS's own BSP already, confirmed working per dmips_benchmark/oled
 * examples) - only these 3 small, self-contained functions are extracted
 * here, verbatim, rather than the whole file.
 *
 * Found 2026-08-26 while cross-checking Zephyr's ESP32-C3 BLE port
 * (zephyrproject-rtos/hal_espressif and zephyrproject-rtos/zephyr, whose
 * `soc/espressif/esp32c3/hw_init.c` calls the equivalent of this) for
 * anything this port might be missing around the "BLE assert emi.c 164"
 * investigation - not confirmed as the fix (existing non-BLE examples run
 * fine without it), but a real, previously-unidentified gap: this port
 * skips ALL 2nd-stage-bootloader-level hardware bring-up, and these
 * specific steps (brownout/clock-glitch hardware reset detector enable,
 * super-watchdog auto-feed) are plausible candidates for something the
 * closed BLE controller blob's own internal state depends on being
 * configured, unlike generic peripherals.
 */

#include "esp_attr.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/chip_revision.h"
#include "hal/efuse_hal.h"
#include "hal/regi2c_ctrl.h"
#include "soc/regi2c_lp_bias.h"
#include "soc/regi2c_bias.h"
#include "bootloader_soc.h"

/* Real value from ESP-IDF v5.3.1's hal/esp32c3/include/hal/rwdt_ll.h:29 -
 * not re-vendoring that whole file just for this one constant. */
#ifndef RTC_CNTL_SWD_WKEY_VALUE
#define RTC_CNTL_SWD_WKEY_VALUE 0x8F1D312A
#endif

static inline void bootloader_hardware_init(void)
{
    // This check is always included in the bootloader so it can
    // print the minimum revision error message later in the boot
    if (!ESP_CHIP_REV_ABOVE(efuse_hal_chip_revision(), 3)) {
        REGI2C_WRITE_MASK(I2C_ULP, I2C_ULP_IR_FORCE_XPD_IPH, 1);
        REGI2C_WRITE_MASK(I2C_BIAS, I2C_BIAS_DREG_1P1_PVT, 12);
    }
}

static inline void bootloader_ana_reset_config(void)
{
    //Enable super WDT reset.
    bootloader_ana_super_wdt_reset_config(true);

    /*
      For origin chip & ECO1: brownout & clock glitch reset not available
      For ECO2: fix brownout reset bug
      For ECO3: fix clock glitch reset bug
    */
    switch (efuse_hal_chip_revision()) {
        case 0:
        case 1:
            //Disable BOD and GLITCH reset
            bootloader_ana_bod_reset_config(false);
            bootloader_ana_clock_glitch_reset_config(false);
            break;
        case 2:
            //Enable BOD reset. Disable GLITCH reset
            bootloader_ana_bod_reset_config(true);
            bootloader_ana_clock_glitch_reset_config(false);
            break;
        case 3:
        default:
            //Enable BOD, and GLITCH reset
            bootloader_ana_bod_reset_config(true);
            bootloader_ana_clock_glitch_reset_config(true);
            break;
    }
}

static void bootloader_super_wdt_auto_feed(void)
{
    REG_WRITE(RTC_CNTL_SWD_WPROTECT_REG, RTC_CNTL_SWD_WKEY_VALUE);
    REG_SET_BIT(RTC_CNTL_SWD_CONF_REG, RTC_CNTL_SWD_AUTO_FEED_EN);
    REG_WRITE(RTC_CNTL_SWD_WPROTECT_REG, 0);
}

/* Real ESP-IDF calls these 3 as part of bootloader_init() (real
 * bootloader_esp32c3.c:126-133) before any app code. This port has no
 * bootloader_init() (direct-boot, no 2nd stage bootloader) - call this
 * once, early, from the app's own Init task instead. */
void esp32c3_bootloader_hw_bringup(void)
{
    bootloader_hardware_init();
    bootloader_ana_reset_config();
    bootloader_super_wdt_auto_feed();
}
