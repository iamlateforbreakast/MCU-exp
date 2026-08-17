/* Drives a GMG12864-06D 128x64 monochrome graphic LCD (ST7565R controller)
 * over SPI on the RP2040. Draws a border (for an orientation sanity check),
 * then animates a small square scanning back and forth across the screen.
 *
 * This module was initially (incorrectly) assumed to be a 1.3" SH1106 OLED
 * based on the seller listing alone - it's actually an ST7565R-driven LCD
 * (identified from the board's own part number/silkscreen), a completely
 * different controller family with its own command set. The SH1106 command
 * bytes this file used to send are meaningless to an ST7565R and, notably,
 * ST7565-family LCDs need a staged internal voltage-regulator/booster
 * power-up sequence with real delays between each stage (missing or wrong
 * here would leave the screen blank) that OLED controllers don't need at
 * all - which fully explains the "completely blank" symptom reported
 * against the earlier SH1106-based version of this file.
 *
 * Wiring (this module's own labeled 13-pin header, reported by the user):
 * CS -> GPIO1, RSE(reset) -> GPIO4, RS(DC) -> GPIO0, SCL(SCK) -> GPIO2,
 * SI(MOSI) -> GPIO3, VDD -> 3V3, VSS -> GND. Backlight: A (anode) -> 3V3
 * through a current-limiting resistor (~100-220ohm), K (cathode) -> GND -
 * wired directly, not through a GPIO; this file doesn't control backlight
 * brightness. The four C_* pins (C_SCL/C_CS/C_SO/C_SI) are internal
 * factory/test pins on this board and are not connected. GPIO0-3 is spi0's
 * alternate-function pin group (RX/CSn/SCK/TX, same repeating pattern used
 * elsewhere in this repo at GPIO4-7/16-19); GPIO0 would normally be spi0 RX,
 * used here as a plain GPIO for DC instead since this module has no
 * MISO/data-out line.
 *
 * The init command sequence (LCD bias, ADC/COM direction, the three-stage
 * power control ramp with its delays, resistor ratio, contrast/electronic
 * volume, display-on) and the page/column addressing commands are taken
 * from Adafruit's actual ST7565 driver (adafruit/Adafruit_CircuitPython_ST7565,
 * adafruit_st7565.py), fetched and cross-checked rather than written from
 * memory - including resolving a discrepancy between two reads of the same
 * file over which LCD bias command it actually sends (0xA3, confirmed by
 * grepping the source directly rather than trusting a first summary).
 *
 * Contrast: confirmed on real hardware via a startup sweep (0x08-0x3F) - 0x08
 * is what actually renders correctly on this panel, well below both the
 * reference driver's own default (0, undocumented/untuned) and this file's
 * first guess (0x24, a generic "moderate" value that turned out too high and
 * showed as a uniform gray field with no visible shapes). This driver
 * applies no column offset (unlike the SH1106 example in this repo, which
 * needs +2) - the reference driver uses none.
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
#define SPI_BAUDRATE (1 * 1000 * 1000)

#define LCD_WIDTH 128
#define LCD_HEIGHT 64
#define LCD_PAGES (LCD_HEIGHT / 8)
#define LCD_BUF_SIZE (LCD_WIDTH * LCD_PAGES)
#define LCD_CONTRAST 0x08 /* 0-0x3F; confirmed on real hardware via startup sweep */

static inline void cs_select(void)
{
    gpio_put(CS_PIN, 0); /* active low */
}

static inline void cs_deselect(void)
{
    gpio_put(CS_PIN, 1);
}

static void lcd_write_cmd(uint8_t cmd)
{
    gpio_put(DC_PIN, 0); /* DC low selects command mode */
    cs_select();
    spi_write_blocking(SPI_PORT, &cmd, 1);
    cs_deselect();
    gpio_put(DC_PIN, 1); /* back to data mode */
}

static void lcd_write_data(const uint8_t *buf, size_t len)
{
    cs_select();
    spi_write_blocking(SPI_PORT, buf, len);
    cs_deselect();
}

static void lcd_reset(void)
{
    gpio_put(RST_PIN, 0);
    sleep_ms(500);
    gpio_put(RST_PIN, 1);
    sleep_ms(500);
}

static void lcd_set_contrast(uint8_t contrast)
{
    lcd_write_cmd(0x81); /* set electronic volume (contrast), 2-byte command */
    lcd_write_cmd(contrast & 0x3F);
}

static void lcd_init(void)
{
    lcd_reset();

    lcd_write_cmd(0xA3); /* LCD bias select: 1/7 */
    lcd_write_cmd(0xA1); /* ADC select: reverse (segment remap) */
    lcd_write_cmd(0xC0); /* COM output mode: normal */
    lcd_write_cmd(0x40); /* display start line = 0 */

    lcd_write_cmd(0x2C); /* power control stage 1: booster circuit on */
    sleep_ms(50);
    lcd_write_cmd(0x2E); /* power control stage 2: + voltage regulator on */
    sleep_ms(50);
    lcd_write_cmd(0x2F); /* power control stage 3: + voltage follower on */
    sleep_ms(10);

    lcd_write_cmd(0x27); /* resistor ratio (regulator resistor ratio = 7) */
    lcd_set_contrast(LCD_CONTRAST);

    lcd_write_cmd(0xAF); /* display on */
    lcd_write_cmd(0xA4); /* all points normal (not the all-pixels-on test mode) */
}

static void lcd_set_pos(uint8_t page, uint8_t col)
{
    lcd_write_cmd(0xB0 | (page & 0x0F)); /* set page address */
    lcd_write_cmd(0x10 | (col >> 4)); /* set higher column address nibble */
    lcd_write_cmd(0x00 | (col & 0x0F)); /* set lower column address nibble */
}

/* Framebuffer layout: LCD_PAGES rows of LCD_WIDTH bytes; each byte packs 8
 * vertically-stacked pixels, LSB = top pixel of that page - the standard
 * ST7565/SH1106/SSD1306-family page-RAM layout. */
static uint8_t framebuffer[LCD_BUF_SIZE];

static void fb_clear(uint8_t *fb)
{
    memset(fb, 0x00, LCD_BUF_SIZE);
}

static void fb_set_pixel(uint8_t *fb, int x, int y, bool on)
{
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) {
        return;
    }
    int page = y / 8;
    int bit = y % 8;
    int byte_index = page * LCD_WIDTH + x;
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

static void lcd_display(const uint8_t *fb)
{
    for (int page = 0; page < LCD_PAGES; page++) {
        lcd_set_pos((uint8_t)page, 0);
        lcd_write_data(&fb[page * LCD_WIDTH], LCD_WIDTH);
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

    printf("Initializing GMG12864-06D (ST7565R) LCD...\n");
    lcd_init();
    printf("Init sequence sent, entering animation loop.\n");

    const int square = 10;
    int x = 0;
    int dir = 1;

    while (1) {
        fb_clear(framebuffer);
        fb_draw_rect(framebuffer, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, true);
        fb_fill_rect(framebuffer, x, (LCD_HEIGHT - square) / 2,
                     x + square - 1, (LCD_HEIGHT + square) / 2 - 1, true);
        lcd_display(framebuffer);

        x += dir * 2;
        if (x <= 1 || x >= LCD_WIDTH - square - 1) {
            dir = -dir;
        }

        sleep_ms(30);
    }
}
