/*
 * Blinks three LEDs on the ESP32-C3, one after another, using RTEMS's
 * generic GPIO API (bsp/gpio.h - rtems_gpio_initialize/request_pin/set/
 * clear) against the register-level GPIO driver drafted in
 * ../../upstream-gpio-driver/bsps/riscv/esp32/gpio/gpio.c.
 *
 * STATUS: builds and links cleanly once the GPIO driver it depends on is
 * integrated into the BSP per ../../upstream-gpio-driver/README.md's
 * "Integration steps" (not persisted in the esp32c3-rtems-dev image yet -
 * see that README and ../../ESP32-C3-RTEMS.md). Not yet run against real
 * hardware.
 *
 * Adjust LED_GPIO_0/1/2 below to match how LEDs are actually wired.
 * GPIO4/5/6 are placeholders - deliberately not the ESP32-C3's strapping
 * pins (GPIO2/8/9), SPI0/SPI1 flash pins (GPIO11-17), or GPIO18-21 (which
 * the console may be using for USB-Serial-JTAG or UART0 depending on
 * build config) - see the GPIO driver's own gpio-regs.h for that pin
 * list. Also matches the same placeholder choice already used in
 * ../../../ESP32-C3/examples/gpio_led_blink, the ESP-IDF version of this
 * same example for the same chip.
 */

#include <rtems.h>
#include <bsp/gpio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define LED_GPIO_0 4
#define LED_GPIO_1 5
#define LED_GPIO_2 6

static const uint32_t leds[] = { LED_GPIO_0, LED_GPIO_1, LED_GPIO_2 };

#define NUM_LEDS ( sizeof( leds ) / sizeof( leds[ 0 ] ) )

static void fatal_if_failed( rtems_status_code sc, const char *what )
{
  if ( sc != RTEMS_SUCCESSFUL ) {
    printf( "%s failed: %s\n", what, rtems_status_text( sc ) );
    exit( 1 );
  }
}

rtems_task Init( rtems_task_argument ignored )
{
  size_t             i;
  size_t             active;
  rtems_status_code  sc;

  (void) ignored;

  sc = rtems_gpio_initialize();
  fatal_if_failed( sc, "rtems_gpio_initialize" );

  for ( i = 0; i < NUM_LEDS; ++i ) {
    sc = rtems_gpio_request_pin( leds[ i ], DIGITAL_OUTPUT, false, false, NULL );
    fatal_if_failed( sc, "rtems_gpio_request_pin" );

    sc = rtems_gpio_clear( leds[ i ] );
    fatal_if_failed( sc, "rtems_gpio_clear" );
  }

  active = 0;

  while ( true ) {
    for ( i = 0; i < NUM_LEDS; ++i ) {
      sc = ( i == active )
        ? rtems_gpio_set( leds[ i ] )
        : rtems_gpio_clear( leds[ i ] );
      fatal_if_failed( sc, "rtems_gpio_set/clear" );
    }

    active = ( active + 1 ) % NUM_LEDS;

    rtems_task_wake_after( RTEMS_MILLISECONDS_TO_TICKS( 300 ) );
  }
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

#define CONFIGURE_MAXIMUM_TASKS 1

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT

#include <rtems/confdefs.h>
