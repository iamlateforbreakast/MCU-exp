/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief Registration entry point for the ESP32-C3 (esp32c3db BSP) GP-SPI2
 *   driver (bsps/riscv/esp32/spi/esp32c3-spi.c).
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

#ifndef LIBBSP_RISCV_ESP32_ESP32C3_SPI_H
#define LIBBSP_RISCV_ESP32_ESP32C3_SPI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Registers a GP-SPI2 master bus device at @p bus_path.
 *
 * Chip select is a plain GPIO toggled by the driver around each transfer -
 * see esp32c3-spi.c's file header for this first draft's scope (single
 * device, polled, 8 bits per word).
 *
 * @param sclk_pin GPIO routed to FSPICLK via the GPIO matrix.
 * @param mosi_pin GPIO routed to FSPID (master out).
 * @param miso_pin GPIO routed to FSPIQ (master in).
 * @param cs_pin GPIO toggled as chip select.
 */
int spi_bus_register_esp32c3(
  const char *bus_path,
  uint32_t    sclk_pin,
  uint32_t    mosi_pin,
  uint32_t    miso_pin,
  uint32_t    cs_pin
);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LIBBSP_RISCV_ESP32_ESP32C3_SPI_H */
