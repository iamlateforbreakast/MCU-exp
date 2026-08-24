/*
 * Drives a 1.14" 135x240 ST7789 IPS SPI TFT (the "AITEXM ... ST7789 LCD
 * Board SPI" module from AliExpress - same panel as
 * ../../ESP32-C3-RTEMS/examples/lcd_st7789_spi/, which this port is based
 * on) from Linux userspace on the Luckfox Pico Mini B, via /dev/spidev0.0
 * plus sysfs GPIO for DC/RESET/BL.
 *
 * Requires spi0 enabled in the board dts (status = "okay" on &spi0, with
 * fbtft@0 disabled so spidev@0 owns the bus) - see
 * ../../luckfoxpic.md / the luckfox_pico_board_access memory for the
 * kernel rebuild + reflash steps that were needed to get /dev/spidev0.0
 * to exist at all.
 *
 * Wiring - SPI0 M0 pins are fixed by the SoC pinmux (CLK/MOSI/CS0 on
 * GPIO1_C1/C2/C0). RESET and DC deliberately reuse GPIO1_C7 and GPIO1_D0
 * (sysfs 55/56) rather than the vendor reference wiring's GPIO1_A2/C3 -
 * those two are already proven physically present on this board's header
 * (they're the same pins ../gpio_led_blink/ drove as LED1/LED2), whereas
 * the header's physical pin numbers for GPIO1_A2/C3 were never confirmed.
 * BLK is wired straight to 3V3 (always-on backlight) instead of a GPIO -
 * this module doesn't need software brightness control for a basic test.
 *
 * Physical header pin numbers (confirmed against this board's own pinout
 * reference, not just the SoC pin name):
 *   VCC -> PIN3 (3V3)       GND -> PIN2
 *   SCL -> PIN7  (SPI0_CLK_M0)
 *   SDA -> PIN8  (SPI0_MOSI_M0)
 *   CS  -> PIN6  (SPI0_CS0_M0)
 *   RES -> PIN17 (GPIO1_C7, sysfs 55)
 *   DC  -> PIN12 (GPIO1_D0, sysfs 56)
 *   BLK -> PIN3  (3V3, same as VCC - always on)
 */

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define GPIO_DC  56
#define GPIO_RST 55

#define SPI_DEV "/dev/spidev0.0"
#define SPI_SPEED_HZ 10000000

#define ST7789_WIDTH  135
#define ST7789_HEIGHT 240
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

#define RGB565_RED   0xF800
#define RGB565_GREEN 0x07E0
#define RGB565_BLUE  0x001F

static volatile sig_atomic_t g_stop;
static int g_spi_fd = -1;

static void sleep_ms( long ms )
{
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = ( ms % 1000 ) * 1000000L;
  nanosleep( &ts, NULL );
}

static void gpio_export( int pin )
{
  char path[ 64 ];
  int  fd;

  snprintf( path, sizeof( path ), "/sys/class/gpio/gpio%d", pin );
  if ( access( path, F_OK ) == 0 ) {
    return;
  }

  fd = open( "/sys/class/gpio/export", O_WRONLY );
  if ( fd < 0 ) {
    perror( "open(export)" );
    exit( 1 );
  }

  char buf[ 16 ];
  int  len = snprintf( buf, sizeof( buf ), "%d", pin );
  if ( write( fd, buf, len ) < 0 ) {
    perror( "write(export)" );
    exit( 1 );
  }
  close( fd );
}

static void gpio_direction_out( int pin )
{
  char path[ 64 ];
  int  fd;

  snprintf( path, sizeof( path ), "/sys/class/gpio/gpio%d/direction", pin );
  fd = open( path, O_WRONLY );
  if ( fd < 0 ) {
    perror( "open(direction)" );
    exit( 1 );
  }
  if ( write( fd, "out", 3 ) < 0 ) {
    perror( "write(direction)" );
    exit( 1 );
  }
  close( fd );
}

static void gpio_write( int pin, int value )
{
  char path[ 64 ];
  int  fd;

  snprintf( path, sizeof( path ), "/sys/class/gpio/gpio%d/value", pin );
  fd = open( path, O_WRONLY );
  if ( fd < 0 ) {
    perror( "open(value)" );
    exit( 1 );
  }
  if ( write( fd, value ? "1" : "0", 1 ) < 0 ) {
    perror( "write(value)" );
    exit( 1 );
  }
  close( fd );
}

static void gpio_unexport( int pin )
{
  char path[ 64 ];
  int  fd;

  snprintf( path, sizeof( path ), "/sys/class/gpio/gpio%d", pin );
  if ( access( path, F_OK ) != 0 ) {
    return;
  }

  fd = open( "/sys/class/gpio/unexport", O_WRONLY );
  if ( fd < 0 ) {
    return;
  }
  char buf[ 16 ];
  int  len = snprintf( buf, sizeof( buf ), "%d", pin );
  write( fd, buf, len );
  close( fd );
}

static void spi_xfer( const uint8_t *tx, size_t len )
{
  struct spi_ioc_transfer msg;

  memset( &msg, 0, sizeof( msg ) );
  msg.tx_buf = (unsigned long) tx;
  msg.len = len;
  msg.speed_hz = SPI_SPEED_HZ;
  msg.bits_per_word = 8;

  if ( ioctl( g_spi_fd, SPI_IOC_MESSAGE( 1 ), &msg ) < 0 ) {
    perror( "SPI_IOC_MESSAGE" );
    exit( 1 );
  }
}

static void st7789_write_cmd( uint8_t cmd )
{
  gpio_write( GPIO_DC, 0 );
  spi_xfer( &cmd, 1 );
}

static void st7789_write_data( const uint8_t *data, size_t len )
{
  gpio_write( GPIO_DC, 1 );
  spi_xfer( data, len );
}

static void st7789_reset( void )
{
  gpio_write( GPIO_RST, 1 );
  sleep_ms( 10 );
  gpio_write( GPIO_RST, 0 );
  sleep_ms( 10 );
  gpio_write( GPIO_RST, 1 );
  sleep_ms( 120 );
}

static void st7789_init( void )
{
  uint8_t colmod = 0x55; /* 16 bits/pixel, RGB565 */
  uint8_t madctl = 0x00;

  st7789_write_cmd( ST7789_SWRESET );
  sleep_ms( 150 );

  st7789_write_cmd( ST7789_SLPOUT );
  sleep_ms( 120 );

  st7789_write_cmd( ST7789_COLMOD );
  st7789_write_data( &colmod, 1 );

  st7789_write_cmd( ST7789_MADCTL );
  st7789_write_data( &madctl, 1 );

  st7789_write_cmd( ST7789_INVON );
  sleep_ms( 10 );

  st7789_write_cmd( ST7789_NORON );
  sleep_ms( 10 );

  st7789_write_cmd( ST7789_DISPON );
  sleep_ms( 100 );
}

static void st7789_set_window( uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1 )
{
  uint8_t caset[ 4 ] = { x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff };
  uint8_t raset[ 4 ] = { y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff };

  st7789_write_cmd( ST7789_CASET );
  st7789_write_data( caset, sizeof( caset ) );

  st7789_write_cmd( ST7789_RASET );
  st7789_write_data( raset, sizeof( raset ) );

  st7789_write_cmd( ST7789_RAMWR );
}

static void st7789_fill_screen( uint16_t color )
{
  uint8_t row[ ST7789_WIDTH * 2 ];
  size_t  i;
  size_t  y;

  for ( i = 0; i < ST7789_WIDTH; ++i ) {
    row[ 2 * i ] = (uint8_t) ( color >> 8 );
    row[ 2 * i + 1 ] = (uint8_t) ( color & 0xff );
  }

  st7789_set_window(
    ST7789_COL_OFFSET,
    ST7789_ROW_OFFSET,
    ST7789_COL_OFFSET + ST7789_WIDTH - 1,
    ST7789_ROW_OFFSET + ST7789_HEIGHT - 1
  );

  for ( y = 0; y < ST7789_HEIGHT; ++y ) {
    st7789_write_data( row, sizeof( row ) );
  }
}

static void handle_signal( int sig )
{
  (void) sig;
  g_stop = 1;
}

static void cleanup( void )
{
  gpio_unexport( GPIO_DC );
  gpio_unexport( GPIO_RST );
  if ( g_spi_fd >= 0 ) {
    close( g_spi_fd );
  }
}

int main( void )
{
  uint8_t mode = SPI_MODE_0;
  uint8_t bits = 8;
  uint32_t speed = SPI_SPEED_HZ;

  signal( SIGINT, handle_signal );
  signal( SIGTERM, handle_signal );

  gpio_export( GPIO_DC );
  gpio_export( GPIO_RST );
  gpio_direction_out( GPIO_DC );
  gpio_direction_out( GPIO_RST );

  g_spi_fd = open( SPI_DEV, O_RDWR );
  if ( g_spi_fd < 0 ) {
    perror( "open(" SPI_DEV ")" );
    return 1;
  }

  if ( ioctl( g_spi_fd, SPI_IOC_WR_MODE, &mode ) < 0 ) {
    perror( "SPI_IOC_WR_MODE" );
    return 1;
  }
  if ( ioctl( g_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits ) < 0 ) {
    perror( "SPI_IOC_WR_BITS_PER_WORD" );
    return 1;
  }
  if ( ioctl( g_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed ) < 0 ) {
    perror( "SPI_IOC_WR_MAX_SPEED_HZ" );
    return 1;
  }

  st7789_reset();
  st7789_init();

  printf( "ST7789 initialized, cycling colors (Ctrl-C to stop)...\n" );
  fflush( stdout );

  while ( !g_stop ) {
    st7789_fill_screen( RGB565_RED );
    printf( "red\n" );
    fflush( stdout );
    for ( int i = 0; i < 10 && !g_stop; ++i ) sleep_ms( 100 );

    if ( g_stop ) break;
    st7789_fill_screen( RGB565_GREEN );
    printf( "green\n" );
    fflush( stdout );
    for ( int i = 0; i < 10 && !g_stop; ++i ) sleep_ms( 100 );

    if ( g_stop ) break;
    st7789_fill_screen( RGB565_BLUE );
    printf( "blue\n" );
    fflush( stdout );
    for ( int i = 0; i < 10 && !g_stop; ++i ) sleep_ms( 100 );
  }

  cleanup();
  return 0;
}
