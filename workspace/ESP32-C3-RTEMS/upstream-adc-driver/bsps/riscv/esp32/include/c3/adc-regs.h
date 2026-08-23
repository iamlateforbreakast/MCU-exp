/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsRISCVESP32
 *
 * @brief SAR ADC (APB_SARADC / ADC1) register/field definitions for the
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
 * DRAFT - see upstream-adc-driver/README.md (two directories up from this
 * file's eventual bsps/riscv/esp32/include/c3/ home) for full status.
 *
 * Register offsets and bit positions below were checked directly against
 * Espressif's public components/soc/esp32c3/register/soc/apb_saradc_reg.h
 * while drafting this. The *sequence* of register operations required to
 * actually take a one-shot reading (which fields to write together, what
 * order, which bit to poll, how to clear it) came from a second,
 * independent source: Espressif's esp-hal Rust driver
 * (esp-hal/src/analog/adc/riscv.rs in github.com/esp-rs/esp-hal), a real,
 * actively maintained, presumably hardware-tested implementation - not
 * reconstructed from the bare register header alone, which mostly lacks
 * field descriptions for this peripheral. See esp32c3-adc.c's comments for
 * exactly which parts came from there.
 */

#ifndef LIBBSP_ESP32_C3_ADC_REGS_H
#define LIBBSP_ESP32_C3_ADC_REGS_H

#ifndef ASM

#include <bsp.h>
#include <bsp/utility.h>

#endif /* ASM */

#define ADC_BASE ( (uintptr_t) 0x60040000U )

#define ADC_REG( reg ) *( (volatile uint32_t *) ( ADC_BASE + ( reg ) ) )

#define ADC_CTRL_REG         0x00
#define ADC_XPD_SAR_FORCE( v ) BSP_FLD32( v, 27, 28 )
#define ADC_SAR_CLK_GATED     BSP_BIT32( 6 )
#define ADC_START             BSP_BIT32( 1 )
#define ADC_START_FORCE       BSP_BIT32( 0 )

#define ADC_ONETIME_SAMPLE_REG    0x20
#define ADC1_ONETIME_SAMPLE       BSP_BIT32( 31 )
#define ADC_ONETIME_START         BSP_BIT32( 29 )
#define ADC_ONETIME_CHANNEL( v )  BSP_FLD32( v, 25, 28 )
#define ADC_ONETIME_ATTEN( v )    BSP_FLD32( v, 23, 24 )

#define ADC1_DATA_STATUS_REG 0x2c
#define ADC1_DATA_MASK       0xfffU /* the low 12 bits of a wider status
                                      * field are the actual raw code - see
                                      * esp32c3-adc.c */

#define ADC_INT_RAW_REG  0x44
#define ADC_INT_CLR_REG  0x4c
#define ADC1_DONE_INT    BSP_BIT32( 31 )

#endif /* LIBBSP_ESP32_C3_ADC_REGS_H */
