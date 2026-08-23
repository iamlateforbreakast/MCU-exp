/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief GPIO driver for the ESP32-C3 (esp32c3db BSP), implementing the
 *   rtems_gpio_bsp_* callbacks required by the generic RTEMS GPIO API
 *   (bsps/include/bsp/gpio.h, bsps/shared/dev/gpio/gpio-support.c).
 */

/*
 * Copyright (C) 2026 Thomas Remion
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * STATUS: draft - not yet built or tested against real hardware. See
 * upstream-gpio-driver/README.md (two directories up from this file's
 * eventual bsps/riscv/esp32/gpio/ home in the RTEMS tree) for what is and
 * isn't verified, and for the spec/build YAML changes this needs to be
 * picked up by the esp32c3db BSP build.
 *
 * Alternate/peripheral pin routing through the GPIO matrix
 * (GPIO_FUNCn_IN/OUT_SEL_CFG_REG) is out of scope for this draft -
 * rtems_gpio_bsp_select_specific_io() below always fails. Only plain
 * digital input/output and edge/level interrupts are implemented.
 */

#include <bsp.h>
#include <bsp/irq.h>
#include <bsp/utility.h>
#include <c3/chip_definitions.h>
#include <c3/gpio-regs.h>
#include <bsp/gpio.h>

static void esp32c3_gpio_select_function( uint32_t pin )
{
  uint32_t reg = IO_MUX_REG( IO_MUX_GPIO_REG( pin ) );

  reg &= ~IO_MUX_MCU_SEL( 0x7 );
  reg |= IO_MUX_MCU_SEL( IO_MUX_FUNCTION_GPIO );
  IO_MUX_REG( IO_MUX_GPIO_REG( pin ) ) = reg;
}

rtems_status_code rtems_gpio_bsp_multi_set( uint32_t bank, uint32_t bitmask )
{
  (void) bank;

  GPIO_REG( GPIO_OUT_W1TS_REG ) = bitmask;
  return RTEMS_SUCCESSFUL;
}

rtems_status_code rtems_gpio_bsp_multi_clear(
  uint32_t bank,
  uint32_t bitmask
)
{
  (void) bank;

  GPIO_REG( GPIO_OUT_W1TC_REG ) = bitmask;
  return RTEMS_SUCCESSFUL;
}

uint32_t rtems_gpio_bsp_multi_read( uint32_t bank, uint32_t bitmask )
{
  (void) bank;

  return GPIO_REG( GPIO_IN_REG ) & bitmask;
}

rtems_status_code rtems_gpio_bsp_specific_group_operation(
  uint32_t bank,
  uint32_t *pins,
  uint32_t pin_count,
  void *arg
)
{
  (void) bank;
  (void) pins;
  (void) pin_count;
  (void) arg;

  return RTEMS_NOT_DEFINED;
}

rtems_status_code rtems_gpio_bsp_multi_select(
  rtems_gpio_multiple_pin_select *pins,
  uint32_t pin_count,
  uint32_t select_bank
)
{
  (void) pins;
  (void) pin_count;
  (void) select_bank;

  return RTEMS_NOT_DEFINED;
}

rtems_status_code rtems_gpio_bsp_set( uint32_t bank, uint32_t pin )
{
  (void) bank;

  GPIO_REG( GPIO_OUT_W1TS_REG ) = BSP_BIT32( pin );
  return RTEMS_SUCCESSFUL;
}

rtems_status_code rtems_gpio_bsp_clear( uint32_t bank, uint32_t pin )
{
  (void) bank;

  GPIO_REG( GPIO_OUT_W1TC_REG ) = BSP_BIT32( pin );
  return RTEMS_SUCCESSFUL;
}

uint32_t rtems_gpio_bsp_get_value( uint32_t bank, uint32_t pin )
{
  (void) bank;

  return ( GPIO_REG( GPIO_IN_REG ) & BSP_BIT32( pin ) ) != 0;
}

rtems_status_code rtems_gpio_bsp_select_input(
  uint32_t bank,
  uint32_t pin,
  void *bsp_specific
)
{
  (void) bank;
  (void) bsp_specific;

  esp32c3_gpio_select_function( pin );

  /* Direction: clear the enable bit to make the pin an input. */
  GPIO_REG( GPIO_ENABLE_W1TC_REG ) = BSP_BIT32( pin );

  IO_MUX_REG( IO_MUX_GPIO_REG( pin ) ) |= IO_MUX_FUN_IE;
  return RTEMS_SUCCESSFUL;
}

rtems_status_code rtems_gpio_bsp_select_output(
  uint32_t bank,
  uint32_t pin,
  void *bsp_specific
)
{
  (void) bank;
  (void) bsp_specific;

  esp32c3_gpio_select_function( pin );

  /* Direction: set the enable bit to make the pin an output. */
  GPIO_REG( GPIO_ENABLE_W1TS_REG ) = BSP_BIT32( pin );
  return RTEMS_SUCCESSFUL;
}

rtems_status_code rtems_gpio_bsp_select_specific_io(
  uint32_t bank,
  uint32_t pin,
  uint32_t function,
  void *pin_data
)
{
  (void) bank;
  (void) pin;
  (void) function;
  (void) pin_data;

  return RTEMS_UNSATISFIED;
}

rtems_status_code rtems_gpio_bsp_set_resistor_mode(
  uint32_t bank,
  uint32_t pin,
  rtems_gpio_pull_mode mode
)
{
  uint32_t reg;

  (void) bank;

  reg = IO_MUX_REG( IO_MUX_GPIO_REG( pin ) );
  reg &= ~( IO_MUX_FUN_WPU | IO_MUX_FUN_WPD );

  switch ( mode ) {
    case PULL_UP:
      reg |= IO_MUX_FUN_WPU;
      break;
    case PULL_DOWN:
      reg |= IO_MUX_FUN_WPD;
      break;
    case NO_PULL_RESISTOR:
      break;
    default:
      return RTEMS_UNSATISFIED;
  }

  IO_MUX_REG( IO_MUX_GPIO_REG( pin ) ) = reg;
  return RTEMS_SUCCESSFUL;
}

uint32_t rtems_gpio_bsp_interrupt_line( rtems_vector_number vector )
{
  uint32_t status;

  (void) vector;

  /* Single bank, so the vector always maps back to the one status word. */
  status = GPIO_REG( GPIO_STATUS_REG );
  GPIO_REG( GPIO_STATUS_W1TC_REG ) = status;
  return status;
}

rtems_vector_number rtems_gpio_bsp_get_vector( uint32_t bank )
{
  (void) bank;

  return (rtems_vector_number) GPIO_PROCPU_INTR;
}

rtems_status_code rtems_gpio_bsp_enable_interrupt(
  uint32_t bank,
  uint32_t pin,
  rtems_gpio_interrupt interrupt
)
{
  uint32_t int_type;
  uint32_t reg;

  (void) bank;

  switch ( interrupt ) {
    case RISING_EDGE:
      int_type = GPIO_PIN_INT_TYPE_RISING;
      break;
    case FALLING_EDGE:
      int_type = GPIO_PIN_INT_TYPE_FALLING;
      break;
    case BOTH_EDGES:
      int_type = GPIO_PIN_INT_TYPE_ANY_EDGE;
      break;
    case LOW_LEVEL:
      int_type = GPIO_PIN_INT_TYPE_LOW_LEVEL;
      break;
    case HIGH_LEVEL:
      int_type = GPIO_PIN_INT_TYPE_HIGH_LEVEL;
      break;
    default:
      /*
       * NONE and BOTH_LEVELS have no matching hardware trigger mode on
       * this GPIO controller.
       */
      return RTEMS_UNSATISFIED;
  }

  reg = GPIO_REG( GPIO_PIN_REG( pin ) );
  reg = ( reg & ~GPIO_PIN_INT_TYPE_MASK ) | GPIO_PIN_INT_TYPE( int_type );
  GPIO_REG( GPIO_PIN_REG( pin ) ) = reg;
  return RTEMS_SUCCESSFUL;
}

rtems_status_code rtems_gpio_bsp_disable_interrupt(
  uint32_t bank,
  uint32_t pin,
  rtems_gpio_interrupt active_interrupt
)
{
  uint32_t reg;

  (void) bank;
  (void) active_interrupt;

  reg = GPIO_REG( GPIO_PIN_REG( pin ) );
  reg &= ~GPIO_PIN_INT_TYPE_MASK;
  GPIO_REG( GPIO_PIN_REG( pin ) ) = reg;
  return RTEMS_SUCCESSFUL;
}
