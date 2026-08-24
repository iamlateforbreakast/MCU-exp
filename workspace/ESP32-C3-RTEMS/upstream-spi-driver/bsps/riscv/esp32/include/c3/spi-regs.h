/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief GP-SPI2 controller and GPIO-matrix pin-routing register/field
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
 * DRAFT - see upstream-spi-driver/README.md (two directories up from this
 * file's eventual bsps/riscv/esp32/include/c3/ home) for full status.
 *
 * Unlike the companion GPIO driver's gpio-regs.h, every offset and bit
 * position below was checked directly against Espressif's public ESP-IDF
 * register headers while drafting this (components/soc/esp32c3/register/
 * soc/{spi_reg,gpio_reg,io_mux_reg}.h and components/soc/esp32c3/include/
 * soc/gpio_sig_map.h, fetched from github.com/espressif/esp-idf), not
 * recalled from memory - so confidence here is high for the register
 * layout itself. What is NOT verified is the driver logic built on top of
 * it (clock divider math, full-duplex CPU-buffer sequencing, the GPIO
 * matrix routing direction) - see the README's confidence breakdown.
 */

#ifndef LIBBSP_ESP32_C3_SPI_REGS_H
#define LIBBSP_ESP32_C3_SPI_REGS_H

#ifndef ASM

#include <bsp.h>
#include <bsp/utility.h>
#include <c3/gpio-regs.h>

#endif /* ASM */

/*
 * GP-SPI2 ("FSPI" in Espressif's signal-map naming) - the one general
 * purpose SPI controller on the ESP32-C3 available to applications. SPI0
 * and SPI1 (0x6000_2000/0x6000_3000) are dedicated to the flash/PSRAM
 * controller and are not usable as a general SPI master.
 */
#define SPI2_BASE ( (uintptr_t) 0x60024000U )

#define SPI2_REG( reg ) *( (volatile uint32_t *) ( SPI2_BASE + ( reg ) ) )

#define SPI_CMD_REG   0x00
#define SPI_USR       BSP_BIT32( 24 )
#define SPI_UPDATE    BSP_BIT32( 23 )

#define SPI_CTRL_REG     0x08
#define SPI_WR_BIT_ORDER BSP_BIT32( 26 ) /* 1 = LSB first on MOSI */
#define SPI_RD_BIT_ORDER BSP_BIT32( 25 ) /* 1 = LSB first on MISO */

/*
 * spi_clk = source_clk / (CLKDIV_PRE + 1) / (CLKCNT_N + 1), with
 * CLKCNT_H = floor((CLKCNT_N + 1) / 2) - 1 and CLKCNT_L = CLKCNT_N for a
 * 50% duty cycle - straight from the register field descriptions.
 */
#define SPI_CLOCK_REG        0x0c
#define SPI_CLK_EQU_SYSCLK   BSP_BIT32( 31 )
#define SPI_CLKDIV_PRE( v )  BSP_FLD32( v, 18, 21 )
#define SPI_CLKCNT_N( v )    BSP_FLD32( v, 12, 17 )
#define SPI_CLKCNT_H( v )    BSP_FLD32( v, 6, 11 )
#define SPI_CLKCNT_L( v )    BSP_FLD32( v, 0, 5 )

#define SPI_USER_REG      0x10
#define SPI_USR_MISO      BSP_BIT32( 28 )
#define SPI_USR_MOSI      BSP_BIT32( 27 )
#define SPI_CK_OUT_EDGE   BSP_BIT32( 9 )
#define SPI_CS_SETUP      BSP_BIT32( 7 )
#define SPI_CS_HOLD       BSP_BIT32( 6 )
#define SPI_DOUTDIN       BSP_BIT32( 0 ) /* 1 = full duplex */

#define SPI_MS_DLEN_REG        0x1c
#define SPI_MS_DATA_BITLEN( v ) BSP_FLD32( v, 0, 17 ) /* value = bit count - 1 */

#define SPI_MISC_REG      0x20
#define SPI_CK_IDLE_EDGE  BSP_BIT32( 29 ) /* idle clock level - part of mode */
#define SPI_CS2_DIS       BSP_BIT32( 2 )
#define SPI_CS1_DIS       BSP_BIT32( 1 )
#define SPI_CS0_DIS       BSP_BIT32( 0 )

/* Left at its reset value of 0 throughout this driver - CPU-buffer mode via
 * SPI_W0_REG..SPI_W15_REG below, no DMA. */
#define SPI_DMA_CONF_REG 0x30

#define SPI_W0_REG( i ) ( 0x98 + 4 * ( i ) ) /* i = 0..15, 16 x 32-bit = 64 bytes max per USR transaction */

#define SPI_SLAVE_REG   0xe0
#define SPI_SOFT_RESET  BSP_BIT32( 27 )
#define SPI_SLAVE_MODE  BSP_BIT32( 26 ) /* 0 = master (this driver only supports master) */

/*
 * Distinct from (and in addition to) SYSTEM_PERIP_CLK_EN0_REG's SPI2 bit,
 * which only gates APB register access - confirmed already correctly
 * enabled before this driver runs (see README). This register instead
 * gates the SPI module's own internal functional/master clock domain,
 * i.e. the clock that actually drives the USR transaction state machine
 * and SCLK generation. Without MST_CLK_ACTIVE set, register writes
 * (CMD_REG, UPDATE) still work over APB, but the hardware never produces
 * a single SCLK edge and SPI_USR never self-clears - exactly the observed
 * hang. Matches Espressif's spi_ll_enable_clock()/spi_ll_master_init() in
 * hal/esp32c3/include/hal/spi_ll.h.
 */
#define SPI_CLK_GATE_REG    0xe8
#define SPI_MST_CLK_SEL     BSP_BIT32( 2 ) /* 1 = 80MHz PLL, 0 = XTAL */
#define SPI_MST_CLK_ACTIVE  BSP_BIT32( 1 ) /* power on the SPI module's own clock */
#define SPI_CLK_EN          BSP_BIT32( 0 ) /* enable the clk_gate register's functional clock */

#define SPI2_MAX_BYTES_PER_TRANSACTION 64

/*
 * GP-SPI2's clock source in this BSP is unconfirmed: bspstart.c does no
 * clock-tree setup at all (this is a direct-boot BSP, no 2nd-stage
 * bootloader - see ../../ESP32-C3-RTEMS.md), so the chip is left running
 * on whatever reset-default clock the hardware comes up on, most likely
 * the 40MHz crystal directly rather than an 80MHz APB clock derived from
 * the PLL. Getting this wrong makes every computed SPI bit rate wrong by a
 * fixed ratio (not a functional failure, but a silent one) - confirm the
 * real running frequency (oscilloscope on SCLK, or a future RTEMS
 * clock-tree driver for this BSP) before trusting speed_hz.
 */
#define SPI2_SOURCE_CLK_HZ ( 40U * 1000U * 1000U )
#define SPI2_MAX_FREQUENCY SPI2_SOURCE_CLK_HZ

/*
 * GPIO-matrix routing, needed here because - unlike simple digital I/O -
 * GP-SPI2's pins are not fixed: any of GPIO0-21 can be routed to SCLK/
 * MOSI/MISO/CS via this matrix. Not needed by (and not present in) the
 * companion GPIO driver's gpio-regs.h.
 *
 * The two directions are indexed differently, per Espressif's
 * gpio_matrix_in()/gpio_matrix_out() convention:
 *   - GPIO_FUNCn_IN_SEL_CFG_REG is indexed by SIGNAL number (n = the
 *     peripheral input signal, e.g. FSPIQ_IN_IDX) and its low 5 bits name
 *     the source GPIO.
 *   - GPIO_FUNCn_OUT_SEL_CFG_REG is indexed by GPIO PIN number (n = the
 *     GPIO) and its low 8 bits name the signal to drive that pin with.
 */
#define GPIO_FUNC_IN_SEL_CFG_REG( signal ) ( 0x154 + 4 * ( signal ) )
#define GPIO_SIG_IN_SEL                    BSP_BIT32( 6 ) /* enable routing */
#define GPIO_FUNC_IN_SEL( gpio )           BSP_FLD32( gpio, 0, 4 )

#define GPIO_FUNC_OUT_SEL_CFG_REG( gpio ) ( 0x554 + 4 * ( gpio ) )
#define GPIO_FUNC_OEN_SEL                 BSP_BIT32( 9 ) /* 1 = GPIO_ENABLE_REG controls OE */
#define GPIO_FUNC_OUT_SEL( signal )       BSP_FLD32( signal, 0, 7 )

/* From ESP-IDF's soc/gpio_sig_map.h for esp32c3 - GP-SPI2 ("FSPI") signals. */
#define FSPICLK_OUT_IDX 63
#define FSPIQ_IN_IDX    64 /* MISO, master input */
#define FSPID_OUT_IDX   65 /* MOSI, master output */

#endif /* LIBBSP_ESP32_C3_SPI_REGS_H */
