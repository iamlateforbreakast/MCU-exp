/*
 * Minimal RTEMS "hello world" for the ESP32-C3 (esp32c3db BSP).
 *
 * Deliberately console-only: the esp32c3db BSP currently provides a
 * clock driver (SYSTIMER) and console driver (UART0/USB-Serial) but no GPIO
 * driver, so an LED-blink example isn't possible yet without poking raw
 * ROM/register addresses. This exercises the two drivers the BSP actually
 * has, following the standard RTEMS init.c pattern used elsewhere in this
 * repo (see rtems_ubuntu_build.md).
 */

#include <rtems.h>
#include <stdio.h>
#include <stdlib.h>

rtems_task Init(rtems_task_argument ignored)
{
  printf("Hello World from RTEMS on the ESP32-C3 (esp32c3db)!\n");
  exit(0);
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

#define CONFIGURE_MAXIMUM_TASKS 1

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT

#include <rtems/confdefs.h>
