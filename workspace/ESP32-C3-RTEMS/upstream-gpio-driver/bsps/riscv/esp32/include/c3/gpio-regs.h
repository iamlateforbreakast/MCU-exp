/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief GPIO controller and IO_MUX register/field definitions for the
 *   ESP32-C3 (esp32c3db BSP), plus the BSP_GPIO_PIN_COUNT and
 *   BSP_GPIO_PINS_PER_BANK constants required by <bsp/gpio.h>.
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
 * DRAFT / UNVERIFIED - see upstream-gpio-driver/README.md (two directories
 * up from this file's eventual bsps/riscv/esp32/include/c3/ home) for full
 * status.
 *
 * GPIO_BASE, IO_MUX_BASE and the plain GPIO_* register offsets below follow
 * the same 0x6000_4000/0x6000_9000-region layout as the already-confirmed
 * SYSTIMER_BASE/RTC_CNTL_BASE/USB_SERIAL_JTAG_BASE addresses already in
 * this BSP's c3/chip_definitions.h.
 *
 * The IO_MUX_* per-pin field positions (FUN_IE/FUN_WPU/FUN_WPD/MCU_SEL) are
 * recalled from the general ESP32-family IO_MUX layout and are NOT
 * confirmed against the ESP32-C3 Technical Reference Manual specifically -
 * cross-check them against the TRM chapter "IO MUX and GPIO Matrix" (or
 * ESP-IDF's soc/gpio_reg.h / soc/io_mux_reg.h for esp32c3) before relying
 * on them against real hardware.
 */

#ifndef LIBBSP_ESP32_C3_GPIO_REGS_H
#define LIBBSP_ESP32_C3_GPIO_REGS_H

#ifndef ASM

#include <bsp.h>
#include <bsp/utility.h>

#endif /* ASM */

/*
 * The ESP32-C3 has a single GPIO bank (GPIO0-GPIO21, 22 pins) that fits in
 * one 32-bit register, unlike the original ESP32's two 32-pin banks. With
 * BSP_GPIO_PINS_PER_BANK > BSP_GPIO_PIN_COUNT, <bsp/gpio.h> always computes
 * bank == 0, so every rtems_gpio_bsp_*(bank, pin, ...) callback below
 * ignores bank and treats pin as the GPIO number directly.
 *
 * Several of these 22 pins have fixed roles on typical modules and should
 * not be blindly requested as general-purpose I/O - this driver does not
 * special-case any of them, the caller must pick pins appropriate for the
 * target board:
 *   GPIO2, GPIO8, GPIO9  - strapping pins, sampled at reset
 *   GPIO11-GPIO17        - SPI0/SPI1 to in-package/external flash on most
 *                          modules
 *   GPIO18, GPIO19       - USB D-/D+ when the USB-Serial/JTAG peripheral
 *                          (the default console in this BSP) is in use
 *   GPIO20, GPIO21       - UART0 RX/TX in the default pin configuration
 */
#define BSP_GPIO_PIN_COUNT     22
#define BSP_GPIO_PINS_PER_BANK 32

#define GPIO_BASE   ( (uintptr_t) 0x60004000U )
#define IO_MUX_BASE ( (uintptr_t) 0x60009000U )

#define GPIO_REG( reg )   *( (volatile uint32_t *) ( GPIO_BASE + ( reg ) ) )
#define IO_MUX_REG( reg ) *( (volatile uint32_t *) ( IO_MUX_BASE + ( reg ) ) )

/* Output level (1 = high), direction (1 = output) and input level. */
#define GPIO_OUT_REG          0x0004
#define GPIO_OUT_W1TS_REG     0x0008
#define GPIO_OUT_W1TC_REG     0x000c
#define GPIO_ENABLE_REG       0x0020
#define GPIO_ENABLE_W1TS_REG  0x0024
#define GPIO_ENABLE_W1TC_REG  0x0028
#define GPIO_IN_REG           0x003c

/* Interrupt status - write 1 to GPIO_STATUS_W1TC_REG to clear. */
#define GPIO_STATUS_REG      0x0044
#define GPIO_STATUS_W1TC_REG 0x004c

/*
 * Per-pin configuration register: GPIO_PINn_REG = GPIO_PIN0_REG + 4 * n.
 * INT_TYPE selects the interrupt trigger. The ESP32 GPIO controller has no
 * combined "both levels" trigger mode, so rtems_gpio_interrupt's
 * BOTH_LEVELS has no hardware equivalent here.
 */
#define GPIO_PIN0_REG        0x0074
#define GPIO_PIN_REG( pin )  ( GPIO_PIN0_REG + 4 * ( pin ) )

#define GPIO_PIN_INT_TYPE_MASK       BSP_FLD32( 0x7, 7, 9 )
#define GPIO_PIN_INT_TYPE( val )     BSP_FLD32( val, 7, 9 )
#define GPIO_PIN_INT_TYPE_GET( reg ) BSP_FLD32GET( reg, 7, 9 )

#define GPIO_PIN_INT_TYPE_DISABLE    0
#define GPIO_PIN_INT_TYPE_RISING     1
#define GPIO_PIN_INT_TYPE_FALLING    2
#define GPIO_PIN_INT_TYPE_ANY_EDGE   3
#define GPIO_PIN_INT_TYPE_LOW_LEVEL  4
#define GPIO_PIN_INT_TYPE_HIGH_LEVEL 5

/*
 * IO_MUX per-pin register: IO_MUX_GPIOn_REG = IO_MUX_GPIO0_REG + 4 * n.
 * UNVERIFIED - see file header.
 */
#define IO_MUX_GPIO0_REG       0x0004
#define IO_MUX_GPIO_REG( pin ) ( IO_MUX_GPIO0_REG + 4 * ( pin ) )

#define IO_MUX_FUN_WPD BSP_BIT32( 7 )
#define IO_MUX_FUN_WPU BSP_BIT32( 8 )
#define IO_MUX_FUN_IE  BSP_BIT32( 9 )

#define IO_MUX_MCU_SEL( val )     BSP_FLD32( val, 12, 14 )
#define IO_MUX_MCU_SEL_GET( reg ) BSP_FLD32GET( reg, 12, 14 )

/*
 * Function code that selects plain GPIO on most (not all - see the pin
 * list above) ESP32-C3 pads.
 */
#define IO_MUX_FUNCTION_GPIO 1

#endif /* LIBBSP_ESP32_C3_GPIO_REGS_H */
