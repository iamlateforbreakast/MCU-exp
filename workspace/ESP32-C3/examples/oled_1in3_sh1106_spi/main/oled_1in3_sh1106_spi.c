/*
 * Drives a 1.3" 128x64 monochrome SH1106 OLED module over SPI on the
 * ESP32-C3, via ESP-IDF's spi_master driver. Draws a border plus a small
 * square scanning back and forth, so it's easy to confirm the SPI link
 * and controller init from across the room.
 *
 * This is the same driver logic as
 * ../../../RPI2040/examples/oled_1in3_sh1106_spi and
 * ../../../ESP32-C3-RTEMS/examples/oled_1in3_sh1106_spi (see that RP2040
 * file's header comment for the sourcing of the init sequence - u8g2's
 * actual SH1106 driver, olikraus/u8g2 - the SH1106-vs-SSD1306
 * identification note, and the caveat that the segment-remap/COM-scan-
 * direction commands (0xA1/0xC8 below) are a guess for generic clone
 * modules, not verified against this specific board - try 0xA0/0xC0
 * instead if the output comes out mirrored/flipped), ported here to
 * ESP-IDF's `driver/spi_master.h` instead of RTEMS's generic SPI bus API
 * - see ../../ESP32-C3-RTEMS.md and
 * ../../../ESP32-C3-RTEMS/upstream-spi-driver/README.md for why: the
 * draft register-level RTEMS GP-SPI2 driver deadlocks on every real
 * transaction on this same hardware, root cause not yet found without an
 * oscilloscope, so this ESP-IDF port exists to get a known-working SPI
 * driver on this exact board/module while that's unresolved.
 *
 * Wiring (this module's own labeled pinout, 7-pin SPI variant - no
 * MISO/data-out line, so there's no MISO pin to wire; ESP-IDF's spi_master
 * driver also doesn't need one configured when the bus has no device that
 * uses it):
 *   VDD -> 3V3   GND -> GND
 *   SCL -> GPIO4 (SCK)   SI -> GPIO5 (MOSI)   RSE -> GPIO6 (RST)
 *   RS  -> GPIO7 (DC)    CS -> GPIO10
 *
 * Unlike the RTEMS driver, ESP-IDF's spi_master toggles CS automatically
 * (spics_io_num below) - no manual GPIO CS handling needed here.
 *
 * STATUS: confirmed working on real hardware (2026-08-23) - border and
 * scanning square both visible on the physical screen. One real bug hit
 * and fixed along the way: spi_bus_initialize() was first called with
 * SPI_DMA_DISABLED, which caps non-DMA polling transfers at 64 bytes on
 * this chip - too small for a 128-byte (OLED_WIDTH) page write, and
 * crashed every boot with `ESP_ERROR_CHECK failed: ESP_ERR_INVALID_ARG`
 * from spi_master's own check_trans_valid(). Switching to SPI_DMA_CH_AUTO
 * fixed it.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#define SCK_PIN  4
#define MOSI_PIN 5
#define RST_PIN  6
#define DC_PIN   7
#define CS_PIN   10

#define SPI_HOST_USED SPI2_HOST
/* u8g2 lowered their own SH1106 default from 8MHz to 4MHz after
 * reliability reports (github.com/olikraus/u8g2 issue #750/#551); going
 * lower still since breadboard/jumper wiring has worse signal integrity
 * than a PCB - matches the RP2040/RTEMS ports' choice. */
#define SPI_CLOCK_HZ (1 * 1000 * 1000)

#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_PAGES      (OLED_HEIGHT / 8)
#define OLED_BUF_SIZE   (OLED_WIDTH * OLED_PAGES)
#define OLED_COL_OFFSET 2 /* SH1106: 132 RAM columns, 128 visible, centered */

static const char *TAG = "oled_sh1106";
static spi_device_handle_t spi;

static void oled_write_cmd(uint8_t cmd)
{
    spi_transaction_t t = {0};

    gpio_set_level(DC_PIN, 0); /* DC low selects command mode */
    t.length = 8;
    t.tx_buffer = &cmd;
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
    gpio_set_level(DC_PIN, 1); /* back to data mode */
}

static void oled_write_data(const uint8_t *buf, size_t len)
{
    spi_transaction_t t = {0};

    t.length = len * 8;
    t.tx_buffer = buf;
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
}

static void oled_reset(void)
{
    gpio_set_level(RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

static void oled_init(void)
{
    oled_reset();

    oled_write_cmd(0xAE); /* display off */
    oled_write_cmd(0xD5); /* clock divide / oscillator frequency */
    oled_write_cmd(0x50);
    oled_write_cmd(0xA8); /* multiplex ratio */
    oled_write_cmd(0x3F); /* 64 (0x3F = 63, i.e. 64 rows, 0-indexed) */
    oled_write_cmd(0xD3); /* display offset */
    oled_write_cmd(0x00);
    oled_write_cmd(0x40); /* display start line = 0 */
    oled_write_cmd(0x20); /* memory addressing mode (SSD1306-only; SH1106 has
                            * no such command and ignores it, but if this
                            * module is actually SSD1306, POR default may not
                            * be page mode - force it explicitly instead of
                            * assuming) */
    oled_write_cmd(0x02); /* page addressing mode */
    oled_write_cmd(0x8D); /* charge pump - trying SSD1306's command/value
                            * (0x8D/0x14) instead of SH1106's (0xAD/0x8B) as
                            * an experiment: stable-but-scrambled output with
                            * verified-correct wiring/power points at wrong
                            * RAM addressing from a charge-pump/addressing
                            * mismatch, not a wiring fault. */
    oled_write_cmd(0x14);
    oled_write_cmd(0xA1); /* segment remap - see file header re: unverified */
    oled_write_cmd(0xC8); /* COM output scan direction reversed - see file header */
    oled_write_cmd(0xDA); /* COM pins hardware configuration */
    oled_write_cmd(0x12);
    oled_write_cmd(0xD9); /* pre-charge period */
    oled_write_cmd(0x22);
    oled_write_cmd(0xDB); /* VCOM deselect level */
    oled_write_cmd(0x35);
    oled_write_cmd(0x32); /* set pump voltage: 8.0V */
    oled_write_cmd(0x81); /* contrast */
    oled_write_cmd(0xFF);
    oled_write_cmd(0xA6); /* normal (not inverted) display */
    oled_write_cmd(0xA4); /* resume to RAM content display */
    oled_write_cmd(0xAF); /* display on */
}

static void oled_set_pos(uint8_t page, uint8_t col)
{
    uint8_t x = col + OLED_COL_OFFSET;

    oled_write_cmd(0xB0 | (page & 0x0F)); /* set page address */
    oled_write_cmd(0x10 | (x >> 4));      /* set higher column address nibble */
    oled_write_cmd(0x00 | (x & 0x0F));    /* set lower column address nibble */
}

/* Framebuffer layout: OLED_PAGES rows of OLED_WIDTH bytes; each byte packs
 * 8 vertically-stacked pixels, LSB = top pixel of that page - the standard
 * SH1106/SSD1306-family page-RAM layout. */
static uint8_t framebuffer[OLED_BUF_SIZE];

static void fb_clear(uint8_t *fb)
{
    memset(fb, 0x00, OLED_BUF_SIZE);
}

static void fb_set_pixel(uint8_t *fb, int x, int y, bool on)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }
    int page = y / 8;
    int bit = y % 8;
    int byte_index = page * OLED_WIDTH + x;
    uint8_t mask = 1u << bit;
    if (on) {
        fb[byte_index] |= mask;
    } else {
        fb[byte_index] &= (uint8_t)~mask;
    }
}

static void fb_draw_rect(uint8_t *fb, int x0, int y0, int x1, int y1, bool on)
{
    for (int x = x0; x <= x1; x++) {
        fb_set_pixel(fb, x, y0, on);
        fb_set_pixel(fb, x, y1, on);
    }
    for (int y = y0; y <= y1; y++) {
        fb_set_pixel(fb, x0, y, on);
        fb_set_pixel(fb, x1, y, on);
    }
}

static void fb_fill_rect(uint8_t *fb, int x0, int y0, int x1, int y1, bool on)
{
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            fb_set_pixel(fb, x, y, on);
        }
    }
}

static void oled_display(const uint8_t *fb)
{
    for (int page = 0; page < OLED_PAGES; page++) {
        oled_set_pos((uint8_t)page, 0);
        oled_write_data(&fb[page * OLED_WIDTH], OLED_WIDTH);
    }
}

void app_main(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = -1, /* this module has no MISO/data-out line */
        .sclk_io_num = SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = OLED_WIDTH,
    };
    /* SPI_DMA_DISABLED caps non-DMA polling transfers at 64 bytes on this
     * chip - too small for a 128-byte (OLED_WIDTH) page write. */
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_USED, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = CS_PIN,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_HOST_USED, &devcfg, &spi));

    gpio_reset_pin(DC_PIN);
    gpio_set_direction(DC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DC_PIN, 1);

    gpio_reset_pin(RST_PIN);
    gpio_set_direction(RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RST_PIN, 1);

    ESP_LOGI(TAG, "Initializing 1.3in SH1106 OLED...");
    oled_init();
    ESP_LOGI(TAG, "Init sequence sent, entering animation loop. If the "
                  "screen is still blank, this confirms the ESP32-C3 and "
                  "SPI bus are running fine and the problem is downstream "
                  "(wiring/power/chip) rather than a hang in oled_init().");

    const int square = 10;
    int x = 0;
    int dir = 1;

    while (1) {
        fb_clear(framebuffer);
        fb_draw_rect(framebuffer, 0, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1, true);
        fb_fill_rect(framebuffer, x, (OLED_HEIGHT - square) / 2,
                     x + square - 1, (OLED_HEIGHT + square) / 2 - 1, true);
        oled_display(framebuffer);

        x += dir * 2;
        if (x <= 1 || x >= OLED_WIDTH - square - 1) {
            dir = -dir;
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
