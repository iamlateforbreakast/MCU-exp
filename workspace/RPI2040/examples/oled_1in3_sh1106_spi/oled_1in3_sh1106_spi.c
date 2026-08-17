/* Drives a genuine 1.3" 128x64 monochrome OLED module over SPI on the RP2040.
 * Draws a border (for an orientation sanity check), then animates a small
 * square scanning back and forth across the screen.
 *
 * Not to be confused with workspace/RPI2040/examples/lcd_12864_st7565_spi in
 * this repo: an earlier "1.3in OLED" AliExpress listing turned out on
 * inspection to actually be a GMG12864-06D LCD (ST7565R controller, a
 * completely different chip/protocol) rather than a true OLED - hence that
 * example's name and driver. This file is for an actual SH1106 OLED module;
 * check your board's own part number/silkscreen if unsure which you have,
 * since the two are easy to conflate from a seller listing alone.
 *
 * Chip: 1.3" 128x64 OLED modules are overwhelmingly SH1106-based (distinct
 * from the SSD1306 used on the more common 0.96" size - multiple independent
 * listings for this exact module confirm SH1106). If your specific board
 * turns out to be SSD1306 instead, the two chips are close but not command-
 * compatible: SSD1306 uses a different charge-pump command (0x8D instead of
 * SH1106's 0xAD) and supports a horizontal/vertical addressing mode SH1106
 * lacks (SH1106 only has page addressing, used here).
 *
 * Wiring (this module's own labeled pinout): SI -> GPIO3 (MOSI), SCL -> GPIO2
 * (SCK), CS -> GPIO1, RS -> GPIO0 (DC), RSE -> GPIO4 (RST), VDD -> 3V3, plus
 * GND. GPIO0-3 is spi0's alternate-function pin group (RX/CSn/SCK/TX, same
 * repeating pattern used elsewhere in this repo at GPIO4-7/16-19); GPIO0
 * would normally be spi0 RX, but is used here as a plain GPIO for DC instead
 * since this module has no MISO/data-out line and doesn't need it. No BUSY
 * pin either, unlike the e-paper example - OLED writes just take effect, so
 * there's nothing to poll and no way to detect bad wiring in software; if
 * nothing lights up, it's wiring/power, not a timeout you can catch here.
 *
 * The init command sequence, the SH1106's +2 column RAM offset (it has 132
 * columns of RAM for a 128-pixel-wide visible area, centered), and the
 * page/column addressing commands are taken from the u8g2 graphics library's
 * actual SH1106 driver (olikraus/u8g2, csrc/u8x8_d_ssd1306_128x64_noname.c,
 * the sh1106_128x64_winstar variant) - a widely-used, heavily-tested
 * library - fetched and cross-checked rather than written from memory.
 *
 * One thing NOT taken from that source, and not verified against this
 * specific module: the segment-remap (0xA1) and COM-scan-direction (0xC8)
 * commands below. u8g2's SH1106 sequence omits both, relying on the chip's
 * power-on defaults (0xA0/0xC0) - which apparently produces correct
 * orientation on the specific "winstar" module that sequence targets. Many
 * generic/clone 1.3" SH1106 breakout boards need 0xA1/0xC8 instead for
 * non-mirrored, right-side-up output, which is what's used here as the more
 * common convention for this kind of generic module - but per the lesson
 * learned on this repo's e-paper example, treat this as a guess until
 * confirmed: if the border/animation appears mirrored or flipped, try
 * toggling 0xA1<->0xA0 and/or 0xC8<->0xC0 below.
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
#define DC_PIN 0
#define RST_PIN 4
/* u8g2's maintainers lowered their own SH1106 default from 8MHz to 4MHz after
 * reliability reports (github.com/olikraus/u8g2 issue #750/#551); going lower
 * still since breadboard/jumper wiring has worse signal integrity than a PCB. */
#define SPI_BAUDRATE (1 * 1000 * 1000)

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGES (OLED_HEIGHT / 8)
#define OLED_BUF_SIZE (OLED_WIDTH * OLED_PAGES)
#define OLED_COL_OFFSET 2 /* SH1106: 132 RAM columns, 128 visible, centered */

static inline void cs_select(void)
{
    gpio_put(CS_PIN, 0); /* active low */
}

static inline void cs_deselect(void)
{
    gpio_put(CS_PIN, 1);
}

static void oled_write_cmd(uint8_t cmd)
{
    gpio_put(DC_PIN, 0); /* DC low selects command mode */
    cs_select();
    spi_write_blocking(SPI_PORT, &cmd, 1);
    cs_deselect();
    gpio_put(DC_PIN, 1); /* back to data mode */
}

static void oled_write_data(const uint8_t *buf, size_t len)
{
    cs_select();
    spi_write_blocking(SPI_PORT, buf, len);
    cs_deselect();
}

static void oled_reset(void)
{
    gpio_put(RST_PIN, 0);
    sleep_ms(10);
    gpio_put(RST_PIN, 1);
    sleep_ms(10);
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
    oled_write_cmd(0xAD); /* DC-DC control (SH1106-specific charge pump) */
    oled_write_cmd(0x8B); /* built-in DC-DC on */
    oled_write_cmd(0xA1); /* segment remap - see file header re: unverified for this module */
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
    oled_write_cmd(0x10 | (x >> 4)); /* set higher column address nibble */
    oled_write_cmd(0x00 | (x & 0x0F)); /* set lower column address nibble */
}

/* Framebuffer layout: OLED_PAGES rows of OLED_WIDTH bytes; each byte packs 8
 * vertically-stacked pixels, LSB = top pixel of that page - the standard
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

    printf("Initializing 1.3in SH1106 OLED...\n");
    oled_init();
    printf("Init sequence sent, entering animation loop. If the screen is still "
           "blank, this confirms the RP2040 is running fine and the problem is "
           "downstream (wiring/power/chip) rather than a hang in oled_init().\n");

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

        sleep_ms(30);
    }
}
