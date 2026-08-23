/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief Polled, register-level ADC1 one-shot driver for the ESP32-C3
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
 * STATUS: draft - not yet built or tested against real hardware. See
 * upstream-adc-driver/README.md (two directories up from this file's
 * eventual bsps/riscv/esp32/adc/ home in the RTEMS tree) for what is and
 * isn't verified, and importantly, for why this file exposes its own small
 * API instead of implementing an established RTEMS framework - unlike the
 * companion GPIO/SPI/I2C drivers, RTEMS has no generic ADC driver API to
 * target (checked directly: no cpukit/include/dev/adc, no bsps/include/
 * dev/adc, and no BSP anywhere in RTEMS main currently has an "adc"
 * subdirectory).
 *
 * Scope of this first draft:
 *  - ADC1 only (5 channels, GPIO0-GPIO4). ADC2 is not implemented: on the
 *    wider ESP32 family ADC2 shares hardware with the WiFi radio and is
 *    documented as unreliable while WiFi is active - since a future
 *    version of this BSP will likely want WiFi, it isn't worth the extra
 *    surface area here.
 *  - Raw 12-bit codes only (0-4095), not calibrated millivolts. Real
 *    calibration on this chip goes through per-device eFuse trim values
 *    applied via an internal "REGI2C" analog-register-access mechanism
 *    (unrelated to the external I2C bus in ../upstream-i2c-driver/) that
 *    this draft does not implement - seemed like real scope creep for a
 *    first cut, and a wrong "calibrated" number that's actually still
 *    inaccurate is worse than an honestly-raw one. See the README for the
 *    nominal (uncalibrated) full-scale voltage per attenuation setting.
 *  - Polled - no interrupts, no continuous/pattern-table scanning mode
 *    (this driver only uses the "one-time sample" path).
 */

#include <bsp.h>
#include <bsp/utility.h>
#include <c3/adc-regs.h>
#include <c3/adc.h>

#include <rtems/thread.h>

#include <stdbool.h>

static rtems_mutex esp32c3_adc1_mutex;
static bool         esp32c3_adc1_initialized = false;

/*
 * Expected to be called once, from a single task (typically Init), before
 * any esp32c3_adc1_read_raw() call - like the companion GPIO/SPI/I2C
 * drivers' own *_register_esp32c3() entry points, concurrent calls to this
 * one specifically are not safe (the initialized-flag check below and the
 * mutex construction it guards are not themselves atomic).
 */
rtems_status_code esp32c3_adc1_init( void )
{
  if ( esp32c3_adc1_initialized ) {
    return RTEMS_SUCCESSFUL;
  }

  rtems_mutex_init( &esp32c3_adc1_mutex, "ESP32C3 ADC1" );

  /*
   * This exact sequence - forcing START/START_FORCE plus xpd_sar_force -
   * matches esp-hal's Adc::new() (see adc-regs.h's file header), not an
   * independent re-derivation from the register descriptions, which are
   * blank for most of APB_SARADC_CTRL_REG in Espressif's own header.
   */
  ADC_REG( ADC_CTRL_REG ) |= ADC_START_FORCE
    | ADC_START
    | ADC_SAR_CLK_GATED
    | ADC_XPD_SAR_FORCE( 0x3 );

  esp32c3_adc1_initialized = true;

  return RTEMS_SUCCESSFUL;
}

rtems_status_code esp32c3_adc1_read_raw(
  uint32_t  channel,
  uint32_t  atten,
  uint16_t *raw
)
{
  uint32_t status;

  if (
    raw == NULL
      || channel > ADC1_CHANNEL_GPIO4
      || atten > ADC_ATTEN_11DB
  ) {
    return RTEMS_INVALID_NUMBER;
  }

  if ( !esp32c3_adc1_initialized ) {
    return RTEMS_NOT_CONFIGURED;
  }

  rtems_mutex_lock( &esp32c3_adc1_mutex );

  /*
   * A plain assignment (not |=) is safe here specifically: every defined
   * bit in ADC_ONETIME_SAMPLE_REG is one this call means to set fresh
   * (unlike, e.g., I2C_CTR_REG in the companion I2C driver, which has
   * other bits that must be preserved) - see adc-regs.h, there's nothing
   * else meaningful in this register to clobber.
   *
   * Configuring the channel/attenuation and triggering the start are two
   * separate register writes, not combined into one - matching esp-hal's
   * config_onetime_sample()+start_onetime_sample() split rather than
   * assuming the hardware latches everything reliably from a single
   * write.
   */
  ADC_REG( ADC_ONETIME_SAMPLE_REG ) = ADC1_ONETIME_SAMPLE
    | ADC_ONETIME_CHANNEL( channel )
    | ADC_ONETIME_ATTEN( atten );

  ADC_REG( ADC_ONETIME_SAMPLE_REG ) |= ADC_ONETIME_START;

  do {
    status = ADC_REG( ADC_INT_RAW_REG );
  } while ( ( status & ADC1_DONE_INT ) == 0 );

  *raw = (uint16_t) ( ADC_REG( ADC1_DATA_STATUS_REG ) & ADC1_DATA_MASK );

  ADC_REG( ADC_INT_CLR_REG ) = ADC1_DONE_INT;
  ADC_REG( ADC_ONETIME_SAMPLE_REG ) &= ~ADC_ONETIME_START;

  rtems_mutex_unlock( &esp32c3_adc1_mutex );

  return RTEMS_SUCCESSFUL;
}
