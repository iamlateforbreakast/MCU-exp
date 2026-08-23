/*
 * Drives a 1.3" 128x64 monochrome SH1106 OLED module over SPI on the
 * ESP32-C3, via the draft GP-SPI2 driver in ../../upstream-spi-driver/.
 * Draws a border plus a small square scanning back and forth, so it's
 * easy to confirm the SPI link and controller init from across the room.
 *
 * This is a port of ../../../RPI2040/examples/oled_1in3_sh1106_spi's
 * driver logic to RTEMS's generic SPI bus (dev/spi/spi.h) and GPIO
 * (bsp/gpio.h) APIs - see that file's header comment for the sourcing of
 * the init sequence (u8g2's actual SH1106 driver, olikraus/u8g2), the
 * SH1106-vs-SSD1306 identification note, and the caveat that the
 * segment-remap/COM-scan-direction commands (0xA1/0xC8 below) are a
 * guess for generic clone modules, not verified against this specific
 * board - try 0xA0/0xC0 instead if the output comes out mirrored/flipped.
 *
 * Wiring (this module's own labeled pinout, 7-pin SPI variant - no
 * MISO/data-out line, so MISO_PIN below is just a spare GPIO the SPI
 * driver needs a number for and is left unconnected):
 *   VDD -> 3V3   GND -> GND
 *   SCL -> GPIO4 (SCK)   SI -> GPIO5 (MOSI)   RSE -> GPIO6 (RST)
 *   RS  -> GPIO7 (DC)    CS -> GPIO10
 *
 * STATUS: builds against real hardware once the SPI driver is integrated
 * per ../../upstream-spi-driver/README.md's "Integration steps" (same
 * not-persisted-in-the-image caveat as the GPIO driver - see
 * ../../ESP32-C3-RTEMS.md) - confirmed 2026-08-23, but it hangs forever on
 * the very first oled_write_cmd() call and never proceeds. The SPI driver
 * itself deadlocks in an unbounded poll loop on every transaction (see
 * that README's status section for what's been ruled out) - this is
 * downstream of that bug, not a problem in this file.
 */

#include <rtems.h>
#include <bsp/gpio.h>
#include <bsp/esp32c3-spi.h>
#include <dev/spi/spi.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>

#define SCK_PIN  4
#define MOSI_PIN 5
#define RST_PIN  6
#define DC_PIN   7
#define MISO_PIN 3 /* unconnected - this module has no MISO/data-out line */
#define CS_PIN   10

/* u8g2 lowered their own SH1106 default from 8MHz to 4MHz after
 * reliability reports (github.com/olikraus/u8g2 issue #750/#551); going
 * lower still since breadboard/jumper wiring has worse signal integrity
 * than a PCB - matches the RP2040 port's choice. */
#define SPI_SPEED_HZ 1000000

#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_PAGES      ( OLED_HEIGHT / 8 )
#define OLED_BUF_SIZE   ( OLED_WIDTH * OLED_PAGES )
#define OLED_COL_OFFSET 2 /* SH1106: 132 RAM columns, 128 visible, centered */

static int spi_fd;

static void fatal_if_failed( rtems_status_code sc, const char *what )
{
  if ( sc != RTEMS_SUCCESSFUL ) {
    printf( "%s failed: %s\n", what, rtems_status_text( sc ) );
    exit( 1 );
  }
}

static void spi_xfer( const uint8_t *tx, size_t len )
{
  spi_ioc_transfer msg;

  memset( &msg, 0, sizeof( msg ) );
  msg.tx_buf = tx;
  msg.len = len;
  msg.speed_hz = SPI_SPEED_HZ;
  msg.bits_per_word = 8;
  msg.mode = SPI_MODE_0;

  if ( ioctl( spi_fd, SPI_IOC_MESSAGE( 1 ), &msg ) < 0 ) {
    perror( "SPI_IOC_MESSAGE" );
    exit( 1 );
  }
}

static void oled_write_cmd( uint8_t cmd )
{
  rtems_gpio_clear( DC_PIN ); /* DC low selects command mode */
  spi_xfer( &cmd, 1 );
  rtems_gpio_set( DC_PIN ); /* back to data mode */
}

static void oled_write_data( const uint8_t *buf, size_t len )
{
  spi_xfer( buf, len );
}

static void oled_reset( void )
{
  rtems_gpio_clear( RST_PIN );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 10 ) );
  rtems_gpio_set( RST_PIN );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 10 ) );
}

static void oled_init( void )
{
  oled_reset();

  oled_write_cmd( 0xAE ); /* display off */
  oled_write_cmd( 0xD5 ); /* clock divide / oscillator frequency */
  oled_write_cmd( 0x50 );
  oled_write_cmd( 0xA8 ); /* multiplex ratio */
  oled_write_cmd( 0x3F ); /* 64 (0x3F = 63, i.e. 64 rows, 0-indexed) */
  oled_write_cmd( 0xD3 ); /* display offset */
  oled_write_cmd( 0x00 );
  oled_write_cmd( 0x40 ); /* display start line = 0 */
  oled_write_cmd( 0xAD ); /* DC-DC control (SH1106-specific charge pump) */
  oled_write_cmd( 0x8B ); /* built-in DC-DC on */
  oled_write_cmd( 0xA1 ); /* segment remap - see file header re: unverified */
  oled_write_cmd( 0xC8 ); /* COM output scan direction reversed - see file header */
  oled_write_cmd( 0xDA ); /* COM pins hardware configuration */
  oled_write_cmd( 0x12 );
  oled_write_cmd( 0xD9 ); /* pre-charge period */
  oled_write_cmd( 0x22 );
  oled_write_cmd( 0xDB ); /* VCOM deselect level */
  oled_write_cmd( 0x35 );
  oled_write_cmd( 0x32 ); /* set pump voltage: 8.0V */
  oled_write_cmd( 0x81 ); /* contrast */
  oled_write_cmd( 0xFF );
  oled_write_cmd( 0xA6 ); /* normal (not inverted) display */
  oled_write_cmd( 0xA4 ); /* resume to RAM content display */
  oled_write_cmd( 0xAF ); /* display on */
}

static void oled_set_pos( uint8_t page, uint8_t col )
{
  uint8_t x = (uint8_t) ( col + OLED_COL_OFFSET );

  oled_write_cmd( 0xB0 | ( page & 0x0F ) ); /* set page address */
  oled_write_cmd( 0x10 | ( x >> 4 ) ); /* set higher column address nibble */
  oled_write_cmd( 0x00 | ( x & 0x0F ) ); /* set lower column address nibble */
}

/* Framebuffer layout: OLED_PAGES rows of OLED_WIDTH bytes; each byte packs
 * 8 vertically-stacked pixels, LSB = top pixel of that page - the standard
 * SH1106/SSD1306-family page-RAM layout. */
static uint8_t framebuffer[ OLED_BUF_SIZE ];

static void fb_clear( uint8_t *fb )
{
  memset( fb, 0x00, OLED_BUF_SIZE );
}

static void fb_set_pixel( uint8_t *fb, int x, int y, bool on )
{
  int     page;
  int     bit;
  int     byte_index;
  uint8_t mask;

  if ( x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT ) {
    return;
  }

  page = y / 8;
  bit = y % 8;
  byte_index = page * OLED_WIDTH + x;
  mask = (uint8_t) ( 1u << bit );

  if ( on ) {
    fb[ byte_index ] |= mask;
  } else {
    fb[ byte_index ] &= (uint8_t) ~mask;
  }
}

static void fb_draw_rect( uint8_t *fb, int x0, int y0, int x1, int y1, bool on )
{
  int x;
  int y;

  for ( x = x0; x <= x1; ++x ) {
    fb_set_pixel( fb, x, y0, on );
    fb_set_pixel( fb, x, y1, on );
  }

  for ( y = y0; y <= y1; ++y ) {
    fb_set_pixel( fb, x0, y, on );
    fb_set_pixel( fb, x1, y, on );
  }
}

static void fb_fill_rect( uint8_t *fb, int x0, int y0, int x1, int y1, bool on )
{
  int x;
  int y;

  for ( y = y0; y <= y1; ++y ) {
    for ( x = x0; x <= x1; ++x ) {
      fb_set_pixel( fb, x, y, on );
    }
  }
}

static void oled_display( const uint8_t *fb )
{
  int page;

  for ( page = 0; page < OLED_PAGES; ++page ) {
    oled_set_pos( (uint8_t) page, 0 );
    oled_write_data( &fb[ page * OLED_WIDTH ], OLED_WIDTH );
  }
}

rtems_task Init( rtems_task_argument ignored )
{
  rtems_status_code sc;
  int                rv;
  const int          square = 10;
  int                x = 0;
  int                dir = 1;

  (void) ignored;

  sc = rtems_gpio_initialize();
  fatal_if_failed( sc, "rtems_gpio_initialize" );

  sc = rtems_gpio_request_pin( RST_PIN, DIGITAL_OUTPUT, false, true, NULL );
  fatal_if_failed( sc, "rtems_gpio_request_pin(RST)" );

  sc = rtems_gpio_request_pin( DC_PIN, DIGITAL_OUTPUT, false, true, NULL );
  fatal_if_failed( sc, "rtems_gpio_request_pin(DC)" );

  rv = spi_bus_register_esp32c3( "/dev/spi0", SCK_PIN, MOSI_PIN, MISO_PIN, CS_PIN );
  if ( rv != 0 ) {
    printf( "spi_bus_register_esp32c3 failed: %d\n", rv );
    exit( 1 );
  }

  spi_fd = open( "/dev/spi0", O_RDWR );
  if ( spi_fd < 0 ) {
    perror( "open(/dev/spi0)" );
    exit( 1 );
  }

  printf( "Initializing 1.3in SH1106 OLED...\n" );
  oled_init();
  printf(
    "Init sequence sent, entering animation loop. If the screen is still "
    "blank, this confirms the ESP32-C3 is running fine and the problem is "
    "downstream (wiring/power/chip) rather than a hang in oled_init().\n"
  );

  while ( true ) {
    fb_clear( framebuffer );
    fb_draw_rect( framebuffer, 0, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1, true );
    fb_fill_rect(
      framebuffer,
      x,
      ( OLED_HEIGHT - square ) / 2,
      x + square - 1,
      ( OLED_HEIGHT + square ) / 2 - 1,
      true
    );
    oled_display( framebuffer );

    x += dir * 2;
    if ( x <= 1 || x >= OLED_WIDTH - square - 1 ) {
      dir = -dir;
    }

    rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 30 ) );
  }
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

#define CONFIGURE_MAXIMUM_TASKS 1
/* rtems_gpio_initialize() needs one semaphore for its internal bank lock
 * (see ../gpio_led_blink/init.c's comment) - CONFIGURE_MAXIMUM_SEMAPHORES
 * otherwise defaults to 0. */
#define CONFIGURE_MAXIMUM_SEMAPHORES 1
/* stdin/stdout/stderr (3) plus /dev/spi0. */
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 4

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT

#include <rtems/confdefs.h>
