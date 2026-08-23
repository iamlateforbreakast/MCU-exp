/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief LEDC (PWM) register/field definitions for the ESP32-C3
 *   (esp32c3db BSP).
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
 * DRAFT - see upstream-ledc-driver/README.md (two directories up from this
 * file's eventual bsps/riscv/esp32/include/c3/ home) for full status.
 *
 * Register offsets/bit positions were checked directly against Espressif's
 * public components/soc/esp32c3/register/soc/ledc_reg.h while drafting
 * this - like apb_saradc_reg.h (../../../../upstream-adc-driver/), that
 * header's field descriptions are blank for this peripheral. As with the
 * ADC driver, a second independent source - Espressif's esp-hal Rust
 * driver (esp-hal/src/ledc/low_level/v1.rs in github.com/esp-rs/esp-hal,
 * "v1" being the LEDC hardware generation the ESP32-C3 uses) - supplied
 * the actual register *sequence*, not just field positions: which
 * registers to touch together, in what order, and the specific
 * "DUTY_NUM=1, DUTY_CYCLE=1, DUTY_SCALE=0" values that make the hardware's
 * fade engine apply a duty change immediately rather than fading it in.
 * That sequence is used directly in esp32c3-ledc.c, confirmed by real
 * working code rather than reconstructed from field names alone.
 *
 * The ESP32-C3's LEDC only has "low-speed" (LS) timers/channels - unlike
 * the original ESP32, there is no separate high-speed channel group.
 */

#ifndef LIBBSP_ESP32_C3_LEDC_REGS_H
#define LIBBSP_ESP32_C3_LEDC_REGS_H

#ifndef ASM

#include <bsp.h>
#include <bsp/utility.h>
#include <c3/gpio-regs.h>

#endif /* ASM */

#define LEDC_BASE ( (uintptr_t) 0x60019000U )

#define LEDC_REG( reg ) *( (volatile uint32_t *) ( LEDC_BASE + ( reg ) ) )

#define LEDC_CHANNEL_COUNT 6
#define LEDC_TIMER_COUNT   4
#define LEDC_MAX_DUTY_RES  14 /* bits - see README */

/* Per-channel registers: 0x14 (20) bytes apart, confirmed by the offsets
 * of LSCH0 (0x00) vs LSCH1 (0x14) in the fetched header. */
#define LEDC_CH_BASE   0x00
#define LEDC_CH_STRIDE 0x14

#define LEDC_CH_CONF0_REG( ch ) ( LEDC_CH_BASE + LEDC_CH_STRIDE * ( ch ) + 0x00 )
#define LEDC_CH_HPOINT_REG( ch ) ( LEDC_CH_BASE + LEDC_CH_STRIDE * ( ch ) + 0x04 )
#define LEDC_CH_DUTY_REG( ch ) ( LEDC_CH_BASE + LEDC_CH_STRIDE * ( ch ) + 0x08 )
#define LEDC_CH_CONF1_REG( ch ) ( LEDC_CH_BASE + LEDC_CH_STRIDE * ( ch ) + 0x0c )

#define LEDC_CH_TIMER_SEL_MASK    BSP_FLD32( 0x3, 0, 1 )
#define LEDC_CH_TIMER_SEL( v )    BSP_FLD32( v, 0, 1 )
#define LEDC_CH_SIG_OUT_EN        BSP_BIT32( 2 )
#define LEDC_CH_PARA_UP           BSP_BIT32( 4 ) /* WO - latch pending changes for this channel */

#define LEDC_CH_DUTY_START        BSP_BIT32( 31 )
#define LEDC_CH_DUTY_INC          BSP_BIT32( 30 )
#define LEDC_CH_DUTY_NUM( v )     BSP_FLD32( v, 20, 29 )
#define LEDC_CH_DUTY_CYCLE( v )   BSP_FLD32( v, 10, 19 )
#define LEDC_CH_DUTY_SCALE( v )   BSP_FLD32( v, 0, 9 )

/* Per-timer registers: 0x08 bytes apart (LSTIMER0 at 0xa0, LSTIMER1 at
 * 0xa8). */
#define LEDC_TIMER_BASE   0xa0
#define LEDC_TIMER_STRIDE 0x08

#define LEDC_TIMER_CONF_REG( t ) ( LEDC_TIMER_BASE + LEDC_TIMER_STRIDE * ( t ) )

#define LEDC_TIMER_PARA_UP        BSP_BIT32( 25 ) /* WO */
#define LEDC_TIMER_TICK_SEL_APB   BSP_BIT32( 24 ) /* 1 = APB_CLK, 0 = REF_TICK */
#define LEDC_TIMER_RST            BSP_BIT32( 23 ) /* defaults to 1 (held in reset) at power-on */
#define LEDC_TIMER_PAUSE          BSP_BIT32( 22 )
#define LEDC_TIMER_CLK_DIV( v )   BSP_FLD32( v, 4, 21 )
#define LEDC_TIMER_DUTY_RES( v )  BSP_FLD32( v, 0, 3 )

/*
 * LEDC's global slow-clock source is APB_CLK (see LEDC_APB_CLK_SEL below)
 * - this BSP's unconfirmed boot-time APB frequency, same caveat as the
 * SPI2/I2C/UART1 drivers' own *_APB_CLK_HZ constants (each driver keeps
 * its own copy rather than sharing one, consistent with how this repo's
 * other drafts are structured - see their READMEs for the underlying
 * "bspstart.c does no clock-tree setup" reason).
 */
#define LEDC_APB_CLK_HZ ( 80U * 1000U * 1000U )

/*
 * Timer clock divider: an 8-bit-fractional fixed-point value,
 * tick_freq = source_clk_freq / (clk_div_integer + clk_div_fraction/256) -
 * a well-established convention across the ESP32 LEDC family. Not
 * independently re-confirmed against esp-hal/the register header for this
 * specific fractional width in this drafting session (esp-hal's
 * ls_configure_hw() takes a pre-computed "divisor" without showing the
 * bit-width breakdown) - flagged in the README.
 */
#define LEDC_CLK_DIV_FRACTIONAL_BITS 8

/*
 * Global config register. LEDC_APB_CLK_SEL is a 2-bit field (unlike what
 * esp-hal's set_global_slow_clock()'s `.apb_clk_sel().set_bit()` call
 * might suggest at a glance - that call sets the field to its "APB_CLK"
 * enum variant, not literally just bit 0). Value 1 below is this driver's
 * best-reasoned choice for "select APB_CLK" given the field name and
 * default-0 reset value, not independently confirmed against a source
 * that states the enum values explicitly.
 */
#define LEDC_CONF_REG          0xd0
#define LEDC_APB_CLK_SEL( v )  BSP_FLD32( v, 0, 1 )
#define LEDC_APB_CLK_SEL_APB   1

/* From ESP-IDF's soc/gpio_sig_map.h for esp32c3 - LEDC_LS_SIG_OUTn_IDX,
 * n = 0..5, consecutive. */
#define LEDC_LS_SIG_OUT_IDX( ch ) ( 45 + ( ch ) )

#endif /* LIBBSP_ESP32_C3_LEDC_REGS_H */
