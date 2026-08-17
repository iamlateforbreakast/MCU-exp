/* Drives a WeAct Studio 1.54" e-paper module (SSD1681 controller, 200x200,
 * monochrome) over SPI on the RP2040. Draws two test patterns a few seconds
 * apart, then puts the panel into deep sleep.
 *
 * Wiring: DIN -> GPIO3 (MOSI), CLK -> GPIO2 (SCK), CS -> GPIO1,
 * DC -> GPIO4, RST -> GPIO5, BUSY -> GPIO6, plus 3V3/GND. GPIO0/1/2/3 is
 * spi0's first alternate-function pin group (RX/CSn/SCK/TX, same repeating
 * pattern as GPIO16-19 and GPIO4-7); this module has no MISO/data-out line,
 * so GPIO0 (spi0 RX) is left unused/unconfigured. VCC is 3.3V ONLY - per
 * WeAct's own module documentation, 5V will damage the panel.
 *
 * The command sequence (init/set-RAM-position/power-on/update/deep-sleep) and
 * the BUSY pin's active-high polarity are taken from WeAct Studio's own
 * reference driver for this exact module (WeActStudio.EpaperModule repo,
 * Example/EpaperModuleTest_RaspberryPi/epaper/epaper.c, EPD154 code path),
 * fetched and cross-checked rather than written from memory - e-paper
 * controllers have no readback, so a wrong init sequence just produces a
 * blank/garbled screen with no error, and is very hard to debug blind.
 *
 * One thing NOT confirmed from that source: which RAM bit value (0 or 1)
 * renders black vs white. This follows the SSD1681's typical convention
 * (1 = white, 0 = black) - if your test patterns render inverted, flip the
 * BLACK/WHITE macros below.
 *
 * Also unconfirmed without real hardware: whether WeAct's Y-address
 * inversion (used here to match their driver) results in the pattern drawn
 * appearing right-side up on your specific panel orientation. The two test
 * patterns below (a border + corner square, then an inverted center square)
 * are deliberately simple shapes so a flipped/mirrored orientation is still
 * obviously "it's working, the picture's just flipped" rather than looking
 * broken.
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define SPI_PORT spi0
#define SCK_PIN 2
#define MOSI_PIN 3
#define CS_PIN 1
#define DC_PIN 4
#define RST_PIN 5
#define BUSY_PIN 6
#define SPI_BAUDRATE (4 * 1000 * 1000) /* conservative; SSD1681 supports faster */

#define EPD_WIDTH 200
#define EPD_HEIGHT 200
#define EPD_WIDTH_BYTES (EPD_WIDTH / 8)
#define EPD_BUF_SIZE (EPD_WIDTH_BYTES * EPD_HEIGHT)

#define PIXEL_WHITE 0xFF
#define PIXEL_BLACK 0x00

static inline void cs_select(void)
{
    gpio_put(CS_PIN, 0); /* active low */
}

static inline void cs_deselect(void)
{
    gpio_put(CS_PIN, 1);
}

static void epd_write_cmd(uint8_t cmd)
{
    gpio_put(DC_PIN, 0); /* DC low selects command mode */
    cs_select();
    spi_write_blocking(SPI_PORT, &cmd, 1);
    cs_deselect();
    gpio_put(DC_PIN, 1); /* back to data mode */
}

static void epd_write_data(uint8_t data)
{
    cs_select();
    spi_write_blocking(SPI_PORT, &data, 1);
    cs_deselect();
}

static void epd_write_data_buf(const uint8_t *buf, size_t len)
{
    cs_select();
    spi_write_blocking(SPI_PORT, buf, len);
    cs_deselect();
}

static void epd_reset(void)
{
    gpio_put(RST_PIN, 0);
    sleep_ms(50);
    gpio_put(RST_PIN, 1);
    sleep_ms(50);
}

/* BUSY reads high while the panel is busy (confirmed for the SSD1681/EPD154
 * case in WeAct's reference driver - some other controllers they support use
 * the opposite polarity, so don't copy this assumption to other panels). */
static bool epd_wait_busy(void)
{
    uint32_t waited_ms = 0;
    while (gpio_get(BUSY_PIN)) {
        sleep_ms(1);
        if (++waited_ms > 40000) {
            return false; /* timeout - panel likely not wired/responding */
        }
    }
    return true;
}

static void epd_set_pos(uint16_t x, uint16_t y)
{
    uint8_t rx = x / 8;
    uint16_t ry = (EPD_HEIGHT - 1) - y; /* WeAct's driver inverts Y for this panel */

    epd_write_cmd(0x4E); /* set RAM X address counter */
    epd_write_data(rx);

    epd_write_cmd(0x4F); /* set RAM Y address counter */
    epd_write_data(ry & 0xFF);
    epd_write_data((ry >> 8) & 0x01);
}

static bool epd_power_on(void)
{
    epd_write_cmd(0x22); /* display update control 2 */
    epd_write_data(0xF8);
    epd_write_cmd(0x20); /* activate display update sequence */
    return epd_wait_busy();
}

static bool epd_update(void)
{
    epd_write_cmd(0x22); /* display update control 2 */
    epd_write_data(0xF4);
    epd_write_cmd(0x20); /* activate display update sequence */
    return epd_wait_busy();
}

static bool epd_init(void)
{
    epd_reset();

    epd_write_cmd(0x01); /* driver output control */
    epd_write_data(0xC7); /* MUX = 199 (200 lines), low byte */
    epd_write_data(0x00); /* MUX high byte */
    epd_write_data(0x01); /* gate scan direction */

    epd_write_cmd(0x11); /* data entry mode */
    epd_write_data(0x01);

    epd_write_cmd(0x44); /* RAM X address start/end */
    epd_write_data(0x00);
    epd_write_data(0x18); /* (200 / 8) - 1 */

    epd_write_cmd(0x45); /* RAM Y address start/end */
    epd_write_data(0xC7); /* end Y low byte (199) */
    epd_write_data(0x00); /* end Y high byte */
    epd_write_data(0x00); /* start Y low byte */
    epd_write_data(0x00); /* start Y high byte */

    epd_write_cmd(0x3C); /* border waveform control */
    epd_write_data(0x05);

    epd_write_cmd(0x18); /* temperature sensor selection: internal */
    epd_write_data(0x80);

    epd_set_pos(0, 0);
    return epd_power_on();
}

/* Writes the same image to both the "previous"/red RAM bank (0x26) and the
 * current BW RAM bank (0x24) before triggering a refresh, matching WeAct's
 * own epd_displayBW - writing both keeps a subsequent full update's ghosting
 * behavior consistent with what the panel expects. */
static bool epd_display(const uint8_t *image)
{
    epd_set_pos(0, 0);
    epd_write_cmd(0x26);
    epd_write_data_buf(image, EPD_BUF_SIZE);

    epd_set_pos(0, 0);
    epd_write_cmd(0x24);
    epd_write_data_buf(image, EPD_BUF_SIZE);

    return epd_update();
}

static void epd_sleep(void)
{
    epd_write_cmd(0x10); /* deep sleep mode */
    epd_write_data(0x01); /* mode 1: RAM retained; needs epd_reset() to wake */
}

static void fb_fill(uint8_t *fb, bool black)
{
    memset(fb, black ? PIXEL_BLACK : PIXEL_WHITE, EPD_BUF_SIZE);
}

static void fb_set_pixel(uint8_t *fb, int x, int y, bool black)
{
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) {
        return;
    }
    int byte_index = y * EPD_WIDTH_BYTES + (x / 8);
    uint8_t mask = 0x80 >> (x % 8);
    if (black) {
        fb[byte_index] &= (uint8_t)~mask;
    } else {
        fb[byte_index] |= mask;
    }
}

static void fb_draw_rect(uint8_t *fb, int x0, int y0, int x1, int y1, bool black)
{
    for (int x = x0; x <= x1; x++) {
        fb_set_pixel(fb, x, y0, black);
        fb_set_pixel(fb, x, y1, black);
    }
    for (int y = y0; y <= y1; y++) {
        fb_set_pixel(fb, x0, y, black);
        fb_set_pixel(fb, x1, y, black);
    }
}

static void fb_fill_rect(uint8_t *fb, int x0, int y0, int x1, int y1, bool black)
{
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            fb_set_pixel(fb, x, y, black);
        }
    }
}

static uint8_t framebuffer[EPD_BUF_SIZE];

int main(void)
{
    stdio_init_all();
    sleep_ms(2000); /* give a USB CDC terminal time to attach before the first prints */

    spi_init(SPI_PORT, SPI_BAUDRATE);
    gpio_set_function(SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);

    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    cs_deselect();

    gpio_init(DC_PIN);
    gpio_set_dir(DC_PIN, GPIO_OUT);
    gpio_put(DC_PIN, 1);

    gpio_init(RST_PIN);
    gpio_set_dir(RST_PIN, GPIO_OUT);
    gpio_put(RST_PIN, 1);

    gpio_init(BUSY_PIN);
    gpio_set_dir(BUSY_PIN, GPIO_IN);

    printf("Initializing WeAct 1.54in ePaper (SSD1681)...\n");
    if (!epd_init()) {
        printf("epd_init: timed out waiting for BUSY - check wiring "
               "(SCK/DIN/CS/DC/RST/BUSY) and that VCC is 3.3V\n");
    }

    /* Pattern 1: white background, black border, filled square in the corner. */
    fb_fill(framebuffer, false);
    fb_draw_rect(framebuffer, 0, 0, EPD_WIDTH - 1, EPD_HEIGHT - 1, true);
    fb_fill_rect(framebuffer, 10, 10, 60, 60, true);
    printf("Drawing pattern 1...\n");
    if (!epd_display(framebuffer)) {
        printf("epd_display: timed out waiting for BUSY\n");
    }
    sleep_ms(5000);

    /* Pattern 2: black background, white square in the middle. */
    fb_fill(framebuffer, true);
    fb_fill_rect(framebuffer, 70, 70, 140, 140, false);
    printf("Drawing pattern 2...\n");
    if (!epd_display(framebuffer)) {
        printf("epd_display: timed out waiting for BUSY\n");
    }
    sleep_ms(5000);

    printf("Entering deep sleep\n");
    epd_sleep();

    while (1) {
        tight_loop_contents();
    }
}
