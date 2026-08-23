/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief TIMG0 general-purpose timer and watchdog driver for the
 *   ESP32-C3 (esp32c3db BSP).
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
 * upstream-timg-driver/README.md (two directories up from this file's
 * eventual bsps/riscv/esp32/timg/ home in the RTEMS tree) for what is and
 * isn't verified, and for how this interacts with bsp_start()'s existing
 * boot-time watchdog handling.
 *
 * Scope of this first draft:
 *  - TIMG0 only, not TIMG1 - see timg-regs.h.
 *  - General timer: polled only (alarm status is read via
 *    esp32c3_timg0_timer_alarm_fired(), not an installed interrupt
 *    handler), and the T0_USE_XTAL bit is always set - the timer counts
 *    off the 40MHz crystal, not APB_CLK, specifically to avoid depending
 *    on this BSP's unconfirmed boot-time APB frequency (see
 *    ../upstream-spi-driver/README.md's SPI2_SOURCE_CLK_HZ caveat, which
 *    does not apply here for that reason - though the 40MHz *crystal*
 *    figure itself is the same "typical ESP32-C3 board" assumption as
 *    that driver's fallback).
 *  - Watchdog: single-stage only (stage 0 configurable; stages 1-3 are
 *    always left off) with a fixed /1 MWDT prescaler - see timg.h's
 *    esp32c3_timg0_wdt_enable() for the resulting timeout range. No
 *    interrupt handler is installed for ESP32C3_WDT_INTERRUPT - the raw
 *    TIMG_WDT_INT status bit is left set (readable via TIMG_INT_RAW_TIMERS_REG)
 *    for now rather than this driver claiming to dispatch it anywhere.
 */

#include <bsp.h>
#include <bsp/utility.h>
#include <c3/timg-regs.h>
#include <c3/timg.h>

static bool esp32c3_timg0_timer_initialized = false;

static void esp32c3_timg_wdt_unlock( void )
{
  TIMG_REG( TIMG_WDTWPROTECT_REG ) = TIMG_WDT_WKEY;
}

rtems_status_code esp32c3_timg0_timer_init( uint32_t divider )
{
  if ( divider == 0 || divider > 0xffff ) {
    return RTEMS_INVALID_NUMBER;
  }

  TIMG_REG( TIMG_T0CONFIG_REG ) = TIMG_T0_EN
    | TIMG_T0_INCREASE
    | TIMG_T0_USE_XTAL
    | TIMG_T0_DIVIDER( divider );

  esp32c3_timg0_timer_initialized = true;

  return RTEMS_SUCCESSFUL;
}

uint64_t esp32c3_timg0_timer_read( void )
{
  uint32_t lo;
  uint32_t hi;

  /* Any write latches the live counter into T0LO_REG/T0HI_REG - they
   * don't free-run themselves, so lo/hi are always a consistent snapshot
   * (no separate low/high tearing race to worry about here). */
  TIMG_REG( TIMG_T0UPDATE_REG ) = 0;

  lo = TIMG_REG( TIMG_T0LO_REG );
  hi = TIMG_REG( TIMG_T0HI_REG ) & TIMG_T0HI_MASK;

  return ( (uint64_t) hi << 32 ) | lo;
}

rtems_status_code esp32c3_timg0_timer_set_alarm(
  uint64_t count,
  bool     auto_reload
)
{
  uint32_t config;

  if ( !esp32c3_timg0_timer_initialized ) {
    return RTEMS_NOT_CONFIGURED;
  }

  if ( ( count >> 32 ) & ~( (uint64_t) TIMG_T0HI_MASK ) ) {
    return RTEMS_INVALID_NUMBER;
  }

  TIMG_REG( TIMG_T0ALARMLO_REG ) = (uint32_t) count;
  TIMG_REG( TIMG_T0ALARMHI_REG ) = (uint32_t) ( count >> 32 );

  if ( auto_reload ) {
    /* Reload to 0 (not the current LOADLO/HI, which default to 0 and are
     * never set elsewhere in this driver) each time the alarm fires. */
    TIMG_REG( TIMG_T0LOADLO_REG ) = 0;
    TIMG_REG( TIMG_T0LOADHI_REG ) = 0;
  }

  config = TIMG_REG( TIMG_T0CONFIG_REG );
  config |= TIMG_T0_ALARM_EN;

  if ( auto_reload ) {
    config |= TIMG_T0_AUTORELOAD;
  } else {
    config &= ~TIMG_T0_AUTORELOAD;
  }

  TIMG_REG( TIMG_T0CONFIG_REG ) = config;

  return RTEMS_SUCCESSFUL;
}

bool esp32c3_timg0_timer_alarm_fired( void )
{
  uint32_t raw = TIMG_REG( TIMG_INT_RAW_TIMERS_REG );

  if ( ( raw & TIMG_T0_INT ) == 0 ) {
    return false;
  }

  TIMG_REG( TIMG_INT_CLR_TIMERS_REG ) = TIMG_T0_INT;

  return true;
}

rtems_status_code esp32c3_timg0_wdt_enable(
  uint32_t            timeout_ms,
  esp32c3_wdt_action  action
)
{
  uint64_t cycles;
  uint32_t config;

  if (
    timeout_ms == 0
      || ( action != ESP32C3_WDT_INTERRUPT
        && action != ESP32C3_WDT_RESET_CPU
        && action != ESP32C3_WDT_RESET_SYSTEM )
  ) {
    return RTEMS_INVALID_NUMBER;
  }

  /* /1 prescaler (see TIMG_WDTCONFIG1_REG below): MWDT clock = 40MHz
   * (XTAL, per TIMG_WDT_USE_XTAL set below - see timg-regs.h) = 40000
   * cycles/ms. Reject rather than silently clamp/overflow if the
   * requested timeout doesn't fit the 32-bit stage-0 cycle count. */
  cycles = (uint64_t) timeout_ms * ( TIMG_WDT_REF_CLK_HZ / 1000U );
  if ( cycles > 0xffffffffU ) {
    return RTEMS_INVALID_NUMBER;
  }

  esp32c3_timg_wdt_unlock();

  TIMG_REG( TIMG_WDTCONFIG1_REG ) = TIMG_WDT_CLK_PRESCALE( 1 );
  TIMG_REG( TIMG_WDTCONFIG2_REG ) = (uint32_t) cycles; /* stage 0 timeout */

  config = TIMG_WDT_EN
    | TIMG_WDT_STG0( action )
    | TIMG_WDT_STG1( TIMG_WDT_STG_OFF )
    | TIMG_WDT_STG2( TIMG_WDT_STG_OFF )
    | TIMG_WDT_STG3( TIMG_WDT_STG_OFF )
    | TIMG_WDT_USE_XTAL; /* same rationale as the general timer, above */
  TIMG_REG( TIMG_WDTCONFIG0_REG ) = config;

  TIMG_REG( TIMG_WDTCONFIG0_REG ) |= TIMG_WDT_CONF_UPDATE_EN;
  TIMG_REG( TIMG_WDTFEED_REG ) = 0; /* start the countdown fresh */

  return RTEMS_SUCCESSFUL;
}

void esp32c3_timg0_wdt_feed( void )
{
  esp32c3_timg_wdt_unlock();
  TIMG_REG( TIMG_WDTFEED_REG ) = 0;
}

void esp32c3_timg0_wdt_disable( void )
{
  esp32c3_timg_wdt_unlock();

  TIMG_REG( TIMG_WDTCONFIG0_REG ) &= ~TIMG_WDT_EN;
  TIMG_REG( TIMG_WDTCONFIG0_REG ) |= TIMG_WDT_CONF_UPDATE_EN;
}
