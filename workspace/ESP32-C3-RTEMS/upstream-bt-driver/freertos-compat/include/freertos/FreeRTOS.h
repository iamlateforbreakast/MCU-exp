/*
 * Umbrella header, drop-in for ESP-IDF's own "freertos/FreeRTOS.h". See
 * ../../README.md for the full mapping table and open risks.
 */
#ifndef FREERTOS_COMPAT_FREERTOS_H
#define FREERTOS_COMPAT_FREERTOS_H

#include "freertos/portmacro.h"

typedef uint32_t configSTACK_DEPTH_TYPE;

/*
 * Ceiling used only to invert FreeRTOS priorities (0=lowest..N-1=highest)
 * into RTEMS priorities (1=highest..lower=lower) in src/task.c - not a
 * real FreeRTOS config option, just this shim's internal convention.
 * Matches ESP-IDF's default CONFIG_FREERTOS_MAX_PRIORITIES (25), rounded
 * up; bt.c's own task priorities must fall inside this range or task.c's
 * inversion will clamp them (see src/task.c comment).
 */
#define FREERTOS_COMPAT_MAX_PRIORITIES 32

#endif /* FREERTOS_COMPAT_FREERTOS_H */
