/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief Polled Termios driver for the ESP32-C3's UART1 (esp32c3db BSP).
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
 * upstream-uart1-driver/README.md (two directories up from this file's
 * eventual bsps/riscv/esp32/console/ home in the RTEMS tree) for what is
 * and isn't verified, and for the spec/build YAML changes this needs.
 *
 * Unlike the GPIO/SPI/I2C drivers, this one *does* implement an
 * established RTEMS framework: Termios device drivers
 * (rtems_termios_device_install(), <rtems/termiosdevice.h>) - the same
 * mechanism used throughout RTEMS for real (non-console-shortcut) UARTs.
 * This BSP's own console driver (console-config.c) does not go through
 * this framework - it uses the simpler console-polled shortcut instead,
 * appropriate for a single fixed boot console but not reusable here for a
 * second, independently open()-able serial port.
 *
 * Scope of this first draft:
 *  - TERMIOS_POLLED mode only - no interrupt handler installed, despite
 *    UART1_INTR already being defined in this BSP's chip_definitions.h.
 *  - Honors baud rate, data bits (5/6/7/8), one or two stop bits, and
 *    parity (even/odd) from struct termios. Does not implement hardware
 *    flow control (RTS/CTS), break signaling, or IrDA mode, all of which
 *    UART_CONF0_REG/UART_CONF1_REG/UART_FLOW_CONF_REG support in
 *    principle.
 *  - last_close and ioctl are left NULL (no per-device cleanup needed,
 *    and no driver-specific ioctls defined) - not independently confirmed
 *    that the Termios core tolerates a NULL last_close specifically, only
 *    inferred from other minimal reference drivers following the same
 *    pattern.
 */

#include <bsp.h>
#include <bsp/utility.h>
#include <c3/uart1-regs.h>
#include <c3/uart1.h>
#include <c3/gpio-regs.h>

#include <rtems/termiostypes.h>

static rtems_termios_device_context esp32c3_uart1_context;

static void esp32c3_uart1_route_output( uint32_t gpio, uint32_t signal )
{
  uint32_t reg;

  reg = IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) );
  reg &= ~IO_MUX_MCU_SEL( 0x7 );
  reg |= IO_MUX_MCU_SEL( IO_MUX_FUNCTION_GPIO );
  IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) ) = reg;

  GPIO_REG( GPIO_ENABLE_W1TS_REG ) = BSP_BIT32( gpio );
  GPIO_REG( GPIO_FUNC_OUT_SEL_CFG_REG( gpio ) ) =
    GPIO_FUNC_OUT_SEL( signal ) | GPIO_FUNC_OEN_SEL;
}

static void esp32c3_uart1_route_input( uint32_t gpio, uint32_t signal )
{
  uint32_t reg;

  reg = IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) );
  reg &= ~IO_MUX_MCU_SEL( 0x7 );
  reg |= IO_MUX_MCU_SEL( IO_MUX_FUNCTION_GPIO );
  reg |= IO_MUX_FUN_IE;
  IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) ) = reg;

  GPIO_REG( GPIO_ENABLE_W1TC_REG ) = BSP_BIT32( gpio );
  GPIO_REG( GPIO_FUNC_IN_SEL_CFG_REG( signal ) ) =
    GPIO_FUNC_IN_SEL( gpio ) | GPIO_SIG_IN_SEL;
}

static bool esp32c3_uart1_configure(
  rtems_termios_device_context *context,
  const struct termios          *term
)
{
  rtems_termios_baud_t baud;
  uint64_t              divider_x16;
  uint32_t              clkdiv_int;
  uint32_t              clkdiv_frag;
  uint32_t              conf0;

  (void) context;

  baud = rtems_termios_baud_to_number( cfgetospeed( term ) );
  if ( baud == 0 ) {
    return false;
  }

  /* CLKDIV_REG's integer part plus a 1/16th-resolution fractional part:
   * divider_x16 = round(APB_CLK * 16 / baud). */
  divider_x16 =
    ( (uint64_t) UART1_APB_CLK_HZ * 16 + baud / 2 ) / baud;
  clkdiv_int = (uint32_t) ( divider_x16 / 16 );
  clkdiv_frag = (uint32_t) ( divider_x16 % 16 );

  if ( clkdiv_int == 0 || clkdiv_int > 0xfff ) {
    return false;
  }

  UART1_REG( UART_CLKDIV_REG ) =
    UART_CLKDIV_INT( clkdiv_int ) | UART_CLKDIV_FRAG( clkdiv_frag );

  conf0 = 0;

  switch ( term->c_cflag & CSIZE ) {
    case CS5:
      conf0 |= UART_BIT_NUM( UART_BIT_NUM_5 );
      break;
    case CS6:
      conf0 |= UART_BIT_NUM( UART_BIT_NUM_6 );
      break;
    case CS7:
      conf0 |= UART_BIT_NUM( UART_BIT_NUM_7 );
      break;
    case CS8:
    default:
      conf0 |= UART_BIT_NUM( UART_BIT_NUM_8 );
      break;
  }

  if ( ( term->c_cflag & PARENB ) != 0 ) {
    conf0 |= UART_PARITY_EN;

    if ( ( term->c_cflag & PARODD ) != 0 ) {
      conf0 |= UART_PARITY_ODD;
    }
  }

  conf0 |= UART_STOP_BIT_NUM(
    ( term->c_cflag & CSTOPB ) != 0 ? UART_STOP_BIT_2 : UART_STOP_BIT_1
  );

  UART1_REG( UART_CONF0_REG ) = conf0;

  return true;
}

static bool esp32c3_uart1_first_open(
  rtems_termios_tty                  *tty,
  rtems_termios_device_context       *context,
  struct termios                     *term,
  struct rtems_libio_open_close_args *args
)
{
  (void) tty;
  (void) args;

  UART1_REG( UART_CONF0_REG ) |= UART_TXFIFO_RST | UART_RXFIFO_RST;
  UART1_REG( UART_CONF0_REG ) &= ~( UART_TXFIFO_RST | UART_RXFIFO_RST );

  return esp32c3_uart1_configure( context, term );
}

static int esp32c3_uart1_poll_read( rtems_termios_device_context *context )
{
  (void) context;

  if ( UART_RXFIFO_CNT_GET( UART1_REG( UART_STATUS_REG ) ) == 0 ) {
    return -1;
  }

  return (int) ( UART1_REG( UART_FIFO_REG ) & 0xff );
}

static void esp32c3_uart1_write(
  rtems_termios_device_context *context,
  const char                   *buf,
  size_t                         len
)
{
  size_t i;

  (void) context;

  for ( i = 0; i < len; ++i ) {
    while (
      UART_TXFIFO_CNT_GET( UART1_REG( UART_STATUS_REG ) ) >= UART_FIFO_DEPTH
    ) {
      ;
    }

    UART1_REG( UART_FIFO_REG ) = (uint32_t) (unsigned char) buf[ i ];
  }
}

static bool esp32c3_uart1_set_attributes(
  rtems_termios_device_context *context,
  const struct termios          *term
)
{
  return esp32c3_uart1_configure( context, term );
}

static const rtems_termios_device_handler esp32c3_uart1_handler = {
  .first_open = esp32c3_uart1_first_open,
  .last_close = NULL,
  .poll_read = esp32c3_uart1_poll_read,
  .write = esp32c3_uart1_write,
  .set_attributes = esp32c3_uart1_set_attributes,
  .ioctl = NULL,
  .mode = TERMIOS_POLLED
};

rtems_status_code esp32c3_uart1_install(
  const char *device_file,
  uint32_t    tx_pin,
  uint32_t    rx_pin
)
{
  rtems_termios_device_context_initialize( &esp32c3_uart1_context, "ESP32C3 UART1" );

  esp32c3_uart1_route_output( tx_pin, U1TXD_OUT_IDX );
  esp32c3_uart1_route_input( rx_pin, U1RXD_IN_IDX );

  return rtems_termios_device_install(
    device_file,
    &esp32c3_uart1_handler,
    NULL,
    &esp32c3_uart1_context
  );
}
