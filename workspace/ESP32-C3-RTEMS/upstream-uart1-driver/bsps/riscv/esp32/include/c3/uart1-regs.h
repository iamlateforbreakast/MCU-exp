/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief UART1 register/field definitions for the ESP32-C3 (esp32c3db
 *   BSP).
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
 * DRAFT - see upstream-uart1-driver/README.md (two directories up from
 * this file's eventual bsps/riscv/esp32/include/c3/ home) for full status.
 *
 * Every offset and bit position below was checked directly against
 * Espressif's public components/soc/esp32c3/register/soc/uart_reg.h while
 * drafting this, fully documented for this peripheral (like timg-regs.h,
 * unlike apb_saradc_reg.h). Two field *value* encodings (not positions -
 * BIT_NUM/STOP_BIT_NUM/PARITY select among several settings, and the
 * header names the field but not, for PARITY specifically, which value
 * means even vs. odd) are standard-convention assumptions, flagged where
 * used below.
 */

#ifndef LIBBSP_ESP32_C3_UART1_REGS_H
#define LIBBSP_ESP32_C3_UART1_REGS_H

#ifndef ASM

#include <bsp.h>
#include <bsp/utility.h>
#include <c3/gpio-regs.h>

#endif /* ASM */

#define UART1_BASE ( (uintptr_t) 0x60010000U )

#define UART1_REG( reg ) *( (volatile uint32_t *) ( UART1_BASE + ( reg ) ) )

#define UART_FIFO_REG 0x00 /* low 8 bits: read pops RXFIFO, write pushes TXFIFO */

#define UART_CLKDIV_REG        0x14
#define UART_CLKDIV_FRAG( v )  BSP_FLD32( v, 20, 23 ) /* 1/16ths */
#define UART_CLKDIV_INT( v )   BSP_FLD32( v, 0, 11 )

#define UART_STATUS_REG 0x1c
#define UART_TXFIFO_CNT_GET( reg ) BSP_FLD32GET( reg, 16, 25 )
#define UART_RXFIFO_CNT_GET( reg ) BSP_FLD32GET( reg, 0, 9 )

/*
 * Not independently confirmed for the ESP32-C3 specifically - a commonly
 * cited figure for this UART IP block, reused across the ESP32 family. If
 * wrong, the practical effect is just conservative/liberal FIFO-full
 * polling (see esp32c3-uart1.c), not data corruption.
 */
#define UART_FIFO_DEPTH 128

#define UART_CONF0_REG      0x20
#define UART_TXFIFO_RST     BSP_BIT32( 18 )
#define UART_RXFIFO_RST     BSP_BIT32( 17 )
#define UART_STOP_BIT_NUM( v ) BSP_FLD32( v, 4, 5 )
#define UART_BIT_NUM( v )      BSP_FLD32( v, 2, 3 )
#define UART_PARITY_EN         BSP_BIT32( 1 )
#define UART_PARITY_ODD        BSP_BIT32( 0 ) /* assumed: 0 = even, 1 = odd - see file header */

#define UART_BIT_NUM_5 0
#define UART_BIT_NUM_6 1
#define UART_BIT_NUM_7 2
#define UART_BIT_NUM_8 3

#define UART_STOP_BIT_1   1
#define UART_STOP_BIT_1_5 2
#define UART_STOP_BIT_2   3

/*
 * UART1's clock source (like GP-SPI2's and I2C_EXT0's - see the other
 * drivers' READMEs) is this BSP's unconfirmed boot-time APB_CLK, not a
 * fixed reference like TIMG's watchdog: no XTAL/APB source-select bit was
 * found in UART1's own register block for this drafting session, unlike
 * TIMG_T0_USE_XTAL/TIMG_WDT_USE_XTAL, and while the UART_CLKDIV_REG /
 * UART_CLKDIV_FRAG_REG reset default (0x2B6 = 694, integer part) works
 * out to almost exactly 115200 baud assuming an 80MHz APB source
 * (80000000/694 = 115273, ESP-IDF's overwhelmingly common UART default
 * rate) - a real, if indirect, corroborating data point for 80MHz - it
 * isn't a substitute for confirming this BSP's actual boot-time frequency.
 */
#define UART1_APB_CLK_HZ ( 80U * 1000U * 1000U )

/* From ESP-IDF's soc/gpio_sig_map.h for esp32c3. */
#define U1RXD_IN_IDX  9
#define U1TXD_OUT_IDX 9

#endif /* LIBBSP_ESP32_C3_UART1_REGS_H */
