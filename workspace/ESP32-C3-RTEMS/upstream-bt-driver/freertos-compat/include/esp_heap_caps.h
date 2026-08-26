/*
 * `heap_caps_malloc`/`heap_caps_calloc`/`heap_caps_add_region` - real
 * signatures confirmed against ESP-IDF v5.3.1
 * `components/heap/include/esp_heap_caps.h`/`esp_heap_caps_init.h`.
 * `bt.c`'s only calls (grepped, 4 call sites) always OR together
 * `MALLOC_CAP_8BIT`/`MALLOC_CAP_INTERNAL`/`MALLOC_CAP_DMA` - real IDF's
 * multi-region capability-aware heap allocator (SPIRAM vs internal RAM,
 * DMA-capable regions, etc.) has no RTEMS equivalent and isn't vendored
 * here. Deliberate simplification, not a recon gap: `heap_caps_malloc`/
 * `_calloc` are implemented as plain `malloc`/`calloc` (RTEMS's C
 * library heap over the BSP workspace - one region, no capability
 * distinction on this chip's real memory map for what `bt.c` needs), and
 * `heap_caps_add_region` (used once, `bt.c:1020`, to register the BTDM
 * ROM data/bss range recovered via `ets_rom_layout_p` - see
 * `../../vendor/components/esp_rom/include/esp32c3/rom/rom_layout.h`) is
 * a no-op returning `ESP_OK`, since a plain single-heap allocator has no
 * "register an extra region" concept. Correctness risk flagged, not
 * hidden: if BTDM's real runtime memory footprint depends on that region
 * actually being added to the allocator (not just its bytes existing in
 * addressable RAM), this no-op would need revisiting once hardware
 * testing can actually observe an allocation failure.
 */
#ifndef _FREERTOS_COMPAT_ESP_HEAP_CAPS_H_
#define _FREERTOS_COMPAT_ESP_HEAP_CAPS_H_

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define MALLOC_CAP_EXEC       (1<<0)
#define MALLOC_CAP_32BIT      (1<<1)
#define MALLOC_CAP_8BIT       (1<<2)
#define MALLOC_CAP_DMA        (1<<3)
#define MALLOC_CAP_SPIRAM     (1<<10)
#define MALLOC_CAP_INTERNAL   (1<<11)
#define MALLOC_CAP_DEFAULT    (1<<12)
/* Real value from ESP-IDF `master`'s components/heap/include/
 * esp_heap_caps.h:43 - needed vendoring bt.c from master (2026-08-26,
 * see sdkconfig-compat.h's "Re-vendoring bt.c" note). "Retention DMA"
 * (deep-sleep memory retention) has no RTEMS equivalent here either -
 * same deliberate simplification as every other cap, ignored. */
#define MALLOC_CAP_RETENTION  (1<<14)

void *heap_caps_malloc(size_t size, uint32_t caps);
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps);
esp_err_t heap_caps_add_region(intptr_t start, intptr_t end);
/* Real signature from the same header, line 215. Backed by RTEMS's own
 * real `malloc_free_space()` (`rtems/libcsupport.h:89`) - a real,
 * non-approximated free-heap query, unlike the capability-ignoring
 * malloc/calloc above (this chip's flat memory map makes capability
 * distinctions moot for allocation *success*, but a real free-space
 * number is cheap to provide correctly). */
size_t heap_caps_get_free_size(uint32_t caps);

#endif /* _FREERTOS_COMPAT_ESP_HEAP_CAPS_H_ */
