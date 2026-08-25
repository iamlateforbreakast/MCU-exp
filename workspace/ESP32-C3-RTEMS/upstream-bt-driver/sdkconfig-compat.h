/*
 * Stand-in for ESP-IDF's Kconfig-generated `sdkconfig.h` - the ~43 (not
 * ~45 as first estimated; see below) `CONFIG_*` macros
 * `components/bt/controller/esp32c3/bt.c` and
 * `components/bt/include/esp32c3/include/esp_bt.h` reference, that a real
 * ESP-IDF build gets for free from `menuconfig` and this RTEMS port has no
 * Kconfig system to generate. This is the blocker Phase 2's recon flagged
 * as new, unattempted scope - see `../README.md`'s "New blocker found"
 * paragraph.
 *
 * Confirmed 2026-08-25 by sparse-cloning `github.com/espressif/esp-idf` at
 * the real tag **v5.3.1** (same tag this repo's Phase 0 recon already
 * used) with real internet access, and grepping/reading the actual
 * Kconfig files, not guessed. Every value below cites the exact file that
 * produced it. Target profile modeled: **ESP32-C3, NimBLE host, BLE-only,
 * no Wi-Fi/BT coexistence, no power management, no HCI-UART, default chip
 * revision** - the same minimal profile `../README.md`'s Phase 2 section
 * already proposed. A different profile (Bluedroid host, coexistence
 * enabled, etc.) would need different values re-derived the same way, not
 * assumed from this file.
 *
 * **Two macros dropped entirely, not just left undefined**, because they
 * are not real Kconfig options anywhere in v5.3.1's full source tree
 * (grepped `config MAC_BB_PD` / `config SW_COEXIST_ENABLE` /
 * `config BT_CTRL_HW_CCA` across every `*Kconfig*` file, zero hits for
 * all three) - a real stock ESP-IDF v5.3.1 build for esp32c3 never
 * defines them either, so the `#if CONFIG_MAC_BB_PD` /
 * `#if CONFIG_SW_COEXIST_ENABLE` blocks they guard in `bt.c` are
 * permanently dead code in this IDF version (undefined macros evaluate to
 * 0 in `#if`), not something this shim is missing. Likely leftover
 * `#ifdef` guards from an earlier chip generation (classic ESP32 had
 * `MAC_BB_PD`) never cleaned out of the shared `bt.c` source.
 * `CONFIG_BTDM_CONTROLLER_MODEM_SLEEP` (part of the original ~45 count)
 * was also dropped: it only appears inside a comment in `esp_bt.h`
 * (line 553), never in a real `#if`/`#ifdef` - a false positive from the
 * original grep-based macro count, not a real requirement.
 *
 * Deliberate build-choice overrides, not discovered defaults - flagged
 * explicitly so nobody mistakes them for what a stock build would do:
 * - `CONFIG_ESP_COEX_ENABLED`: a real esp32c3 build defaults this to
 *   **y** (`components/esp_coex/Kconfig:6`, `default y if
 *   (!SOC_WIRELESS_HOST_SUPPORTED)`, and esp32c3 has no
 *   `SOC_WIRELESS_HOST_SUPPORTED` define). Left undefined here on purpose
 *   to avoid pulling in `esp_coex`'s `private/esp_coexist_internal.h`
 *   (`bt.c:40`) for a minimal smoke test with no Wi-Fi component to
 *   coexist with - real scope-creep avoidance, not a recon miss.
 */
#ifndef _FREERTOS_COMPAT_SDKCONFIG_COMPAT_H_
#define _FREERTOS_COMPAT_SDKCONFIG_COMPAT_H_

/* --- Target/arch identity ---
 * Kconfig (root): `Kconfig:93-96` (IDF_TARGET_ESP32C3, `select
 * FREERTOS_UNICORE`) / `Kconfig:88-90` (IDF_TARGET_ESP32S3). */
#define CONFIG_IDF_TARGET_ESP32C3 1
/* CONFIG_IDF_TARGET_ESP32S3 intentionally undefined - not our target. */

/*
 * Found needed 2026-08-26 vendoring efuse/src/esp_efuse_fields.c:
 * `esp_fault.h`'s `_ESP_FAULT_ILLEGAL_INSTRUCTION` macro branches on this
 * to pick RISC-V `unimp` vs Xtensa `ill.n` inline asm - without it,
 * Xtensa asm gets fed to the RISC-V assembler (real compile failure, not
 * a shim gap: the file itself has the correct branch, just needed the
 * macro this repo hadn't defined yet). Root `Kconfig:64-65`:
 * `IDF_TARGET_ESP32C3` `select`s `IDF_TARGET_ARCH_RISCV` (real, esp32c3
 * is RISC-V, not Xtensa). */
#define CONFIG_IDF_TARGET_ARCH_RISCV 1

/* --- FreeRTOS core count ---
 * `components/freertos/Kconfig:23` (FREERTOS_UNICORE has no default of
 * its own for esp32c3, but root `Kconfig:96` `select`s it unconditionally
 * for IDF_TARGET_ESP32C3, overriding the visible default) and
 * `components/freertos/Kconfig:596-602` (NUMBER_OF_CORES = 1 if
 * UNICORE). */
#define CONFIG_FREERTOS_UNICORE 1
#define CONFIG_FREERTOS_NUMBER_OF_CORES 1
/* CONFIG_FREERTOS_USE_TICKLESS_IDLE intentionally undefined -
 * `components/freertos/Kconfig:296-299`, default n, and depends on
 * PM_ENABLE which is also off below. */

/* --- Power management ---
 * `components/esp_pm/Kconfig:2-6`, default n (SMP FreeRTOS + most
 * targets). */
/* CONFIG_PM_ENABLE intentionally undefined. */

/* --- PHY ---
 * `components/esp_phy/Kconfig:3-5`, default y if SOC_PHY_SUPPORTED
 * (confirmed 1 for esp32c3, `components/soc/esp32c3/include/soc/
 * soc_caps.h:32`). */
#define CONFIG_ESP_PHY_ENABLED 1

/* --- Chip revision gate ---
 * `components/esp_hw_support/port/esp32c3/Kconfig.hw_support:1-24`,
 * `choice ESP32C3_REV_MIN` defaults to `ESP32C3_REV_MIN_3` (rev v0.3), so
 * the separate `ESP32C3_REV_MIN_101` (rev v1.1) option this shim's real
 * hardware doesn't need is off either way (real board is rev v0.4 per
 * [[esp32c3-rtems-bsp-status]] dmips_benchmark run - still below REV_MIN_101,
 * so this holds for the actual board too, not just the Kconfig default). */
/* CONFIG_ESP32C3_REV_MIN_101 intentionally undefined. */

/* --- Bluetooth: host selection ---
 * `components/bt/Kconfig:1-35`: BT_ENABLED gates the whole subsystem;
 * `choice BT_HOST` defaults to BT_BLUEDROID_ENABLED but NimBLE is the
 * deliberate choice here per `../README.md`'s architecture (BLE-only,
 * "recommended for BLE only usecases" per the choice's own help text). */
#define CONFIG_BT_ENABLED 1
#define CONFIG_BT_NIMBLE_ENABLED 1
/* CONFIG_BT_BLUEDROID_ENABLED intentionally undefined - NimBLE chosen instead. */

/* --- BLE 5.0 feature gate ---
 * `esp_bt.h:170-183`'s `#if defined(CONFIG_BT_BLE_50_FEATURES_SUPPORTED)
 * || defined(CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT)` is an OR - only the
 * NimBLE side needs to be true. `CONFIG_BT_BLE_50_FEATURES_SUPPORTED`
 * itself is Bluedroid-only (`components/bt/host/bluedroid/
 * Kconfig.in:1184`, `depends on BT_BLUEDROID_ENABLED`) and never
 * referenced by `bt.c` directly - left undefined, satisfied via the
 * NimBLE macro below instead.
 * `components/bt/host/nimble/Kconfig.in:547-552`: default y, `depends on
 * BT_NIMBLE_ENABLED && (SOC_BLE_50_SUPPORTED || !BT_CONTROLLER_ENABLED)` -
 * SOC_BLE_50_SUPPORTED confirmed 1 for esp32c3 (`soc_caps.h:465`). */
#define CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT 1
/* CONFIG_BT_BLE_50_FEATURES_SUPPORTED intentionally undefined (Bluedroid-only). */

/* --- Controller: mode/HCI transport ---
 * `components/bt/controller/esp32c3/Kconfig.in:1-3` (MODE_EFF,
 * unconditional `default 1` - BLE-only mode, matches esp32c3 having no
 * classic BT) and `:66-72` (HCI_TL_EFF; the `default 1 if
 * BT_CTRL_HCI_M0DE_VHCI` line is a real upstream typo - "M0DE" with a
 * zero - so it never matches, but the unconditional `default 1` right
 * after it is what actually applies, landing on the same VHCI value the
 * option was clearly meant to select). */
#define CONFIG_BT_CTRL_MODE_EFF 1
#define CONFIG_BT_CTRL_HCI_TL_EFF 1

/* --- Controller: activities/buffers ---
 * `Kconfig.in:5-23` (BLE_MAX_ACT default 6, feeding MAX_ACT_EFF) and
 * `:20-23` (STATIC_ACL_TX_BUF_NB default 0). */
#define CONFIG_BT_CTRL_BLE_MAX_ACT_EFF 6
#define CONFIG_BT_CTRL_BLE_STATIC_ACL_TX_BUF_NB 0

/* --- Controller: core pinning ---
 * `Kconfig.in:29-47`: the pin-to-core choice `depends on
 * !FREERTOS_UNICORE`, invisible here since FREERTOS_UNICORE=1 above, so
 * PINNED_TO_CORE falls through to its unconditional `default 0`. */
#define CONFIG_BT_CTRL_PINNED_TO_CORE 0

/* --- Controller: scan/adv filter tuning (all explicit Kconfig defaults) ---
 * `Kconfig.in:74-79` (ADV_DUP_FILT_MAX), `:266-303` (SCAN_DUPL_TYPE,
 * default "by device address"), `:305-312` (SCAN_DUPL_CACHE_SIZE),
 * `:314-328` (DUPL_SCAN_CACHE_REFRESH_PERIOD). */
#define CONFIG_BT_CTRL_ADV_DUP_FILT_MAX 30
#define CONFIG_BT_CTRL_SCAN_DUPL_TYPE 0
#define CONFIG_BT_CTRL_SCAN_DUPL_CACHE_SIZE 100
#define CONFIG_BT_CTRL_DUPL_SCAN_CACHE_REFRESH_PERIOD 0
/* CONFIG_BT_CTRL_BLE_MESH_SCAN_DUPL_EN intentionally undefined -
 * Kconfig.in:330-335, default n. */
/* CONFIG_BT_CTRL_MESH_DUPL_SCAN_CACHE_SIZE intentionally undefined -
 * Kconfig.in:337-344, depends on the (off) option above. */

/* --- Controller: CCA / antenna / Tx power (choices with no explicit
 * `default` line resolve to the first-listed option - standard Kconfig
 * behavior for choices, not independently runtime-confirmed via
 * menuconfig this session) ---
 * `Kconfig.in:81-114` (BLE_CCA_MODE explicit default NONE=0, HW_CCA_VAL
 * explicit default 20; HW_CCA_EFF's `if BT_CTRL_HW_CCA` branch is dead -
 * that parent symbol doesn't exist as a config anywhere in v5.3.1 either,
 * same pattern as MAC_BB_PD/SW_COEXIST_ENABLE above - so it always falls
 * through to `default 0`), `:116-133` (CE_LENGTH_TYPE, no explicit
 * default -> first entry ORIG=0), `:135-165` (TX/RX_ANTENNA_INDEX, no
 * explicit default -> first entry, index 0 both), `:167-225`
 * (DFT_TX_POWER_LEVEL, explicit default P9 -> value 11). */
#define CONFIG_BT_BLE_CCA_MODE 0
#define CONFIG_BT_CTRL_HW_CCA_VAL 20
#define CONFIG_BT_CTRL_HW_CCA_EFF 0
#define CONFIG_BT_CTRL_CE_LENGTH_TYPE_EFF 0
#define CONFIG_BT_CTRL_TX_ANTENNA_INDEX_EFF 0
#define CONFIG_BT_CTRL_RX_ANTENNA_INDEX_EFF 0
#define CONFIG_BT_CTRL_DFT_TX_POWER_LEVEL_EFF 11

/* --- Controller: coexistence-dependent tuning ---
 * `Kconfig.in:346-369`: COEX_PHY_CODED_TX_RX_TLIM_EFF's `default 0 if
 * !ESP_COEX_SW_COEXIST_ENABLE` applies since coexistence is off in this
 * minimal profile (see CONFIG_ESP_COEX_ENABLED note above). */
#define CONFIG_BT_CTRL_COEX_PHY_CODED_TX_RX_TLIM_EFF 0
/* CONFIG_ESP_COEX_ENABLED intentionally undefined - see file header;
 * real esp32c3 default is y, overridden here to avoid vendoring esp_coex. */
/* CONFIG_SW_COEXIST_ENABLE intentionally omitted - not a real Kconfig
 * symbol in v5.3.1 at all; see file header. */

/* --- Controller: AGC / channel assessment / LE ping / misc bools ---
 * `Kconfig.in:450-491`, all explicit Kconfig defaults. */
/* CONFIG_BT_CTRL_AGC_RECORRECT_EN intentionally undefined - default n. */
/* CONFIG_BT_CTRL_CODED_AGC_RECORRECT_EN intentionally undefined - default n. */
/* CONFIG_BT_CTRL_SCAN_BACKOFF_UPPERLIMITMAX intentionally undefined - default n. */
/* CONFIG_BT_BLE_ADV_DATA_LENGTH_ZERO_AUX intentionally undefined - default n. */
#define CONFIG_BT_CTRL_CHAN_ASS_EN 1
#define CONFIG_BT_CTRL_LE_PING_EN 1

/* --- Controller: modem sleep ---
 * `Kconfig.in:374-442`: BT_CTRL_MODEM_SLEEP itself defaults n, so
 * SLEEP_MODE_EFF and SLEEP_CLOCK_EFF both fall through to their
 * unconditional `default 0`; MAIN_XTAL_PU_DURING_LIGHT_SLEEP defaults n
 * directly and also depends on the (off) low-power-clock choice. */
#define CONFIG_BT_CTRL_SLEEP_MODE_EFF 0
#define CONFIG_BT_CTRL_SLEEP_CLOCK_EFF 0
/* CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP intentionally undefined. */

/* CONFIG_MAC_BB_PD intentionally omitted entirely - not a real Kconfig
 * symbol in v5.3.1; see file header. */

/* --- PHY: default TX power ---
 * Found needed 2026-08-25 when vendoring esp_phy/esp32c3/phy_init_data.c
 * (not part of bt.c's own ~43-macro surface this file originally
 * targeted - a new file pulled in a new Kconfig dependency, same pattern
 * as every other addition in this file). `components/esp_phy/Kconfig`:
 * `ESP_PHY_MAX_TX_POWER` defaults to `ESP_PHY_MAX_WIFI_TX_POWER`, which
 * itself defaults to 20 (dBm). */
#define CONFIG_ESP_PHY_MAX_TX_POWER 20

#endif /* _FREERTOS_COMPAT_SDKCONFIG_COMPAT_H_ */
