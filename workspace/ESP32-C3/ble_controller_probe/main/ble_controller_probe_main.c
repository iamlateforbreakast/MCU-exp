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

    static uint8_t hci_reset_cmd[] = { 0x01, 0x03, 0x0C, 0x00 };
    printf("sending HCI Reset over VHCI...\n");
    esp_vhci_host_send_packet(hci_reset_cmd, sizeof(hci_reset_cmd));

    if (xSemaphoreTake(s_response_sem, pdMS_TO_TICKS(2000)) != pdTRUE) {
        printf("FAIL: no VHCI response within timeout\n");
        return;
    }

    printf("received %u bytes:", s_response_len);
    for (uint16_t i = 0; i < s_response_len; i++) {
        printf(" %02x", s_response[i]);
    }
    printf("\n");

    if (s_response_len >= 7 && s_response[0] == 0x04 && s_response[1] == 0x0E
        && s_response[4] == 0x03 && s_response[5] == 0x0C
        && s_response[6] == 0x00) {
        printf("PASS: valid HCI Command Complete for Reset, status 0x00\n");
    } else {
        printf("FAIL: unexpected response bytes\n");
    }
}
