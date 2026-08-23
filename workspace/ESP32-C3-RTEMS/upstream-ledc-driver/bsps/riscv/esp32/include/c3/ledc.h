/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief Public API for the ESP32-C3 LEDC (PWM) driver (esp32c3-ledc.c).
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
 * DRAFT - like the ADC/TIMG drivers, this is an original API for this
 * driver, not an implementation of an existing RTEMS convention: RTEMS
 * has no generic PWM driver framework (checked the same way as for ADC:
 * no cpukit/include/dev/pwm; other BSPs each define their own bespoke PWM
 * API - e.g. arm/atsam's libchip/include/pwmc.h - rather than a shared
 * one). See esp32c3-ledc.c's file header and
 * ../../../../upstream-ledc-driver/README.md for scope and confidence
 * details.
 *
 * Model: up to 4 independent timers, each defining a PWM frequency and
 * duty resolution; up to 6 channels, each bound to one timer and one GPIO
 * pin, with its own independently settable duty cycle. Several channels
 * can share one timer (same frequency, independent duty), which is the
 * normal way to drive, e.g., an RGB LED's three channels in sync.
 */

#ifndef LIBBSP_ESP32_C3_LEDC_H
#define LIBBSP_ESP32_C3_LEDC_H

#include <rtems.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configures a timer's PWM frequency and duty resolution.
 *
 * @param[in] timer Timer number, 0-3.
 * @param[in] freq_hz Desired PWM frequency in Hz.
 * @param[in] duty_resolution_bits Duty cycle resolution in bits, 1-14 (a
 *   channel using this timer's duty ranges 0 to
 *   (1 << duty_resolution_bits) - 1). Higher resolution means a lower
 *   maximum @a freq_hz for a given source clock - see esp32c3-ledc.c.
 *
 * @retval RTEMS_SUCCESSFUL Timer configured successfully.
 * @retval RTEMS_INVALID_NUMBER @a timer or @a duty_resolution_bits out of
 *   range, or @a freq_hz can't be reached (too high for the resulting
 *   clock divider to represent, or too low to fit the 18-bit divider
 *   field without overflowing).
 */
rtems_status_code esp32c3_ledc_timer_config(
  uint32_t timer,
  uint32_t freq_hz,
  uint32_t duty_resolution_bits
);

/**
 * @brief Binds a channel to a timer and a GPIO pin, and starts it at 0%
 *   duty. esp32c3_ledc_timer_config() must have been called for @a timer
 *   first.
 *
 * @param[in] channel Channel number, 0-5.
 * @param[in] timer The timer (0-3) this channel takes its frequency and
 *   duty resolution from.
 * @param[in] gpio GPIO pin number to output the PWM signal on.
 *
 * @retval RTEMS_SUCCESSFUL Channel configured successfully.
 * @retval RTEMS_INVALID_NUMBER @a channel or @a timer out of range.
 * @retval RTEMS_NOT_CONFIGURED @a timer has not been configured yet.
 */
rtems_status_code esp32c3_ledc_channel_config(
  uint32_t channel,
  uint32_t timer,
  uint32_t gpio
);

/**
 * @brief Sets a channel's duty cycle, applied immediately (no fade).
 *   esp32c3_ledc_channel_config() must have been called for @a channel
 *   first.
 *
 * @param[in] channel Channel number, 0-5.
 * @param[in] duty Duty value, 0 to (1 << duty_resolution_bits) - 1 for the
 *   timer this channel is bound to (0 = always off, the maximum value =
 *   always on).
 *
 * @retval RTEMS_SUCCESSFUL Duty updated successfully.
 * @retval RTEMS_INVALID_NUMBER @a channel out of range, or @a duty
 *   exceeds its timer's resolution.
 * @retval RTEMS_NOT_CONFIGURED @a channel has not been configured yet.
 */
rtems_status_code esp32c3_ledc_set_duty( uint32_t channel, uint32_t duty );

#ifdef __cplusplus
}
#endif

#endif /* LIBBSP_ESP32_C3_LEDC_H */
