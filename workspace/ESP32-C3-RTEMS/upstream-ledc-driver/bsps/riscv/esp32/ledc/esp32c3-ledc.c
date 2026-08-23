/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief LEDC (PWM) driver for the ESP32-C3 (esp32c3db BSP).
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
 * upstream-ledc-driver/README.md (two directories up from this file's
 * eventual bsps/riscv/esp32/ledc/ home in the RTEMS tree) for what is and
 * isn't verified, and for the spec/build YAML changes this needs.
 *
 * Scope of this first draft:
 *  - Duty changes always apply immediately (no fade) - this driver always
 *    programs the hardware fade engine with a degenerate one-step,
 *    zero-scale "fade" (see esp32c3_ledc_apply_duty()), matching
 *    esp-hal's start_duty_without_fading() rather than exposing the real
 *    multi-step fade capability (DUTY_NUM/DUTY_CYCLE/DUTY_SCALE) this
 *    hardware has.
 *  - No interrupt handling - duty/fade-done interrupts exist
 *    (LEDC_INT_RAW_REG) but aren't used; this driver's calls are all
 *    fire-and-forget register writes.
 *  - Global slow-clock source is always APB_CLK (LEDC_CONF_REG's
 *    APB_CLK_SEL, set once on first use) - the alternative REF_TICK/
 *    RC_FAST/XTAL sources this register supports aren't exposed.
 */

#include <bsp.h>
#include <bsp/utility.h>
#include <c3/ledc-regs.h>
#include <c3/ledc.h>
#include <c3/gpio-regs.h>

static bool esp32c3_ledc_clock_configured = false;

static bool esp32c3_ledc_timer_configured[ LEDC_TIMER_COUNT ];
static uint32_t esp32c3_ledc_timer_duty_res[ LEDC_TIMER_COUNT ];

static bool esp32c3_ledc_channel_configured[ LEDC_CHANNEL_COUNT ];
static uint32_t esp32c3_ledc_channel_timer[ LEDC_CHANNEL_COUNT ];

static void esp32c3_ledc_configure_clock( void )
{
  if ( esp32c3_ledc_clock_configured ) {
    return;
  }

  /* Matches esp-hal's set_global_slow_clock(): select APB_CLK in the
   * global config register, then latch it via timer 0's PARA_UP - this
   * is genuinely how the reference implementation does it, not an
   * assumption made here. */
  LEDC_REG( LEDC_CONF_REG ) = LEDC_APB_CLK_SEL( LEDC_APB_CLK_SEL_APB );
  LEDC_REG( LEDC_TIMER_CONF_REG( 0 ) ) |= LEDC_TIMER_PARA_UP;

  esp32c3_ledc_clock_configured = true;
}

static void esp32c3_ledc_route_output( uint32_t gpio, uint32_t signal )
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

rtems_status_code esp32c3_ledc_timer_config(
  uint32_t timer,
  uint32_t freq_hz,
  uint32_t duty_resolution_bits
)
{
  uint64_t tick_hz;
  uint64_t clk_div_fixed;

  if (
    timer >= LEDC_TIMER_COUNT
      || duty_resolution_bits < 1
      || duty_resolution_bits > LEDC_MAX_DUTY_RES
      || freq_hz == 0
  ) {
    return RTEMS_INVALID_NUMBER;
  }

  esp32c3_ledc_configure_clock();

  /* tick_hz = freq_hz * 2^duty_resolution_bits - the timer counts one
   * full period of 2^duty_resolution_bits ticks per PWM cycle.
   * clk_div_fixed = round(APB_CLK_HZ * 256 / tick_hz), an 8-bit-
   * fractional fixed-point divider (see ledc-regs.h). */
  tick_hz = (uint64_t) freq_hz << duty_resolution_bits;
  if ( tick_hz == 0 || tick_hz > LEDC_APB_CLK_HZ ) {
    /* tick_hz > source clock: divider would be < 1, unrepresentable. */
    return RTEMS_INVALID_NUMBER;
  }

  clk_div_fixed =
    ( (uint64_t) LEDC_APB_CLK_HZ * 256U + tick_hz / 2 ) / tick_hz;

  if ( clk_div_fixed == 0 || clk_div_fixed > 0x3ffffU ) {
    return RTEMS_INVALID_NUMBER;
  }

  LEDC_REG( LEDC_TIMER_CONF_REG( timer ) ) =
    LEDC_TIMER_TICK_SEL_APB
      | LEDC_TIMER_CLK_DIV( (uint32_t) clk_div_fixed )
      | LEDC_TIMER_DUTY_RES( duty_resolution_bits );
  /* RST and PAUSE both omitted above, i.e. left clear - RST defaults to 1
   * at power-on (see ledc-regs.h) so this write's implicit "clear" is
   * required, not just a default already in place, matching esp-hal's
   * explicit w.rst().clear_bit(). */

  LEDC_REG( LEDC_TIMER_CONF_REG( timer ) ) |= LEDC_TIMER_PARA_UP;

  esp32c3_ledc_timer_configured[ timer ] = true;
  esp32c3_ledc_timer_duty_res[ timer ] = duty_resolution_bits;

  return RTEMS_SUCCESSFUL;
}

static void esp32c3_ledc_apply_duty( uint32_t channel, uint32_t duty )
{
  /* duty << 4: the DUTY register carries 4 fractional bits beyond the
   * timer's duty resolution - confirmed directly against esp-hal's
   * set_duty_hw(), not inferred from the field width alone. */
  LEDC_REG( LEDC_CH_DUTY_REG( channel ) ) = duty << 4;

  /* A degenerate one-step, zero-scale "fade" applies the duty value above
   * immediately rather than ramping to it - confirmed against esp-hal's
   * start_duty_without_fading(), which uses these exact values. */
  LEDC_REG( LEDC_CH_CONF1_REG( channel ) ) = LEDC_CH_DUTY_START
    | LEDC_CH_DUTY_INC
    | LEDC_CH_DUTY_NUM( 1 )
    | LEDC_CH_DUTY_CYCLE( 1 )
    | LEDC_CH_DUTY_SCALE( 0 );

  LEDC_REG( LEDC_CH_CONF0_REG( channel ) ) |= LEDC_CH_PARA_UP;
}

rtems_status_code esp32c3_ledc_channel_config(
  uint32_t channel,
  uint32_t timer,
  uint32_t gpio
)
{
  uint32_t conf0;

  if ( channel >= LEDC_CHANNEL_COUNT || timer >= LEDC_TIMER_COUNT ) {
    return RTEMS_INVALID_NUMBER;
  }

  if ( !esp32c3_ledc_timer_configured[ timer ] ) {
    return RTEMS_NOT_CONFIGURED;
  }

  esp32c3_ledc_route_output( gpio, LEDC_LS_SIG_OUT_IDX( channel ) );

  LEDC_REG( LEDC_CH_HPOINT_REG( channel ) ) = 0;

  conf0 = LEDC_REG( LEDC_CH_CONF0_REG( channel ) );
  conf0 &= ~LEDC_CH_TIMER_SEL_MASK;
  conf0 |= LEDC_CH_TIMER_SEL( timer ) | LEDC_CH_SIG_OUT_EN;
  LEDC_REG( LEDC_CH_CONF0_REG( channel ) ) = conf0;

  esp32c3_ledc_channel_timer[ channel ] = timer;
  esp32c3_ledc_channel_configured[ channel ] = true;

  esp32c3_ledc_apply_duty( channel, 0 );

  return RTEMS_SUCCESSFUL;
}

rtems_status_code esp32c3_ledc_set_duty( uint32_t channel, uint32_t duty )
{
  uint32_t timer;
  uint32_t max_duty;

  if ( channel >= LEDC_CHANNEL_COUNT ) {
    return RTEMS_INVALID_NUMBER;
  }

  if ( !esp32c3_ledc_channel_configured[ channel ] ) {
    return RTEMS_NOT_CONFIGURED;
  }

  timer = esp32c3_ledc_channel_timer[ channel ];
  max_duty = ( 1U << esp32c3_ledc_timer_duty_res[ timer ] ) - 1U;

  if ( duty > max_duty ) {
    return RTEMS_INVALID_NUMBER;
  }

  esp32c3_ledc_apply_duty( channel, duty );

  return RTEMS_SUCCESSFUL;
}
