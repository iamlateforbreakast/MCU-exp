/*
 * BLE controller-only smoke test for RTEMS on the ESP32-C3 (esp32c3db BSP),
 * per upstream-bt-driver/README.md's Phase 3 plan: initialize the vendored
 * ESP-IDF BLE controller (bt.c + the closed libbtdm_app.a blob + esp_phy),
 * send an HCI Reset command over the in-process VHCI transport, and check
 * whether a valid HCI Command Complete event comes back.
 *
 * Status (2026-08-26): links and boots on real ESP32-C3 hardware (QFN32
 * rev v0.4) - both closed blobs execute real code (their own internal
 * version-banner log lines print over serial). The register_chipv7_phy()
 * RF-calibration hang (root-caused to missing rtc_clk_init() clock-tree
 * bring-up) is fixed and confirmed on hardware. Now blocked on a
 * different real assert from the closed blob's own code ("BLE assert
 * emi.c 164") past that point - a missing 2nd-stage-bootloader-level
 * hardware bring-up step (see esp32c3_bootloader_hw_bringup() below) is
 * the current candidate fix, not yet re-verified on hardware as of this
 * comment - see ../../upstream-bt-driver/vendor/README.md for the full
 * writeup. Do not trust the "PASS"/"FAIL" framing below to mean anything
 * until it has actually completed a real run on hardware.
 *
 * HCI Reset command bytes (Bluetooth Core Spec, Vol 4 Part E, section 7.3.2 -
 * a fixed, standard command, not something this port invents): packet type
 * 0x01 (HCI Command), opcode 0x0C03 (OGF 0x03 "Host Controller & Baseband",
 * OCF 0x0003 "Reset") little-endian as 0x03 0x0C, parameter length 0x00.
 */

#include <rtems.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "soc/rtc.h"
#include "private/esp_coexist_internal.h"
#include "esp_private/periph_ctrl.h"

/* upstream-bt-driver/vendor/components/bootloader_support/src/esp32c3/
 * bootloader_hw_init.c - no public header, this is the only caller. */
extern void esp32c3_bootloader_hw_bringup(void);

static rtems_id s_response_sem;
static uint8_t s_response[16];
static uint16_t s_response_len;

static void notify_host_send_available(void)
{
    /* No-op: this app only ever sends one packet, never blocked on
     * send-buffer availability. */
}

static int notify_host_recv(uint8_t *data, uint16_t len)
{
    if (len > sizeof(s_response)) {
        len = sizeof(s_response);
    }
    memcpy(s_response, data, len);
    s_response_len = len;
    rtems_semaphore_release(s_response_sem);
    return 0;
}

static const esp_vhci_host_callback_t vhci_callback = {
    .notify_host_send_available = notify_host_send_available,
    .notify_host_recv = notify_host_recv,
};

rtems_task Init(rtems_task_argument ignored)
{
    (void) ignored;

    printf("\nBLE controller-only smoke test (untested on real hardware)\n");

    /* DIAG (2026-08-26): temporary JTAG-attach grace period - JTAG's own
     * "reset halt" only resets the CPU core, not full chip/peripheral
     * state, so replaying the crash that way lands somewhere different
     * from a real power-on boot. This gives a wide window to attach and
     * plain-`halt` (not reset) a genuine power-on boot before it reaches
     * the crash, without needing split-second timing. */
    printf("DIAG: sleeping 40s for JTAG attach...\n");
    {
        rtems_interval per_second = rtems_clock_get_ticks_per_second();
        rtems_task_wake_after(per_second * 40);
    }
    printf("DIAG: done sleeping, continuing\n");

    /* Real ESP-IDF's 2nd-stage bootloader (bootloader_init(), real
     * bootloader_esp32c3.c) runs chip-safety hardware bring-up before any
     * app code - brownout/clock-glitch hardware reset detector enable,
     * super-watchdog auto-feed. This port has no 2nd-stage bootloader
     * (direct-boot header instead) and never ran any of this - found
     * 2026-08-26 by cross-checking Zephyr's ESP32-C3 BLE port for
     * anything missed while investigating a "BLE assert emi.c 164"
     * closed-blob assert. Not confirmed as the fix for that (existing
     * non-BLE examples run fine without it), but a real, previously-
     * unidentified gap, cheap to close - see
     * upstream-bt-driver/vendor/components/bootloader_support/src/
     * esp32c3/bootloader_hw_init.c for the extracted real source. Called
     * first, matching real IDF's own ordering (ana-reset/WDT bring-up
     * before clock-tree bring-up). */
    esp32c3_bootloader_hw_bringup();

    /* Real ESP-IDF brings up the XTAL/BBPLL clock tree via rtc_clk_init()
     * very early in its own startup (bootloader_clock_init()/
     * esp_startup.c - neither vendored here, since RTEMS's own startup
     * replaces ESP-IDF's), before any app code runs. This RTEMS port
     * never called it at all until now - found 2026-08-26 while tracking
     * down a real-hardware hang inside the closed PHY blob's
     * register_chipv7_phy() RF-calibration call, which calls back into
     * this port's rtc_clk.c and into a ROM BBPLL-calibration routine
     * that likely polls a PLL-lock bit that can never assert if the
     * BBPLL was never digitally configured - see
     * ../../upstream-bt-driver/vendor/README.md for the full writeup.
     * Values match this board's confirmed real configuration (QFN32 rev
     * v0.4, 40MHz XTAL per esptool's own detection, 160MHz CPU per the
     * dmips_benchmark milestone) rather than RTC_CLK_CONFIG_DEFAULT()'s
     * literal 80MHz default, to avoid changing the CPU clock speed RTEMS
     * itself already brought the board up at. */
    rtc_clk_config_t clk_cfg = {
        .xtal_freq = SOC_XTAL_FREQ_40M,
        .cpu_freq_mhz = 160,
        .fast_clk_src = SOC_RTC_FAST_CLK_SRC_RC_FAST,
        .slow_clk_src = SOC_RTC_SLOW_CLK_SRC_RC_SLOW,
        .clk_rtc_clk_div = 0,
        .clk_8m_clk_div = 0,
        .slow_clk_dcap = RTC_CNTL_SCK_DCAP_DEFAULT,
        .clk_8m_dfreq = RTC_CNTL_CK8M_DFREQ_DEFAULT,
    };
    printf("calling rtc_clk_init (XTAL/BBPLL bring-up)...\n");
    rtc_clk_init(clk_cfg);

    rtems_status_code sc = rtems_semaphore_create(
        rtems_build_name('v', 'h', 'c', 'i'),
        0,
        RTEMS_COUNTING_SEMAPHORE,
        0,
        &s_response_sem
    );
    if (sc != RTEMS_SUCCESSFUL) {
        printf("FAIL: rtems_semaphore_create: %d\n", sc);
        exit(1);
    }

    /* Real ESP-IDF calls this from init_coexist() (components/esp_system/
     * startup_funcs.c), a SECONDARY-stage system-init function that runs
     * before app_main() - i.e. before any app code, including this one's
     * own controller_init/enable calls, ever touches BT. This RTEMS port
     * never linked esp_coex at all until now - found 2026-08-26 while
     * investigating "BLE assert emi.c 164/331": bt.c itself calls
     * coex_init()/coex_enable() directly (guarded by
     * CONFIG_SW_COEXIST_ENABLE, previously assumed off - see
     * sdkconfig-compat.h's CORRECTION note), and coex_enable() runs
     * immediately before the exact call (btdm_controller_enable()) that
     * leads to the crash. Not confirmed as the fix, but a real,
     * previously-entirely-missing component, not a guess - see
     * upstream-bt-driver/vendor/components/esp_coex/. */
    extern esp_err_t coex_pre_init(void);
    printf("calling esp_coex_adapter_register + coex_pre_init...\n");
    esp_coex_adapter_register(&g_coex_adapter_funcs);
    coex_pre_init();

    /* THE FIX for "BLE assert emi.c" (2026-08-26): live register dumps
     * proved the closed blob's EM base registers (0x60031204-0x60031218)
     * permanently read 0x00000000 even immediately after their store
     * instructions definitely execute (confirmed via return-address
     * tracing into r_emi_em_base_init) - not "written then cleared",
     * the writes had no effect at all. Root cause: the hardware block
     * owning these registers needs clocks this port never enabled.
     * `periph_module_enable(PERIPH_BT_MODULE)` (already called inside
     * bt.c's own esp_bt_controller_init()) has a real but zero-width
     * clock-enable mask (SYSTEM_WIFI_CLK_BT_EN_M) - it does nothing.
     * The three calls below - `wifi_bt_common_module_enable()`
     * (SYSTEM_WIFI_CLK_WIFI_BT_COMMON_M, otherwise only reached via
     * esp_phy_enable() much later, inside esp_bt_controller_enable() -
     * too late for these init-time writes) and
     * `periph_module_enable(PERIPH_BT_BASEBAND_MODULE/PERIPH_BT_LC_MODULE)`
     * (SYSTEM_BT_BASEBAND_EN/SYSTEM_BT_LC_EN, never called by bt.c or
     * phy_init.c at all) - are real, nonzero clock gates real IDF's
     * 2nd-stage bootloader most likely leaves already enabled from an
     * earlier boot stage, which this port's direct-boot header skips
     * entirely. Confirmed on real hardware: with all three called here,
     * the EM registers finally hold real, structured values
     * (0x00027e10/0x04027e64/0x0c027f74/0x10028010/0x14028044) that
     * persist through esp_bt_controller_enable(), and the assert is
     * gone - execution reaches a real HCI Reset send over VHCI. Not yet
     * isolated which of the three is strictly necessary vs. redundant -
     * kept all three since together they're the confirmed, working fix. */
    extern void wifi_bt_common_module_enable(void);
    wifi_bt_common_module_enable();
    periph_module_enable(PERIPH_BT_BASEBAND_MODULE);
    periph_module_enable(PERIPH_BT_LC_MODULE);

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    printf("calling esp_bt_controller_init...\n");
    esp_err_t err = esp_bt_controller_init(&cfg);
    if (err != ESP_OK) {
        printf("FAIL: esp_bt_controller_init: %d\n", err);
        exit(1);
    }

    extern size_t malloc_free_space(void);
    printf("DIAG: malloc_free_space() before enable = %u\n", (unsigned) malloc_free_space());
    printf("calling esp_bt_controller_enable(ESP_BT_MODE_BLE)...\n");
    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
        printf("FAIL: esp_bt_controller_enable: %d\n", err);
        exit(1);
    }

    printf("registering VHCI callback...\n");
    err = esp_vhci_host_register_callback(&vhci_callback);
    if (err != ESP_OK) {
        printf("FAIL: esp_vhci_host_register_callback: %d\n", err);
        exit(1);
    }

    static uint8_t hci_reset_cmd[] = { 0x01, 0x03, 0x0C, 0x00 };
    printf("sending HCI Reset over VHCI...\n");
    esp_vhci_host_send_packet(hci_reset_cmd, sizeof(hci_reset_cmd));

    rtems_interval per_second = rtems_clock_get_ticks_per_second();
    sc = rtems_semaphore_obtain(s_response_sem, RTEMS_WAIT, per_second * 2);
    if (sc != RTEMS_SUCCESSFUL) {
        printf("FAIL: no VHCI response within timeout (sc=%d)\n", sc);
        exit(1);
    }

    printf("received %u bytes:", s_response_len);
    for (uint16_t i = 0; i < s_response_len; i++) {
        printf(" %02x", s_response[i]);
    }
    printf("\n");

    /* HCI Command Complete event: packet type 0x04, event code 0x0E,
     * followed by param length, num_hci_command_packets, opcode (LE),
     * status. A correct Reset response has status byte 0x00 at the end. */
    if (s_response_len >= 7 && s_response[0] == 0x04 && s_response[1] == 0x0E
        && s_response[4] == 0x03 && s_response[5] == 0x0C
        && s_response[6] == 0x00) {
        printf("PASS: valid HCI Command Complete for Reset, status 0x00\n");
    } else {
        printf("FAIL: unexpected response bytes\n");
    }

    exit(0);
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

/* Bumped 2026-08-26 after adding esp_coex (coex_pre_init()): with
 * CONFIG_SW_COEXIST_ENABLE on, bt.c/libcoexist.a create more RTEMS
 * objects (semaphores, timers) than this port originally budgeted for
 * - a real `xSemaphoreCreateCounting` -> NULL -> `assert(semphr->handle)`
 * failure in bt.c's semphr_create_wrapper() at real, live count 8
 * confirmed this was a hard limit, not a logic bug. Sized generously
 * rather than tuned precisely - no tight memory constraint here. */
#define CONFIGURE_MAXIMUM_TASKS 16
#define CONFIGURE_MAXIMUM_SEMAPHORES 32
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES 16
#define CONFIGURE_MAXIMUM_TIMERS 16

#define CONFIGURE_INIT_TASK_STACK_SIZE (8 * 1024)

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT

#include <rtems/confdefs.h>
