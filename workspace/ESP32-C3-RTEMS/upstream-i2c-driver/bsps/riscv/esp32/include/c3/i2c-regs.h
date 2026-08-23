/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief I2C controller and GPIO-matrix pin-routing register/field
 *   definitions for the ESP32-C3 (esp32c3db BSP).
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
 * DRAFT - see upstream-i2c-driver/README.md (two directories up from this
 * file's eventual bsps/riscv/esp32/include/c3/ home) for full status.
 *
 * As with upstream-spi-driver/spi-regs.h, every offset and bit position
 * below was checked directly against Espressif's public ESP-IDF register
 * header (components/soc/esp32c3/register/soc/i2c_reg.h) while drafting
 * this - not recalled from memory. The one exception, called out below at
 * I2C_TXFIFO_REG/I2C_RXFIFO_REG, is the FIFO access stride: that header
 * only gives a single _START_ADDR offset each for TX/RX, not a per-byte
 * layout, so the 4-byte-per-byte stride here is inferred by analogy with
 * this same SoC's SPI2 W0..W15 data-buffer layout (bsps/riscv/esp32/
 * include/c3/spi-regs.h), not independently confirmed for I2C.
 */

#ifndef LIBBSP_ESP32_C3_I2C_REGS_H
#define LIBBSP_ESP32_C3_I2C_REGS_H

#ifndef ASM

#include <bsp.h>
#include <bsp/utility.h>
#include <c3/gpio-regs.h>
/* GPIO_FUNC_{IN,OUT}_SEL{,_CFG_REG}/GPIO_SIG_IN_SEL live here, not in
 * gpio-regs.h - shared with the SPI driver's GPIO-matrix routing. */
#include <c3/spi-regs.h>

#endif /* ASM */

/*
 * The ESP32-C3 has exactly one general-purpose I2C controller ("I2C_EXT0").
 * There is a second, unrelated "RTC_I2C" block (0x6000_e000) used
 * internally for analog/PMU register access, not exposed as a general I2C
 * master.
 */
#define I2C_BASE ( (uintptr_t) 0x60013000U )

#define I2C_REG( reg ) *( (volatile uint32_t *) ( I2C_BASE + ( reg ) ) )

#define I2C_SCL_LOW_PERIOD_REG  0x00
#define I2C_SCL_LOW_PERIOD( v ) BSP_FLD32( v, 0, 8 )

#define I2C_CTR_REG        0x04
#define I2C_CONF_UPGATE    BSP_BIT32( 11 ) /* sync config into I2C module clock domain */
#define I2C_FSM_RST        BSP_BIT32( 10 )
#define I2C_TRANS_START    BSP_BIT32( 5 )  /* execute the currently loaded COMDn queue */
#define I2C_MS_MODE        BSP_BIT32( 4 )  /* 1 = master mode */

#define I2C_SR_REG   0x08
#define I2C_BUS_BUSY BSP_BIT32( 4 )

#define I2C_FIFO_CONF_REG 0x18
#define I2C_TX_FIFO_RST   BSP_BIT32( 13 )
#define I2C_RX_FIFO_RST   BSP_BIT32( 12 )
#define I2C_NONFIFO_EN    BSP_BIT32( 10 ) /* left clear - this driver uses FIFO mode */

#define I2C_INT_RAW_REG           0x20
#define I2C_INT_CLR_REG           0x24
#define I2C_NACK_INT             BSP_BIT32( 10 )
#define I2C_TIME_OUT_INT         BSP_BIT32( 8 )
#define I2C_TRANS_COMPLETE_INT   BSP_BIT32( 7 )
#define I2C_ARBITRATION_LOST_INT BSP_BIT32( 5 )
#define I2C_END_DETECT_INT       BSP_BIT32( 3 )
#define I2C_ERROR_INT_MASK \
  ( I2C_NACK_INT | I2C_TIME_OUT_INT | I2C_ARBITRATION_LOST_INT )
#define I2C_ALL_INT_MASK 0x3ffff /* every defined I2C_INT_* bit, for INT_CLR */

#define I2C_SDA_HOLD_REG   0x30
#define I2C_SDA_HOLD( v )  BSP_FLD32( v, 0, 8 )
#define I2C_SDA_SAMPLE_REG 0x34
#define I2C_SDA_SAMPLE( v ) BSP_FLD32( v, 0, 8 )

#define I2C_SCL_HIGH_PERIOD_REG  0x38
#define I2C_SCL_HIGH_PERIOD( v ) BSP_FLD32( v, 0, 8 )

#define I2C_SCL_START_HOLD_REG    0x40
#define I2C_SCL_START_HOLD( v )   BSP_FLD32( v, 0, 8 )
#define I2C_SCL_RSTART_SETUP_REG  0x44
#define I2C_SCL_RSTART_SETUP( v ) BSP_FLD32( v, 0, 8 )
#define I2C_SCL_STOP_HOLD_REG     0x48
#define I2C_SCL_STOP_HOLD( v )    BSP_FLD32( v, 0, 8 )
#define I2C_SCL_STOP_SETUP_REG    0x4c
#define I2C_SCL_STOP_SETUP( v )   BSP_FLD32( v, 0, 8 )

/* Source-clock select/pre-divider - left at reset defaults (XTAL, /1) in
 * this driver; only the PERIOD, HOLD, and SETUP registers above are used
 * to set the actual bit rate. See I2C_SOURCE_CLK_HZ below. */
#define I2C_CLK_CONF_REG 0x54

/*
 * Command queue: up to 8 slots, each a 14-bit (RESTART/WRITE/READ/STOP/END)
 * instruction. I2C_TRANS_START (in I2C_CTR_REG) executes the currently
 * loaded queue from I2C_COMD0_REG, in order, until it hits a STOP (bus
 * released, I2C_TRANS_COMPLETE_INT fires) or an END (bus held - SCL low -
 * paused for firmware to reload the queue and re-trigger, e.g. once this
 * driver's whole-transaction command list doesn't fit in 8 slots; not used
 * in this first draft, see the README's scope section).
 */
#define I2C_COMD0_REG      0x58
#define I2C_COMD_REG( n )  ( I2C_COMD0_REG + 4 * ( n ) )
#define I2C_COMD_COUNT     8

#define I2C_COMD_OP_RESTART 0
#define I2C_COMD_OP_WRITE   1
#define I2C_COMD_OP_READ    2
#define I2C_COMD_OP_STOP    3
#define I2C_COMD_OP_END     4

#define I2C_COMD_BYTE_NUM( v ) BSP_FLD32( v, 0, 7 )
#define I2C_COMD_ACK_EN     BSP_BIT32( 8 )  /* WRITE: abort (NACK_INT) if the slave doesn't ACK */
#define I2C_COMD_ACK_EXP    BSP_BIT32( 9 )  /* WRITE: expected ACK level, 0 = normal ACK */
#define I2C_COMD_ACK_VALUE  BSP_BIT32( 10 ) /* READ: ACK bit master sends back, 1 = NACK (last byte) */
#define I2C_COMD_OP_CODE( v ) BSP_FLD32( v, 11, 13 )

/*
 * FIFO access. UNVERIFIED stride - see file header. Each 32-bit-aligned
 * slot is assumed to hold one FIFO byte in its low 8 bits, mirroring
 * SPI2's SPI_W0_REG..SPI_W15_REG layout on this same SoC.
 */
#define I2C_TXFIFO_START_ADDR_REG 0x100
#define I2C_RXFIFO_START_ADDR_REG 0x180
#define I2C_TXFIFO_REG( i ) ( I2C_TXFIFO_START_ADDR_REG + 4 * ( i ) )
#define I2C_RXFIFO_REG( i ) ( I2C_RXFIFO_START_ADDR_REG + 4 * ( i ) )

/* Assumed FIFO depth (bytes) - not independently confirmed, see README. */
#define I2C_FIFO_DEPTH 32

/*
 * I2C_EXT0's source clock, like GP-SPI2's (see spi-regs.h), is unconfirmed
 * in this direct-boot BSP - bspstart.c does no clock-tree setup. 40MHz
 * (typical ESP32-C3 crystal, XTAL is I2C_SCLK_SEL's reset-default source)
 * is a guess, not a measurement.
 */
#define I2C_SOURCE_CLK_HZ ( 40U * 1000U * 1000U )

/* From ESP-IDF's soc/gpio_sig_map.h for esp32c3 - the sole I2C controller
 * ("I2C_EXT0"). SCL and SDA share the same index for IN and OUT, since
 * each is a single open-drain matrix signal, not separate uni-directional
 * ones like SPI's. */
#define I2CEXT0_SCL_IDX 53
#define I2CEXT0_SDA_IDX 54

#endif /* LIBBSP_ESP32_C3_I2C_REGS_H */
