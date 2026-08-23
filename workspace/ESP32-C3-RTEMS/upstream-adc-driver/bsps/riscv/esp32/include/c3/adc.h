/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief Public API for the ESP32-C3 ADC1 one-shot driver
 *   (esp32c3-adc.c).
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
 * DRAFT - this is an original API for this driver, not an implementation
 * of an existing RTEMS convention: unlike GPIO/SPI/I2C, RTEMS has no
 * generic ADC driver framework to target. See esp32c3-adc.c's file header
 * and ../../../../upstream-adc-driver/README.md for why, and for this
 * driver's scope (ADC1 only, raw codes only, polled one-shot only).
 */

#ifndef LIBBSP_ESP32_C3_ADC_H
#define LIBBSP_ESP32_C3_ADC_H

#include <rtems.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ADC1 channel numbers, straight from the ESP32-C3 pinout (well-known,
 * public reference information - not from a fetched register header):
 * ADC1 has exactly 5 channels, mapped 1:1 to GPIO0-GPIO4. There is no
 * ADC2 channel support in this driver - see the README for why.
 */
#define ADC1_CHANNEL_GPIO0 0
#define ADC1_CHANNEL_GPIO1 1
#define ADC1_CHANNEL_GPIO2 2
#define ADC1_CHANNEL_GPIO3 3
#define ADC1_CHANNEL_GPIO4 4

/*
 * Attenuation select - the input attenuator changes the usable input
 * voltage range at the cost of accuracy/linearity, same enumeration and
 * approximate nominal full-scale voltages as ESP-IDF's adc_atten_t
 * (uncalibrated - see the README before treating these as accurate):
 *   ADC_ATTEN_0DB   - approx. 0 - 1.1V
 *   ADC_ATTEN_2_5DB - approx. 0 - 1.5V
 *   ADC_ATTEN_6DB   - approx. 0 - 2.2V
 *   ADC_ATTEN_11DB  - approx. 0 - 3.9V (clamped in practice below Vdd)
 */
#define ADC_ATTEN_0DB   0
#define ADC_ATTEN_2_5DB 1
#define ADC_ATTEN_6DB   2
#define ADC_ATTEN_11DB  3

/**
 * @brief Initializes the ADC1 peripheral. Must be called once, from a
 *   single task, before esp32c3_adc1_read_raw().
 *
 * @retval RTEMS_SUCCESSFUL Always, in this draft.
 */
rtems_status_code esp32c3_adc1_init( void );

/**
 * @brief Takes one polled one-shot ADC1 reading.
 *
 * @param[in] channel One of the ADC1_CHANNEL_GPIOn constants above.
 * @param[in] atten One of the ADC_ATTEN_* constants above.
 * @param[out] raw The raw 12-bit conversion result (0-4095), uncalibrated -
 *   see the README for approximate nominal voltage ranges per attenuation.
 *
 * @retval RTEMS_SUCCESSFUL Reading taken successfully.
 * @retval RTEMS_INVALID_NUMBER @a raw is NULL, or @a channel/@a atten is
 *   out of range.
 * @retval RTEMS_NOT_CONFIGURED esp32c3_adc1_init() has not been called.
 */
rtems_status_code esp32c3_adc1_read_raw(
  uint32_t  channel,
  uint32_t  atten,
  uint16_t *raw
);

#ifdef __cplusplus
}
#endif

#endif /* LIBBSP_ESP32_C3_ADC_H */
