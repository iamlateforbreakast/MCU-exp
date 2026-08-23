/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief Polled, register-level I2C master driver for the ESP32-C3
 *   (esp32c3db BSP), implementing RTEMS's generic I2C bus API
 *   (dev/i2c/i2c.h).
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
 * upstream-i2c-driver/README.md (two directories up from this file's
 * eventual bsps/riscv/esp32/i2c/ home in the RTEMS tree) for what is and
 * isn't verified, and for the spec/build YAML changes this needs.
 *
 * Scope of this first draft:
 *  - Master mode only, standard 7-bit addressing (I2C_M_TEN rejected).
 *  - Polled - no interrupts.
 *  - The whole i2c_msg[] array for one transfer() call must fit in the
 *    hardware's 8-slot command queue and its (assumed 32-byte, see
 *    i2c-regs.h) TX/RX FIFOs in a single hardware pass: every message
 *    contributes RSTART + WRITE(address) + (WRITE(data) or up to two
 *    READ(data) commands), and the whole batch is queued, run, and
 *    drained once. This covers the overwhelmingly common case (a handful
 *    of small messages - e.g. "write register address, repeated start,
 *    read N bytes" - which is most real sensor/EEPROM traffic) but not
 *    arbitrarily long or many-message transfers; see esp32c3_i2c_transfer()
 *    for the exact limits, all checked explicitly (-EINVAL, not silent
 *    truncation) rather than implementing the hardware's END-based
 *    pause/resume streaming mechanism to lift them.
 */

#include <bsp.h>
#include <bsp/utility.h>
#include <c3/i2c-regs.h>
#include <c3/gpio-regs.h>

#include <dev/i2c/i2c.h>

#include <errno.h>

typedef struct {
  i2c_bus  base;
  uint32_t scl_pin;
  uint32_t sda_pin;
} esp32c3_i2c_bus;

/* Where the bytes read back for one i2c_msg land once the whole batch's
 * RX FIFO is drained after the transaction completes. */
typedef struct {
  uint8_t *dst;
  uint32_t rx_offset;
  uint32_t len;
} esp32c3_i2c_read_dest;

static void esp32c3_i2c_route_pin( uint32_t gpio, uint32_t signal )
{
  uint32_t reg;

  reg = IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) );
  reg &= ~IO_MUX_MCU_SEL( 0x7 );
  reg |= IO_MUX_MCU_SEL( IO_MUX_FUNCTION_GPIO );
  reg |= IO_MUX_FUN_IE;  /* always sense the line level */
  reg |= IO_MUX_FUN_WPU; /* weak internal pull-up - bench convenience only,
                           * not a substitute for real external pull-ups */
  IO_MUX_REG( IO_MUX_GPIO_REG( gpio ) ) = reg;

  /* Open-drain: this pin only ever gets pulled low, never actively driven
   * high - the pull-up (external, or the weak internal one above) does
   * that. */
  GPIO_REG( GPIO_PIN_REG( gpio ) ) |= GPIO_PIN_PAD_DRIVER;

  /* GPIO_FUNC_OEN_SEL is deliberately left clear (reset default): output-
   * enable follows the I2C peripheral's own per-bit signal, not a static
   * GPIO_ENABLE_REG bit - required for open-drain SCL/SDA to toggle in
   * sync with each transmitted bit, unlike SPI2's always-output MOSI/CLK
   * pins in the companion SPI driver, which use GPIO_FUNC_OEN_SEL. */
  GPIO_REG( GPIO_FUNC_OUT_SEL_CFG_REG( gpio ) ) = GPIO_FUNC_OUT_SEL( signal );

  GPIO_REG( GPIO_FUNC_IN_SEL_CFG_REG( signal ) ) =
    GPIO_FUNC_IN_SEL( gpio ) | GPIO_SIG_IN_SEL;
}

static int esp32c3_i2c_set_clock( i2c_bus *base, unsigned long clock )
{
  uint32_t half_cycle;

  if ( clock == 0 || clock > I2C_SOURCE_CLK_HZ / 4 ) {
    return -EINVAL;
  }

  /* Approximate, symmetric timing: every setup/hold parameter this
   * controller exposes is set relative to the bit period rather than
   * computed against the I2C specification's individual timing margins
   * for the requested mode (standard/fast/fast-plus) - see the README. */
  half_cycle = I2C_SOURCE_CLK_HZ / clock / 2;
  if ( half_cycle < 1 ) {
    half_cycle = 1;
  } else if ( half_cycle > 0x1ff ) {
    half_cycle = 0x1ff;
  }

  I2C_REG( I2C_SCL_LOW_PERIOD_REG ) = I2C_SCL_LOW_PERIOD( half_cycle );
  I2C_REG( I2C_SCL_HIGH_PERIOD_REG ) = I2C_SCL_HIGH_PERIOD( half_cycle );
  I2C_REG( I2C_SCL_START_HOLD_REG ) = I2C_SCL_START_HOLD( half_cycle );
  I2C_REG( I2C_SCL_RSTART_SETUP_REG ) = I2C_SCL_RSTART_SETUP( half_cycle );
  I2C_REG( I2C_SCL_STOP_HOLD_REG ) = I2C_SCL_STOP_HOLD( half_cycle );
  I2C_REG( I2C_SCL_STOP_SETUP_REG ) = I2C_SCL_STOP_SETUP( half_cycle );
  I2C_REG( I2C_SDA_HOLD_REG ) = I2C_SDA_HOLD( half_cycle / 2 );
  I2C_REG( I2C_SDA_SAMPLE_REG ) = I2C_SDA_SAMPLE( half_cycle / 2 );

  I2C_REG( I2C_CTR_REG ) |= I2C_CONF_UPGATE;

  base->functionality = I2C_FUNC_I2C;

  return 0;
}

static int esp32c3_i2c_transfer(
  i2c_bus  *base,
  i2c_msg  *msgs,
  uint32_t  msg_count
)
{
  uint32_t              cmds[ I2C_COMD_COUNT ];
  uint32_t              cmd_count = 0;
  uint8_t               tx_bytes[ I2C_FIFO_DEPTH ];
  uint32_t              tx_count = 0;
  esp32c3_i2c_read_dest reads[ I2C_COMD_COUNT ];
  uint32_t              read_count = 0;
  uint32_t              rx_total = 0;
  uint32_t              status;
  uint32_t              i;
  uint32_t              j;

  (void) base;

  if ( msg_count == 0 ) {
    return 0;
  }

  for ( i = 0; i < msg_count; ++i ) {
    i2c_msg *msg = &msgs[ i ];
    bool     is_read = ( msg->flags & I2C_M_RD ) != 0;
    /* Commands this message will push below: RSTART + WRITE(address),
     * always, plus the data phase - one WRITE for a non-empty write, one
     * READ for a 1-byte read, or two READs (ack all but the last byte,
     * then NACK it) for a longer read. Must match what's actually pushed
     * beneath exactly, or cmd_count can walk past cmds[I2C_COMD_COUNT]. */
    uint32_t needed_cmds = 2;

    if ( ( msg->flags & I2C_M_TEN ) != 0 ) {
      /* 10-bit addressing not implemented in this draft. */
      return -EINVAL;
    }

    if ( is_read ) {
      if ( msg->len == 1 ) {
        needed_cmds += 1;
      } else if ( msg->len > 1 ) {
        needed_cmds += 2;
      }
    } else if ( msg->len > 0 ) {
      needed_cmds += 1;
    }

    /* +1 reserves the final STOP command below. */
    if ( cmd_count + needed_cmds + 1 > I2C_COMD_COUNT ) {
      return -EINVAL;
    }

    cmds[ cmd_count++ ] =
      I2C_COMD_OP_CODE( I2C_COMD_OP_RESTART );

    if ( tx_count >= I2C_FIFO_DEPTH ) {
      return -EINVAL;
    }

    tx_bytes[ tx_count++ ] =
      (uint8_t) ( ( msg->addr << 1 ) | ( is_read ? 1 : 0 ) );
    cmds[ cmd_count++ ] = I2C_COMD_OP_CODE( I2C_COMD_OP_WRITE )
      | I2C_COMD_BYTE_NUM( 1 )
      | I2C_COMD_ACK_EN;

    if ( is_read ) {
      if ( msg->len == 0 ) {
        continue;
      }

      if ( rx_total + msg->len > I2C_FIFO_DEPTH ) {
        return -EINVAL;
      }

      if ( read_count >= I2C_COMD_COUNT ) {
        return -EINVAL;
      }

      if ( msg->len > 1 ) {
        cmds[ cmd_count++ ] = I2C_COMD_OP_CODE( I2C_COMD_OP_READ )
          | I2C_COMD_BYTE_NUM( msg->len - 1 );
      }

      cmds[ cmd_count++ ] = I2C_COMD_OP_CODE( I2C_COMD_OP_READ )
        | I2C_COMD_BYTE_NUM( 1 )
        | I2C_COMD_ACK_VALUE; /* NACK the last byte of the message */

      reads[ read_count ].dst = msg->buf;
      reads[ read_count ].rx_offset = rx_total;
      reads[ read_count ].len = msg->len;
      ++read_count;

      rx_total += msg->len;
    } else if ( msg->len > 0 ) {
      if ( tx_count + msg->len > I2C_FIFO_DEPTH ) {
        return -EINVAL;
      }

      for ( j = 0; j < msg->len; ++j ) {
        tx_bytes[ tx_count++ ] = msg->buf[ j ];
      }

      cmds[ cmd_count++ ] = I2C_COMD_OP_CODE( I2C_COMD_OP_WRITE )
        | I2C_COMD_BYTE_NUM( msg->len )
        | I2C_COMD_ACK_EN;
    }
  }

  cmds[ cmd_count++ ] = I2C_COMD_OP_CODE( I2C_COMD_OP_STOP );

  I2C_REG( I2C_FIFO_CONF_REG ) |= I2C_TX_FIFO_RST | I2C_RX_FIFO_RST;
  I2C_REG( I2C_FIFO_CONF_REG ) &= ~( I2C_TX_FIFO_RST | I2C_RX_FIFO_RST );

  for ( i = 0; i < tx_count; ++i ) {
    I2C_REG( I2C_TXFIFO_REG( i ) ) = tx_bytes[ i ];
  }

  for ( i = 0; i < cmd_count; ++i ) {
    I2C_REG( I2C_COMD_REG( i ) ) = cmds[ i ];
  }

  I2C_REG( I2C_INT_CLR_REG ) = I2C_ALL_INT_MASK;

  I2C_REG( I2C_CTR_REG ) |= I2C_CONF_UPGATE;
  I2C_REG( I2C_CTR_REG ) |= I2C_TRANS_START;

  for ( ;; ) {
    status = I2C_REG( I2C_INT_RAW_REG );

    if ( ( status & I2C_ERROR_INT_MASK ) != 0 ) {
      I2C_REG( I2C_INT_CLR_REG ) = I2C_ALL_INT_MASK;
      I2C_REG( I2C_CTR_REG ) |= I2C_FSM_RST;

      if ( ( status & I2C_NACK_INT ) != 0 ) {
        return -EIO;
      } else if ( ( status & I2C_ARBITRATION_LOST_INT ) != 0 ) {
        return -EAGAIN;
      } else {
        return -ETIMEDOUT;
      }
    }

    if ( ( status & I2C_TRANS_COMPLETE_INT ) != 0 ) {
      break;
    }
  }

  for ( i = 0; i < read_count; ++i ) {
    for ( j = 0; j < reads[ i ].len; ++j ) {
      uint32_t word = I2C_REG( I2C_RXFIFO_REG( reads[ i ].rx_offset + j ) );

      reads[ i ].dst[ j ] = (uint8_t) word;
    }
  }

  I2C_REG( I2C_INT_CLR_REG ) = I2C_ALL_INT_MASK;

  return 0;
}

static void esp32c3_i2c_destroy( i2c_bus *base )
{
  esp32c3_i2c_bus *bus = (esp32c3_i2c_bus *) base;

  i2c_bus_destroy_and_free( &bus->base );
}

int i2c_bus_register_esp32c3(
  const char *bus_path,
  uint32_t    scl_pin,
  uint32_t    sda_pin
)
{
  esp32c3_i2c_bus *bus;

  bus = (esp32c3_i2c_bus *) i2c_bus_alloc_and_init( sizeof( *bus ) );
  if ( bus == NULL ) {
    return -1;
  }

  bus->scl_pin = scl_pin;
  bus->sda_pin = sda_pin;

  bus->base.transfer = esp32c3_i2c_transfer;
  bus->base.set_clock = esp32c3_i2c_set_clock;
  bus->base.destroy = esp32c3_i2c_destroy;

  /* |= rather than = : I2C_CTR_REG's reset defaults include several bits
   * that must stay set (SCL_FORCE_OUT/SDA_FORCE_OUT - without them the
   * peripheral never actually drives the bus at all - and
   * ARBITRATION_EN/RX_FULL_ACK_LEVEL); a plain assignment would clobber
   * them to 0. */
  I2C_REG( I2C_CTR_REG ) |= I2C_MS_MODE; /* master mode */
  I2C_REG( I2C_CTR_REG ) |= I2C_FSM_RST;
  I2C_REG( I2C_FIFO_CONF_REG ) &= ~I2C_NONFIFO_EN; /* FIFO mode */

  esp32c3_i2c_route_pin( scl_pin, I2CEXT0_SCL_IDX );
  esp32c3_i2c_route_pin( sda_pin, I2CEXT0_SDA_IDX );

  if ( esp32c3_i2c_set_clock( &bus->base, I2C_BUS_CLOCK_DEFAULT ) != 0 ) {
    i2c_bus_destroy_and_free( &bus->base );
    return -1;
  }

  return i2c_bus_register( &bus->base, bus_path );
}
