/*
 * Control experiment for the ESP32-C3-RTEMS BLE port's "BLE assert
 * emi.c" investigation (see ../../ESP32-C3-RTEMS/upstream-bt-driver/
 * vendor/README.md and the esp32c3_rtems_ble_driver_status memory).
 *
 * This is a real ESP-IDF v5.3.1 project (this container's IDF checkout,
 * cloned --recursive, so its libbtdm_app.a submodule pin is
 * bfdfe8f851c99ced8316b133b0b15521917ea049 - the exact same blob commit
 * the RTEMS port's first, self-consistent v5.3.1 pairing used) doing
 * the same minimal controller-only sequence as
 * ../../ESP32-C3-RTEMS/examples/ble_vhci_smoke/init.c: init, enable
 * BLE, register a VHCI callback, send one HCI Reset, wait for the
 * response. No NimBLE/Bluedroid host, no app-level BLE logic - as close
 * to an apples-to-apples comparison against the RTEMS port as this
 * framework allows, to test whether the "BLE assert emi.c ..." the
 * RTEMS port hits with this same blob is real-ESP-IDF-reproducible
 * (implicating the blob/hardware) or RTEMS-port-specific (implicating
 * something in that port's platform layer).
 */
#ifndef SEND_HCI_RESET
#define SEND_HCI_RESET 1
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_bt.h"

static SemaphoreHandle_t s_response_sem;
static uint8_t s_response[16];
static uint16_t s_response_len;

static void notify_host_send_available(void)
{
}

static int notify_host_recv(uint8_t *data, uint16_t len)
{
    if (len > sizeof(s_response)) {
        len = sizeof(s_response);
    }
    memcpy(s_response, data, len);
    s_response_len = len;
    xSemaphoreGive(s_response_sem);
    return 0;
}

static const esp_vhci_host_callback_t vhci_callback = {
    .notify_host_send_available = notify_host_send_available,
    .notify_host_recv = notify_host_recv,
};


/* Send one HCI command and wait for its Command Complete; true only on
 * status 0x00. Mirrors the RTEMS port's hci_cmd(). */
static bool hci_cmd(const char *name, const uint8_t *cmd, uint16_t len)
{
    printf("HCI %-22s ->", name);
    for (uint16_t i = 0; i < len; i++) {
        printf(" %02x", cmd[i]);
    }
    printf("\n");

    s_response_len = 0;
    esp_vhci_host_send_packet((uint8_t *) cmd, len);

    if (xSemaphoreTake(s_response_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
        printf("    %-22s <- (no response)\n", "");
        return false;
    }

    printf("    %-22s <-", "");
    for (uint16_t i = 0; i < s_response_len; i++) {
        printf(" %02x", s_response[i]);
    }

    bool ok = s_response_len >= 7 && s_response[0] == 0x04
              && s_response[1] == 0x0e && s_response[6] == 0x00;
    printf("   %s\n", ok ? "OK" : "UNEXPECTED");
    return ok;
}

void app_main(void)
{
    printf("\nBLE controller-only probe (real ESP-IDF v5.3.1 control experiment)\n");

    s_response_sem = xSemaphoreCreateBinary();
    if (s_response_sem == NULL) {
        printf("FAIL: xSemaphoreCreateBinary\n");
        return;
    }

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    printf("calling esp_bt_controller_init...\n");
    esp_err_t err = esp_bt_controller_init(&cfg);
    if (err != ESP_OK) {
        printf("FAIL: esp_bt_controller_init: %d\n", err);
        return;
    }

    printf("calling esp_bt_controller_enable(ESP_BT_MODE_BLE)...\n");
    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
        printf("FAIL: esp_bt_controller_enable: %d\n", err);
        return;
    }

    printf("registering VHCI callback...\n");
    err = esp_vhci_host_register_callback(&vhci_callback);
    if (err != ESP_OK) {
        printf("FAIL: esp_vhci_host_register_callback: %d\n", err);
        return;
    }

    /* Same controller-only advertising sequence as the RTEMS port's
     * examples/ble_vhci_smoke/init.c, byte for byte, so the two can be
     * compared over the air on the same board. Deliberately skips HCI Reset:
     * that is the one command that crashes the RTEMS port, and including it
     * here would make the two runs non-comparable. */
    static const uint8_t hci_read_local_version[]  = { 0x01, 0x01, 0x10, 0x00 };
    static const uint8_t hci_le_read_buffer_size[] = { 0x01, 0x02, 0x20, 0x00 };
    static const uint8_t hci_le_set_adv_params[] = {
        0x01, 0x06, 0x20, 0x0f,
        0xa0, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x07, 0x00
    };
    static const uint8_t hci_le_set_adv_data[] = {
        0x01, 0x08, 0x20, 0x20,
        0x0a,
        0x02, 0x01, 0x06,
        0x06, 0x09, 'R', 'T', 'E', 'M', 'S',
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    static const uint8_t hci_le_set_adv_enable[] = { 0x01, 0x0a, 0x20, 0x01, 0x01 };

    /* TEST (2026-09-14): does advertising actually reach the air without an
     * HCI Reset first? Both this probe and the RTEMS port ACK every
     * advertising command with status 0x00 yet are invisible to a real BLE
     * scanner. Every real host stack sends Reset before anything else, so
     * send it here and re-scan - if that is what makes advertising visible,
     * then "skip Reset" is NOT a valid fix for the RTEMS port. */
#if SEND_HCI_RESET
    static const uint8_t hci_reset[] = { 0x01, 0x03, 0x0c, 0x00 };
    if (!hci_cmd("Reset", hci_reset, sizeof(hci_reset))) {
        printf("FAIL: HCI Reset\n");
        return;
    }
#else
    printf("(HCI Reset deliberately skipped)\n");
#endif

    if (!hci_cmd("Read Local Version", hci_read_local_version, sizeof(hci_read_local_version))
        || !hci_cmd("LE Read Buffer Size", hci_le_read_buffer_size, sizeof(hci_le_read_buffer_size))
        || !hci_cmd("LE Set Adv Parameters", hci_le_set_adv_params, sizeof(hci_le_set_adv_params))
        || !hci_cmd("LE Set Adv Data", hci_le_set_adv_data, sizeof(hci_le_set_adv_data))
        || !hci_cmd("LE Set Adv Enable", hci_le_set_adv_enable, sizeof(hci_le_set_adv_enable))) {
        printf("FAIL: advertising setup did not complete\n");
        return;
    }

    printf("PASS: real-IDF controller advertising as \"RTEMS\" - scan for it\n");

    for (int i = 0; i < 60; i++) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        printf("advertising... t=%ds\n", (i + 1) * 5);
    }
}
