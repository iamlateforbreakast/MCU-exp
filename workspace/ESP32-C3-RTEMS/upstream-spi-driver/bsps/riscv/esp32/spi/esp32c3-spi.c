/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief Polled, register-level GP-SPI2 master driver for the ESP32-C3
 *   (esp32c3db BSP), implementing RTEMS's generic SPI bus API
 *   (dev/spi/spi.h).
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
 * upstream-spi-driver/README.md (two directories up from this file's
 * eventual bsps/riscv/esp32/spi/ home in the RTEMS tree) for what is and
 * isn't verified, and for the spec/build YAML changes this needs.
 *
 * Scope of this first draft:
 *  - Master mode only, 8 bits per word, full duplex, MSB or LSB first.
 *  - Polled (CPU-buffer) transfers only - no DMA, no interrupts. Each
 *    spi_ioc_transfer is chunked into <=64-byte USR transactions (the
 *    SPI_W0_REG..SPI_W15_REG buffer is 16 x 32-bit = 64 bytes; there is no
 *    DMA path wired up in this draft to go beyond that in one shot).
 *  - Single device: chip select is a plain GPIO the caller supplies to
 *    spi_bus_register_esp32c3(), toggled manually around each transfer
 *    rather than using GP-SPI2's own hardware CS0/1/2 lines. This sidesteps
 *    the GPIO-matrix CS routing and CS_SETUP/CS_HOLD auto-timing entirely,
 *    at the cost of not supporting more than one device on the bus in this
 *    draft. spi_ioc_transfer.cs must be 0.
 */

#include <bsp.h>
#include <bsp/utility.h>
#include <bsp/esp32c3-spi.h>
#include <c3/spi-regs.h>
#include <c3/gpio-regs.h>

#include <dev/spi/spi.h>

#include <errno.h>

typedef struct {
  spi_bus  base;
  uint32_t sclk_pin;
  uint32_t mosi_pin;
  uint32_t miso_pin;
  uint32_t cs_pin;
} esp32c3_spi_bus;

#define SPI_POLL_WHILE( condition ) \
  while ( condition ) {             \
    ;                               \
  }

static void esp32c3_spi_route_output( uint32_t gpio, uint32_t signal )
{
  uint32_t reg;

  /* Digital function on the pad, direction = output. */
  reg = IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) );
  reg &= ~IO_MUX_MCU_SEL( 0x7 );
  reg |= IO_MUX_MCU_SEL( IO_MUX_FUNCTION_GPIO );
  IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) ) = reg;

  GPIO_REG( GPIO_ENABLE_W1TS_REG ) = BSP_BIT32( gpio );

  /* Let the routed signal (not GPIO_OUT_REG) drive the pad. */
  GPIO_REG( GPIO_FUNC_OUT_SEL_CFG_REG( gpio ) ) =
    GPIO_FUNC_OUT_SEL( signal ) | GPIO_FUNC_OEN_SEL;
}

static void esp32c3_spi_route_input( uint32_t gpio, uint32_t signal )
{
  uint32_t reg;

  reg = IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) );
  reg &= ~IO_MUX_MCU_SEL( 0x7 );
  reg |= IO_MUX_MCU_SEL( IO_MUX_FUNCTION_GPIO );
  IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) ) = reg;

  IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) ) |= IO_MUX_FUN_IE;
  GPIO_REG( GPIO_ENABLE_W1TC_REG ) = BSP_BIT32( gpio );

  GPIO_REG( GPIO_FUNC_IN_SEL_CFG_REG( signal ) ) =
    GPIO_FUNC_IN_SEL( gpio ) | GPIO_SIG_IN_SEL;
}

static void esp32c3_spi_cs_init( esp32c3_spi_bus *bus )
{
  uint32_t reg;

  reg = IO_MUX_REG( IO_MUX_GPIO_REG( bus->cs_pin ) );
  reg &= ~IO_MUX_MCU_SEL( 0x7 );
  reg |= IO_MUX_MCU_SEL( IO_MUX_FUNCTION_GPIO );
  IO_MUX_REG( IO_MUX_GPIO_REG( bus->cs_pin ) ) = reg;

  /* Plain GPIO output, not routed through the matrix - deselected (idle
   * high unless SPI_CS_HIGH is set in the current mode) to start. */
  GPIO_REG( GPIO_FUNC_OUT_SEL_CFG_REG( bus->cs_pin ) ) = GPIO_FUNC_OUT_SEL( 0x80 );
  GPIO_REG( GPIO_ENABLE_W1TS_REG ) = BSP_BIT32( bus->cs_pin );

  if ( ( bus->base.mode & SPI_CS_HIGH ) != 0 ) {
    GPIO_REG( GPIO_OUT_W1TC_REG ) = BSP_BIT32( bus->cs_pin );
  } else {
    GPIO_REG( GPIO_OUT_W1TS_REG ) = BSP_BIT32( bus->cs_pin );
  }
}

static void esp32c3_spi_cs_assert( esp32c3_spi_bus *bus )
{
  bool active_high = ( bus->base.mode & SPI_CS_HIGH ) != 0;

  GPIO_REG( active_high ? GPIO_OUT_W1TS_REG : GPIO_OUT_W1TC_REG ) =
    BSP_BIT32( bus->cs_pin );
}

static void esp32c3_spi_cs_deassert( esp32c3_spi_bus *bus )
{
  bool active_high = ( bus->base.mode & SPI_CS_HIGH ) != 0;

  GPIO_REG( active_high ? GPIO_OUT_W1TC_REG : GPIO_OUT_W1TS_REG ) =
    BSP_BIT32( bus->cs_pin );
}

static int esp32c3_spi_configure( esp32c3_spi_bus *bus )
{
  uint32_t clkdiv_n;
  uint32_t clkdiv_h;
  uint32_t clock;
  uint32_t ctrl;
  uint32_t user;
  uint32_t misc;

  if ( bus->base.bits_per_word != 8 ) {
    return -EINVAL;
  }

  if ( bus->base.speed_hz == 0 || bus->base.speed_hz > SPI2_MAX_FREQUENCY ) {
    return -EINVAL;
  }

  /* Only bits 0-1 (SPI_CPOL/SPI_CPHA) affect the clock mode below; other
   * mode flags (e.g. SPI_CS_HIGH, read separately by the CS helpers) are
   * intentionally not validated here - see spidev.h's SPI_* flag bits. */

  /* clkdiv_pre is left at 0 (divide by 1); clkcnt_n alone gives a
   * reasonable range (source_clk down to source_clk/64) without the extra
   * pre-divider stage. Rounds down to the next slower-or-equal rate. */
  clkdiv_n = SPI2_SOURCE_CLK_HZ / bus->base.speed_hz;
  if ( clkdiv_n < 1 ) {
    clkdiv_n = 1;
  } else if ( clkdiv_n > 64 ) {
    clkdiv_n = 64;
  }

  if ( clkdiv_n == 1 ) {
    clock = SPI_CLK_EQU_SYSCLK;
  } else {
    uint32_t n = clkdiv_n - 1;

    clkdiv_h = ( clkdiv_n / 2 );
    if ( clkdiv_h > 0 ) {
      clkdiv_h -= 1;
    }

    clock = SPI_CLKCNT_N( n ) | SPI_CLKCNT_H( clkdiv_h ) | SPI_CLKCNT_L( n );
  }

  SPI2_REG( SPI_CLOCK_REG ) = clock;

  ctrl = 0;
  if ( bus->base.lsb_first ) {
    ctrl |= SPI_WR_BIT_ORDER | SPI_RD_BIT_ORDER;
  }
  SPI2_REG( SPI_CTRL_REG ) = ctrl;

  /* Mode -> (ck_idle_edge, ck_out_edge) mapping, matching Espressif's own
   * spi_ll_master_set_mode() for this register generation:
   *   mode 0: idle=0, out_edge=0    mode 1: idle=0, out_edge=1
   *   mode 2: idle=1, out_edge=1    mode 3: idle=1, out_edge=0 */
  user = SPI_USR_MOSI | SPI_USR_MISO | SPI_DOUTDIN;
  misc = SPI_CS0_DIS | SPI_CS1_DIS | SPI_CS2_DIS; /* CS is a plain GPIO, see file header */

  switch ( bus->base.mode & SPI_MODE_3 ) {
    case SPI_MODE_0:
      break;
    case SPI_MODE_1:
      user |= SPI_CK_OUT_EDGE;
      break;
    case SPI_MODE_2:
      misc |= SPI_CK_IDLE_EDGE;
      user |= SPI_CK_OUT_EDGE;
      break;
    case SPI_MODE_3:
      misc |= SPI_CK_IDLE_EDGE;
      break;
    default:
      return -EINVAL;
  }

  SPI2_REG( SPI_USER_REG ) = user;
  SPI2_REG( SPI_MISC_REG ) = misc;

  SPI2_REG( SPI_CMD_REG ) = SPI_UPDATE;
  SPI_POLL_WHILE( ( SPI2_REG( SPI_CMD_REG ) & SPI_UPDATE ) != 0 );

  return 0;
}

static int esp32c3_spi_do_chunk(
  const uint8_t       *tx,
  uint8_t             *rx,
  size_t               n
)
{
  size_t words = ( n + 3 ) / 4;
  size_t i;

  if ( n == 0 ) {
    return 0;
  }

  if ( n > SPI2_MAX_BYTES_PER_TRANSACTION ) {
    return -EINVAL;
  }

  for ( i = 0; i < words; ++i ) {
    uint32_t word = 0;
    size_t   j;

    for ( j = 0; j < 4 && i * 4 + j < n; ++j ) {
      uint8_t byte = tx != NULL ? tx[ i * 4 + j ] : 0xff;

      word |= (uint32_t) byte << ( 8 * j );
    }

    SPI2_REG( SPI_W0_REG( i ) ) = word;
  }

  SPI2_REG( SPI_MS_DLEN_REG ) = SPI_MS_DATA_BITLEN( n * 8 - 1 );

  SPI2_REG( SPI_CMD_REG ) = SPI_UPDATE;
  SPI_POLL_WHILE( ( SPI2_REG( SPI_CMD_REG ) & SPI_UPDATE ) != 0 );

  SPI2_REG( SPI_CMD_REG ) = SPI_USR;
  SPI_POLL_WHILE( ( SPI2_REG( SPI_CMD_REG ) & SPI_USR ) != 0 );

  if ( rx != NULL ) {
    for ( i = 0; i < words; ++i ) {
      uint32_t word = SPI2_REG( SPI_W0_REG( i ) );
      size_t   j;

      for ( j = 0; j < 4 && i * 4 + j < n; ++j ) {
        rx[ i * 4 + j ] = (uint8_t) ( word >> ( 8 * j ) );
      }
    }
  }

  return 0;
}

static int esp32c3_spi_transfer(
  spi_bus                *base,
  const spi_ioc_transfer *msgs,
  uint32_t                 msg_count
)
{
  esp32c3_spi_bus *bus = (esp32c3_spi_bus *) base;
  uint32_t          i;

  for ( i = 0; i < msg_count; ++i ) {
    const spi_ioc_transfer *msg = &msgs[ i ];
    const uint8_t           *tx = msg->tx_buf;
    uint8_t                  *rx = msg->rx_buf;
    size_t                    remaining = msg->len;

    if ( msg->cs != 0 ) {
      /* Single-device draft, see file header. */
      return -EINVAL;
    }

    if (
      ( msg->bits_per_word != 0 && msg->bits_per_word != 8 )
        || ( msg->speed_hz > SPI2_MAX_FREQUENCY )
    ) {
      return -EINVAL;
    }

    if (
      ( msg->speed_hz != 0 && msg->speed_hz != bus->base.speed_hz )
        || msg->mode != bus->base.mode
    ) {
      int rv;

      bus->base.speed_hz = msg->speed_hz != 0 ? msg->speed_hz : bus->base.speed_hz;
      bus->base.mode = msg->mode;
      rv = esp32c3_spi_configure( bus );
      if ( rv != 0 ) {
        return rv;
      }
    }

    esp32c3_spi_cs_assert( bus );

    while ( remaining > 0 ) {
      size_t chunk = remaining > SPI2_MAX_BYTES_PER_TRANSACTION
        ? SPI2_MAX_BYTES_PER_TRANSACTION
        : remaining;
      int    rv = esp32c3_spi_do_chunk( tx, rx, chunk );

      if ( rv != 0 ) {
        esp32c3_spi_cs_deassert( bus );
        return rv;
      }

      if ( tx != NULL ) {
        tx += chunk;
      }

      if ( rx != NULL ) {
        rx += chunk;
      }

      remaining -= chunk;
    }

    if ( msg->cs_change || i + 1 == msg_count ) {
      esp32c3_spi_cs_deassert( bus );
    }
  }

  return 0;
}

static int esp32c3_spi_setup( spi_bus *base )
{
  esp32c3_spi_bus *bus = (esp32c3_spi_bus *) base;

  if ( bus->base.bits_per_word != 8 || bus->base.speed_hz > SPI2_MAX_FREQUENCY ) {
    return -EINVAL;
  }

  return esp32c3_spi_configure( bus );
}

static void esp32c3_spi_destroy( spi_bus *base )
{
  esp32c3_spi_bus *bus = (esp32c3_spi_bus *) base;

  spi_bus_destroy_and_free( &bus->base );
}

int spi_bus_register_esp32c3(
  const char *bus_path,
  uint32_t    sclk_pin,
  uint32_t    mosi_pin,
  uint32_t    miso_pin,
  uint32_t    cs_pin
)
{
  esp32c3_spi_bus *bus;

  bus = (esp32c3_spi_bus *) spi_bus_alloc_and_init( sizeof( *bus ) );
  if ( bus == NULL ) {
    return -1;
  }

  bus->base.transfer = esp32c3_spi_transfer;
  bus->base.destroy = esp32c3_spi_destroy;
  bus->base.setup = esp32c3_spi_setup;
  bus->base.max_speed_hz = SPI2_MAX_FREQUENCY;
  bus->base.speed_hz = SPI2_MAX_FREQUENCY / 4;
  bus->base.bits_per_word = 8;
  bus->base.mode = SPI_MODE_0;
  bus->base.cs = 0;

  bus->sclk_pin = sclk_pin;
  bus->mosi_pin = mosi_pin;
  bus->miso_pin = miso_pin;
  bus->cs_pin = cs_pin;

  SPI2_REG( SPI_SLAVE_REG ) = SPI_SOFT_RESET;
  SPI2_REG( SPI_SLAVE_REG ) = 0; /* master mode (SPI_SLAVE_MODE == 0) */
  SPI2_REG( SPI_DMA_CONF_REG ) = 0; /* no DMA - CPU-buffer mode only */

  esp32c3_spi_route_output( sclk_pin, FSPICLK_OUT_IDX );
  esp32c3_spi_route_output( mosi_pin, FSPID_OUT_IDX );
  esp32c3_spi_route_input( miso_pin, FSPIQ_IN_IDX );
  esp32c3_spi_cs_init( bus );

  if ( esp32c3_spi_configure( bus ) != 0 ) {
    spi_bus_destroy_and_free( &bus->base );
    return -1;
  }

  return spi_bus_register( &bus->base, bus_path );
}
