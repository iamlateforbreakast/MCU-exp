/*
 * Scans the I2C bus for responding devices on the ESP32-C3, via the draft
 * I2C_EXT0 driver in ../../upstream-i2c-driver/. Probes every address in
 * the conventional 7-bit scan range (0x03-0x77) with a zero-length read
 * and reports which ones ACK - this validates RESTART/address/ACK-
 * checking/STOP sequencing without needing any specific device or its
 * data protocol to be right (see that driver's README "Testing plan"
 * step 3), and needs nothing wired beyond SCL/SDA themselves, since the
 * driver enables its own weak internal pull-ups.
 *
 * Wiring: SCL -> GPIO0, SDA -> GPIO1 (deliberately not GPIO4-7/10, which
 * other examples in this repo use for SPI, so this can run without
 * rewiring anything already on the breadboard). Attach a real I2C
 * device's SCL/SDA to the same two pins to see it show up in the scan.
 *
 * STATUS: builds against real hardware once the I2C driver is integrated
 * per ../../upstream-i2c-driver/README.md's "Integration steps" (same
 * not-persisted-in-the-image caveat as the GPIO/SPI drivers - see
 * ../../ESP32-C3-RTEMS.md) - confirmed 2026-08-23, but it hangs forever on
 * the very first address probe and never proceeds. The I2C driver itself
 * deadlocks in an unbounded poll loop on every transfer (see that
 * README's status section for what's been found and ruled out) - this is
 * downstream of that bug, not a problem in this file.
 */

#include <rtems.h>
#include <bsp/esp32c3-i2c.h>
#include <dev/i2c/i2c.h>
#include <linux/i2c-dev.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>

#define SCL_PIN 0
#define SDA_PIN 1

rtems_task Init( rtems_task_argument ignored )
{
  int      rv;
  int      fd;
  uint16_t addr;
  int      found = 0;

  (void) ignored;

  rv = i2c_bus_register_esp32c3( "/dev/i2c-0", SCL_PIN, SDA_PIN );
  if ( rv != 0 ) {
    printf( "i2c_bus_register_esp32c3 failed: %d\n", rv );
    exit( 1 );
  }

  fd = open( "/dev/i2c-0", O_RDWR );
  if ( fd < 0 ) {
    perror( "open(/dev/i2c-0)" );
    exit( 1 );
  }

  printf( "Scanning I2C bus (0x03-0x77)...\n" );

  for ( addr = 0x03; addr <= 0x77; ++addr ) {
    struct i2c_msg           msg;
    struct i2c_rdwr_ioctl_data data;

    msg.addr = addr;
    msg.flags = I2C_M_RD;
    msg.len = 0;
    msg.buf = NULL;

    data.msgs = &msg;
    data.nmsgs = 1;

    if ( ioctl( fd, I2C_RDWR, &data ) >= 0 ) {
      printf( "  found device at 0x%02x\n", addr );
      ++found;
    }
  }

  printf( "Scan complete, %d device(s) found.\n", found );

  exit( 0 );
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

#define CONFIGURE_MAXIMUM_TASKS 1
/* rtems_gpio_initialize() isn't used here, but i2c-bus.c's own internal
 * locking has the same self-contained-mutex vs. classic-semaphore
 * question the GPIO framework hit - see ../gpio_led_blink/init.c's
 * comment. Reserved defensively; harmless if unneeded. */
#define CONFIGURE_MAXIMUM_SEMAPHORES 1
/* stdin/stdout/stderr (3) plus /dev/i2c-0. */
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 4

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT

#include <rtems/confdefs.h>
