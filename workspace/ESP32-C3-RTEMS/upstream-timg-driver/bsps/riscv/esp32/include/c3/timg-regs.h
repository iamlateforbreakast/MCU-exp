/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief TIMG0 (Timer Group 0) general-purpose timer and watchdog
 *   register/field definitions for the ESP32-C3 (esp32c3db BSP).
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
 * DRAFT - see upstream-timg-driver/README.md (two directories up from this
 * file's eventual bsps/riscv/esp32/include/c3/ home) for full status.
 *
 * Every offset, bit position and field description below was checked
 * directly against Espressif's public components/soc/esp32c3/register/
 * soc/timer_group_reg.h while drafting this - and unlike apb_saradc_reg.h
 * (see ../../../../upstream-adc-driver/), that header's field descriptions
 * are fully populated for this peripheral, not blank. Confidence here is
 * high for both the register layout and (from the WDTCONFIG0 fields'
 * prose) the operation sequence - no second cross-check source was needed
 * the way the ADC driver needed esp-hal. That said, high confidence in the
 * register facts didn't prevent a self-inflicted design bug while drafting
 * this: see the watchdog clock-source note further down for a caught (not
 * left in) mismatch between which clock this driver selects and which
 * clock its own timeout arithmetic assumed.
 *
 * This driver only covers TIMG0 (the instance bspstart.c already touches
 * to disable the boot-time watchdog - see WDTWPROTECT below). TIMG1 exists
 * at a different, currently-unused base address and would be a
 * straightforward extension, not drafted here.
 */

#ifndef LIBBSP_ESP32_C3_TIMG_REGS_H
#define LIBBSP_ESP32_C3_TIMG_REGS_H

#ifndef ASM

#include <bsp.h>
#include <bsp/utility.h>

#endif /* ASM */

/* TIMG0 - matches chip_definitions.h's existing TIMG_BASE (already used by
 * bspstart.c's watchdog-disable code), redefined here so this driver
 * doesn't have to depend on that file's naming. */
#define TIMG0_BASE ( (uintptr_t) 0x6001F000U )

#define TIMG_REG( reg ) *( (volatile uint32_t *) ( TIMG0_BASE + ( reg ) ) )

/* General-purpose timer T0: a 54-bit (32-bit low + 22-bit high) up- or
 * down-counting hardware timer, independent of the SYSTIMER the clock
 * driver already uses for the RTOS tick. */
#define TIMG_T0CONFIG_REG    0x00
#define TIMG_T0_EN           BSP_BIT32( 31 )
#define TIMG_T0_INCREASE     BSP_BIT32( 30 ) /* 1 = count up, 0 = count down */
#define TIMG_T0_AUTORELOAD   BSP_BIT32( 29 )
#define TIMG_T0_DIVIDER( v ) BSP_FLD32( v, 13, 28 )
#define TIMG_T0_DIVCNT_RST   BSP_BIT32( 12 )
#define TIMG_T0_ALARM_EN     BSP_BIT32( 10 ) /* self-clears when the alarm fires */
#define TIMG_T0_USE_XTAL     BSP_BIT32( 9 )  /* 1 = XTAL_CLK, 0 = APB_CLK - see README */

#define TIMG_T0LO_REG     0x04 /* RO - counter bits [31:0], latched by T0UPDATE_REG */
#define TIMG_T0HI_REG     0x08 /* RO - counter bits [53:32] (22 bits) */
#define TIMG_T0UPDATE_REG 0x0c /* write any value to latch T0LO/T0HI */

#define TIMG_T0ALARMLO_REG 0x10
#define TIMG_T0ALARMHI_REG 0x14
#define TIMG_T0HI_MASK     0x3fffffU /* 22 bits */

#define TIMG_T0LOADLO_REG 0x18
#define TIMG_T0LOADHI_REG 0x1c
#define TIMG_T0LOAD_REG   0x20 /* write any value to reload the counter from
                                 * T0LOADLO/HI_REG */

#define TIMG_INT_ENA_TIMERS_REG 0x70
#define TIMG_INT_RAW_TIMERS_REG 0x74
#define TIMG_INT_CLR_TIMERS_REG 0x7c
#define TIMG_WDT_INT             BSP_BIT32( 1 )
#define TIMG_T0_INT               BSP_BIT32( 0 )

/*
 * Watchdog (MWDT) clock source. TIMG_WDT_USE_XTAL below selects between
 * APB_CLK (default) and XTAL_CLK - this driver always sets it, to run the
 * watchdog off the 40MHz crystal rather than APB_CLK, for the same reason
 * the general timer above uses T0_USE_XTAL: this BSP's bspstart.c does no
 * clock-tree setup, so APB_CLK's actual boot-time frequency is unconfirmed
 * (see ../../../../upstream-spi-driver/README.md's SPI2_SOURCE_CLK_HZ
 * caveat), while the crystal is a fixed physical component, not subject to
 * any PLL/divider configuration question - independent of that particular
 * uncertainty, though "identifies as a typical 40MHz ESP32-C3 crystal" is
 * still an assumption in its own right, same tier as the general timer's.
 *
 * (TIMG_WDTCONFIG1_REG's own field description states the MWDT clock
 * period as "12.5ns * prescale value" - a figure that assumes APB_CLK at
 * its commonly-quoted 80MHz, i.e. XTAL_CLK selected instead roughly
 * doubles that period per prescale step. Caught and corrected while
 * drafting this: an earlier version of this driver set
 * TIMG_WDT_USE_XTAL but computed cycle counts against 80MHz - internally
 * inconsistent, and would have made every configured timeout run about
 * 2x longer than requested.)
 */
#define TIMG_WDT_REF_CLK_HZ ( 40U * 1000U * 1000U )

#define TIMG_WDTCONFIG0_REG          0x48
#define TIMG_WDT_EN                  BSP_BIT32( 31 )
#define TIMG_WDT_STG0( v )           BSP_FLD32( v, 29, 30 )
#define TIMG_WDT_STG1( v )           BSP_FLD32( v, 27, 28 )
#define TIMG_WDT_STG2( v )           BSP_FLD32( v, 25, 26 )
#define TIMG_WDT_STG3( v )           BSP_FLD32( v, 23, 24 )
#define TIMG_WDT_CONF_UPDATE_EN      BSP_BIT32( 22 ) /* WT - latch config changes */
#define TIMG_WDT_USE_XTAL            BSP_BIT32( 21 )
#define TIMG_WDT_FLASHBOOT_MOD_EN    BSP_BIT32( 14 ) /* ROM's boot-time watchdog path - see README */

#define TIMG_WDT_STG_OFF        0
#define TIMG_WDT_STG_INTERRUPT  1
#define TIMG_WDT_STG_RESET_CPU  2
#define TIMG_WDT_STG_RESET_SYS  3

#define TIMG_WDTCONFIG1_REG        0x4c
#define TIMG_WDT_CLK_PRESCALE( v ) BSP_FLD32( v, 16, 31 )

#define TIMG_WDTCONFIG2_REG 0x50 /* stage 0 timeout, in MWDT clock cycles */
#define TIMG_WDTCONFIG3_REG 0x54 /* stage 1 */
#define TIMG_WDTCONFIG4_REG 0x58 /* stage 2 */
#define TIMG_WDTCONFIG5_REG 0x5c /* stage 3 */

#define TIMG_WDTFEED_REG 0x60 /* write any value to feed/reset the watchdog */

/*
 * Write protection: while TIMG_WDTWPROTECT_REG holds any value other than
 * this key, writes to WDTCONFIG0-5/WDTFEED are ignored. bspstart.c's own
 * RTC (not TIMG) watchdog-disable code uses the identical numeric key for
 * RTC_CNTL_WDTWPROTECT_REG - confirmed by direct arithmetic comparison
 * against this register's own reset-default value while drafting this,
 * not assumed by name similarity.
 */
#define TIMG_WDTWPROTECT_REG 0x64
#define TIMG_WDT_WKEY        0x50d83aa1U

#endif /* LIBBSP_ESP32_C3_TIMG_REGS_H */
