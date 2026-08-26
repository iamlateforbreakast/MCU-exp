/*
 * BLE controller-only smoke test for RTEMS on the ESP32-C3 (esp32c3db BSP),
 * per upstream-bt-driver/README.md's Phase 3 plan: initialize the vendored
 * ESP-IDF BLE controller (bt.c + the closed libbtdm_app.a blob + esp_phy),
 * send an HCI Reset command over the in-process VHCI transport, and check
 * whether a valid HCI Command Complete event comes back.
 *
 * Status: UNTESTED - this repo's sandbox has no ESP32-C3 hardware, so this
 * has only been built (see ../../upstream-bt-driver/vendor/README.md and
 * ../../upstream-bt-driver/linker-section-patch/README.md for what's been
 * validated so far: every piece this app calls compiles and links against
 * real vendored/fetched ESP-IDF source, but nothing has run on a real chip
 * yet). Do not trust the "PASS"/"FAIL" framing below to mean anything until
 * it has actually executed on hardware.
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

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    printf("calling esp_bt_controller_init...\n");
    esp_err_t err = esp_bt_controller_init(&cfg);
    if (err != ESP_OK) {
        printf("FAIL: esp_bt_controller_init: %d\n", err);
        exit(1);
    }

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

#define CONFIGURE_MAXIMUM_TASKS 8
#define CONFIGURE_MAXIMUM_SEMAPHORES 8
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES 8
#define CONFIGURE_MAXIMUM_TIMERS 8

#define CONFIGURE_INIT_TASK_STACK_SIZE (8 * 1024)

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT

#include <rtems/confdefs.h>
