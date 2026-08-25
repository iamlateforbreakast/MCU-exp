/*
 * Subset of ESP-IDF's esp_intr_alloc.h needed by bt.c at IDF v5.3.1.
 * Signatures (esp_intr_alloc/esp_intr_free, intr_handler_t, the source/
 * flags/handler/arg/ret_handle parameter order) confirmed against real IDF
 * v5.3.1 source (components/esp_hw_support/include/esp_intr_alloc.h,
 * esp_intr_types.h) this session - see ../../README.md.
 *
 * `source` is the ESP32-C3 interrupt-matrix source number (an
 * `interrupt_source_t` value from IDF's components/soc/esp32c3/include/soc/
 * interrupts.h) - bt.c never names it directly (see README's note on
 * osi_funcs_t): it's supplied at runtime by the closed libbtdm_app.a blob
 * through the interrupt_alloc callback it's handed. This header doesn't
 * define BT's specific source number - that's a BSP-side chip_definitions.h/
 * irq_mappings[] patch, explicitly not part of this session's deliverable
 * (see README's Phase 2 section).
 */
#ifndef FREERTOS_COMPAT_ESP_INTR_ALLOC_H
#define FREERTOS_COMPAT_ESP_INTR_ALLOC_H

#include "esp_err.h"

typedef void (*intr_handler_t)(void *arg);
typedef struct intr_handle_data_s *intr_handle_t;

/*
 * Flag bit values are this shim's own, NOT re-confirmed against real IDF's
 * numeric encoding this session. Safe: bt.c only ORs these together and
 * passes them opaquely through to esp_intr_alloc() - it never inspects the
 * bits itself, and this shim's own esp_intr_alloc() (src/esp_intr_alloc.c)
 * doesn't currently act on them either (RTEMS's rtems_interrupt_handler_install
 * has no separate priority-level/IRAM-residency concept to map them onto).
 */
#define ESP_INTR_FLAG_LEVEL3 (1 << 0)
#define ESP_INTR_FLAG_IRAM   (1 << 1)

esp_err_t esp_intr_alloc(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t *ret_handle);
esp_err_t esp_intr_free(intr_handle_t handle);
esp_err_t esp_intr_enable(intr_handle_t handle);
esp_err_t esp_intr_disable(intr_handle_t handle);

#endif /* FREERTOS_COMPAT_ESP_INTR_ALLOC_H */
