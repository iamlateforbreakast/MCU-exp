/*
 * Drives a 1.14" 135x240 ST7789 IPS SPI TFT (the "AITEXM ... ST7789 LCD
 * Board SPI" module from AliExpress) via the draft GP-SPI2 driver in
 * ../../upstream-spi-driver/, cycling the whole screen through solid
 * colors so it's easy to confirm the SPI link and controller init are
 * both working from across the room.
 *
 * Wiring (this module has no MISO pin - it's write-only - so MISO_PIN
 * below is just a spare GPIO the SPI driver needs a number for and is
 * left unconnected):
 *   VCC -> 3V3   GND -> GND   BLK -> 3V3 (backlight always on)
 *   SCL -> GPIO4   SDA -> GPIO5   RES -> GPIO6   DC -> GPIO7   CS -> GPIO10
 *
 * STATUS: builds against real hardware once the SPI driver is integrated
 * per ../../upstream-spi-driver/README.md's "Integration steps" (same
 * not-persisted-in-the-image caveat as the GPIO driver - see
 * ../../ESP32-C3-RTEMS.md) - confirmed 2026-08-23, but it hangs forever on
 * the very first st7789_write_cmd() call and never proceeds. The SPI
 * driver itself deadlocks in an unbounded poll loop on every transaction
 * (see that README's status section for what's been ruled out) - this is
 * downstream of that bug, not a problem in this file.
 *
 * SPI2_SOURCE_CLK_HZ (and therefore every speed_hz below) is a guess, not
 * a measurement - see upstream-spi-driver/README.md. SPI_SPEED_HZ is kept
 * conservative (10MHz, well under this draft's 40MHz cap) for reliability
 * over breadboard jumper wires; raise it once the link is confirmed
 * solid.
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

#define SCLK_PIN 4
#define MOSI_PIN 5
#define RST_PIN  6
#define DC_PIN   7
#define MISO_PIN 3 /* unconnected - this module has no MISO pin */
#define CS_PIN   10

#define ST7789_WIDTH  135
#define ST7789_HEIGHT 240

/* Offsets into the controller's 240x320 RAM for this particular panel
 * size/variant - the commonly cited values for 1.14" 135x240 ST7789
 * modules (e.g. TFT_eSPI's Setup for this panel). */
#define ST7789_COL_OFFSET 52
#define ST7789_ROW_OFFSET 40

#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_COLMOD  0x3A
#define ST7789_MADCTL  0x36
#define ST7789_INVON   0x21
#define ST7789_NORON   0x13
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C

#define SPI_SPEED_HZ 10000000

#define RGB565_RED   0xF800
#define RGB565_GREEN 0x07E0
#define RGB565_BLUE  0x001F

static void fatal_if_failed( rtems_status_code sc, const char *what )
{
  if ( sc != RTEMS_SUCCESSFUL ) {
    printf( "%s failed: %s\n", what, rtems_status_text( sc ) );
    exit( 1 );
  }
}

static void spi_xfer( int fd, const uint8_t *tx, size_t len )
{
  spi_ioc_transfer msg;

  memset( &msg, 0, sizeof( msg ) );
  msg.tx_buf = tx;
  msg.len = len;
  msg.speed_hz = SPI_SPEED_HZ;
  msg.bits_per_word = 8;
  msg.mode = SPI_MODE_0;

  if ( ioctl( fd, SPI_IOC_MESSAGE( 1 ), &msg ) < 0 ) {
    perror( "SPI_IOC_MESSAGE" );
    exit( 1 );
  }
}

static void st7789_write_cmd( int fd, uint8_t cmd )
{
  rtems_gpio_clear( DC_PIN );
  spi_xfer( fd, &cmd, 1 );
}

static void st7789_write_data( int fd, const uint8_t *data, size_t len )
{
  rtems_gpio_set( DC_PIN );
  spi_xfer( fd, data, len );
}

static void st7789_reset( void )
{
  rtems_gpio_set( RST_PIN );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 10 ) );
  rtems_gpio_clear( RST_PIN );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 10 ) );
  rtems_gpio_set( RST_PIN );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 120 ) );
}

static void st7789_init( int fd )
{
  uint8_t colmod = 0x55; /* 16 bits/pixel, RGB565 */
  uint8_t madctl = 0x00;

  st7789_write_cmd( fd, ST7789_SWRESET );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 150 ) );

  st7789_write_cmd( fd, ST7789_SLPOUT );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 120 ) );

  st7789_write_cmd( fd, ST7789_COLMOD );
  st7789_write_data( fd, &colmod, 1 );

  st7789_write_cmd( fd, ST7789_MADCTL );
  st7789_write_data( fd, &madctl, 1 );

  st7789_write_cmd( fd, ST7789_INVON );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 10 ) );

  st7789_write_cmd( fd, ST7789_NORON );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 10 ) );

  st7789_write_cmd( fd, ST7789_DISPON );
  rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 100 ) );
}

static void st7789_set_window(
  int      fd,
  uint16_t x0,
  uint16_t y0,
  uint16_t x1,
  uint16_t y1
)
{
  uint8_t caset[ 4 ] = { x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff };
  uint8_t raset[ 4 ] = { y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff };

  st7789_write_cmd( fd, ST7789_CASET );
  st7789_write_data( fd, caset, sizeof( caset ) );

  st7789_write_cmd( fd, ST7789_RASET );
  st7789_write_data( fd, raset, sizeof( raset ) );

  st7789_write_cmd( fd, ST7789_RAMWR );
}

static void st7789_fill_screen( int fd, uint16_t color )
{
  uint8_t row[ ST7789_WIDTH * 2 ];
  size_t  i;
  size_t  y;

  for ( i = 0; i < ST7789_WIDTH; ++i ) {
    row[ 2 * i ] = (uint8_t) ( color >> 8 );
    row[ 2 * i + 1 ] = (uint8_t) ( color & 0xff );
  }

  st7789_set_window(
    fd,
    ST7789_COL_OFFSET,
    ST7789_ROW_OFFSET,
    ST7789_COL_OFFSET + ST7789_WIDTH - 1,
    ST7789_ROW_OFFSET + ST7789_HEIGHT - 1
  );

  for ( y = 0; y < ST7789_HEIGHT; ++y ) {
    st7789_write_data( fd, row, sizeof( row ) );
  }
}

rtems_task Init( rtems_task_argument ignored )
{
  rtems_status_code sc;
  int                fd;
  int                rv;

  (void) ignored;

  sc = rtems_gpio_initialize();
  fatal_if_failed( sc, "rtems_gpio_initialize" );

  sc = rtems_gpio_request_pin( RST_PIN, DIGITAL_OUTPUT, false, false, NULL );
  fatal_if_failed( sc, "rtems_gpio_request_pin(RST)" );

  sc = rtems_gpio_request_pin( DC_PIN, DIGITAL_OUTPUT, false, false, NULL );
  fatal_if_failed( sc, "rtems_gpio_request_pin(DC)" );

  st7789_reset();

  rv = spi_bus_register_esp32c3( "/dev/spi0", SCLK_PIN, MOSI_PIN, MISO_PIN, CS_PIN );
  if ( rv != 0 ) {
    printf( "spi_bus_register_esp32c3 failed: %d\n", rv );
    exit( 1 );
  }

  fd = open( "/dev/spi0", O_RDWR );
  if ( fd < 0 ) {
    perror( "open(/dev/spi0)" );
    exit( 1 );
  }

  st7789_init( fd );

  while ( true ) {
    st7789_fill_screen( fd, RGB565_RED );
    rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 1000 ) );

    st7789_fill_screen( fd, RGB565_GREEN );
    rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 1000 ) );

    st7789_fill_screen( fd, RGB565_BLUE );
    rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 1000 ) );
  }
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

#define CONFIGURE_MAXIMUM_TASKS 1
/* See ../gpio_led_blink/init.c's comment - rtems_gpio_initialize() needs
 * one semaphore for its internal bank lock. */
#define CONFIGURE_MAXIMUM_SEMAPHORES 1
/* stdin/stdout/stderr (3) plus /dev/spi0. */
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 4

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT

#include <rtems/confdefs.h>
