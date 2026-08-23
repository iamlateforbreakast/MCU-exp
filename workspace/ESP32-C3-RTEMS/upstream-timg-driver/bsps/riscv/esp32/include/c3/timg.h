/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief Public API for the ESP32-C3 TIMG0 general-purpose timer and
 *   watchdog driver (esp32c3-timg.c).
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
 * DRAFT - like ../../../../upstream-adc-driver/, this is an original API
 * for this driver, not an implementation of an existing RTEMS convention:
 * RTEMS has no generic hardware-timer or watchdog driver framework (same
 * check as for ADC: no cpukit/include/dev/{timer,watchdog}, nothing
 * comparable in any BSP). See esp32c3-timg.c's file header and
 * ../../../../upstream-timg-driver/README.md for scope and confidence
 * details - notably, this one didn't need a second cross-check source the
 * way the ADC driver needed esp-hal, since Espressif's own register header
 * is fully documented for this peripheral.
 *
 * Two independent pieces of functionality, both on TIMG0:
 *  - A 54-bit general-purpose hardware timer (T0) - read the free-running
 *    count, or set a one-shot/auto-reload alarm. Independent of the
 *    SYSTIMER the clock driver uses for the RTOS tick; RTEMS applications
 *    needing a plain periodic/one-shot software callback should generally
 *    prefer the Classic API's Timer Manager (rtems_timer_fire_after() and
 *    friends), which already sits on top of that tick - this is for cases
 *    that specifically need the hardware (pulse timing, an alarm that must
 *    survive independent of task scheduling, etc).
 *  - A hardware watchdog. bsp_start() (bspstart.c) already neutralizes
 *    TIMG0's watchdog (and the RTC one) at boot, so calling
 *    esp32c3_timg0_wdt_enable() is what actually turns a real, configurable
 *    reset watchdog on for the first time in this BSP - see the README.
 */

#ifndef LIBBSP_ESP32_C3_TIMG_H
#define LIBBSP_ESP32_C3_TIMG_H

#include <rtems.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief What the watchdog does when it times out without being fed.
 *   Configures TIMG0's watchdog stage 0 only - stages 1-3 are always left
 *   off in this driver (see the README for what a multi-stage escalation
 *   policy - e.g. interrupt first, reset later - would need).
 */
typedef enum {
  ESP32C3_WDT_INTERRUPT = 1,
  ESP32C3_WDT_RESET_CPU = 2,
  ESP32C3_WDT_RESET_SYSTEM = 3
} esp32c3_wdt_action;

/**
 * @brief Initializes and starts TIMG0's general-purpose timer (T0) as a
 *   free-running up-counter.
 *
 * @param[in] divider Hardware clock divider, 1-65535. The timer counts at
 *   40MHz / @a divider (see the README for why 40MHz - the XTAL, not the
 *   unconfirmed APB clock the SPI/I2C drivers had to caveat).
 *
 * @retval RTEMS_SUCCESSFUL Successfully started.
 * @retval RTEMS_INVALID_NUMBER @a divider is 0.
 */
rtems_status_code esp32c3_timg0_timer_init( uint32_t divider );

/**
 * @brief Reads the current 54-bit counter value.
 */
uint64_t esp32c3_timg0_timer_read( void );

/**
 * @brief Sets (or replaces) the alarm value. esp32c3_timg0_timer_init()
 *   must have been called first.
 *
 * @param[in] count The counter value to alarm at.
 * @param[in] auto_reload If true, the counter reloads to 0 and the alarm
 *   re-arms automatically each time it fires (a repeating alarm every
 *   @a count ticks); if false, it fires once.
 */
rtems_status_code esp32c3_timg0_timer_set_alarm(
  uint64_t count,
  bool     auto_reload
);

/**
 * @brief Polls whether the alarm has fired since the last call, clearing
 *   it if so.
 *
 * @retval true The alarm fired (and has now been cleared).
 * @retval false It has not.
 */
bool esp32c3_timg0_timer_alarm_fired( void );

/**
 * @brief Enables TIMG0's watchdog with a single timeout stage. Reverses
 *   bsp_start()'s boot-time disable of this same watchdog - see the
 *   README for what state it's in before this is called.
 *
 * @param[in] timeout_ms Time without a esp32c3_timg0_wdt_feed() call
 *   before @a action triggers. Valid range, given the 40MHz MWDT clock
 *   (XTAL, not APB_CLK - see timg-regs.h) and this driver's fixed /1
 *   prescaler (see esp32c3-timg.c): 1 to 107374ms (~107.4s, where
 *   0xFFFFFFFF cycles at 40MHz runs out) - out-of-range values are
 *   rejected, not clamped.
 * @param[in] action What happens on timeout.
 *
 * @retval RTEMS_SUCCESSFUL Watchdog enabled.
 * @retval RTEMS_INVALID_NUMBER @a timeout_ms is 0 or out of range, or
 *   @a action isn't one of the esp32c3_wdt_action values.
 */
rtems_status_code esp32c3_timg0_wdt_enable(
  uint32_t            timeout_ms,
  esp32c3_wdt_action  action
);

/**
 * @brief Resets the watchdog countdown. Must be called more often than the
 *   configured timeout or @a action (from esp32c3_timg0_wdt_enable())
 *   triggers.
 */
void esp32c3_timg0_wdt_feed( void );

/**
 * @brief Disables the watchdog.
 */
void esp32c3_timg0_wdt_disable( void );

#ifdef __cplusplus
}
#endif

#endif /* LIBBSP_ESP32_C3_TIMG_H */
