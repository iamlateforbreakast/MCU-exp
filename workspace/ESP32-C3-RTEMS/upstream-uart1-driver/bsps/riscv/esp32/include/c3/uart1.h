/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief Public API for the ESP32-C3 UART1 Termios driver
 *   (esp32c3-uart1.c).
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
 * DRAFT - unlike the ADC/TIMG drivers, this one *does* implement an
 * established RTEMS framework (Termios device drivers, <rtems/
 * termiosdevice.h> and <rtems/termiostypes.h> - the same one this BSP's
 * own console driver would use if it went through it, though
 * console-config.c actually takes a simpler shortcut - see
 * esp32c3-uart1.c's file header and ../../../../upstream-uart1-driver/
 * README.md for details and scope).
 */

#ifndef LIBBSP_ESP32_C3_UART1_H
#define LIBBSP_ESP32_C3_UART1_H

#include <rtems.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Installs UART1 as a Termios device, routing its TX/RX signals to
 *   the given GPIO pins through the GPIO matrix.
 *
 * After a successful call, @a device_file behaves like a normal serial
 * device file: open(), read(), write(), and tcsetattr()/tcgetattr() for
 * baud rate, parity, stop bits and data bits (see esp32c3-uart1.c for
 * exactly which termios settings are honored in this draft).
 *
 * @param[in] device_file The device file path, e.g. "/dev/ttyS1".
 * @param[in] tx_pin GPIO number to use for UART1 TXD (output).
 * @param[in] rx_pin GPIO number to use for UART1 RXD (input).
 *
 * @retval RTEMS_SUCCESSFUL Device installed successfully.
 * @retval * @see rtems_termios_device_install().
 */
rtems_status_code esp32c3_uart1_install(
  const char *device_file,
  uint32_t    tx_pin,
  uint32_t    rx_pin
);

#ifdef __cplusplus
}
#endif

#endif /* LIBBSP_ESP32_C3_UART1_H */
