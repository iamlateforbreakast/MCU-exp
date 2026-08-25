/*
 * esp_intr_alloc/free/enable/disable on top of RTEMS's generic interrupt
 * API. `rtems_interrupt_handler_install`/`_remove` signatures were
 * confirmed against real RTEMS `main` source in Phase 2's recon (see
 * ../../README.md); `rtems_interrupt_vector_enable`/`_disable` (used below
 * for esp_intr_enable/disable) were NOT independently re-confirmed this
 * session - they're RTEMS's well-known public vector-enable API
 * (`cpukit/include/rtems/irq-extension.h`) but re-check the exact
 * signature before trusting this file.
 *
 * `source` (the ESP32-C3 interrupt-matrix number) is used directly as the
 * RTEMS vector number, matching how this BSP's own irq_mappings[] table
 * already works (e.g. GPIO_PROCPU_INTR=16 is both the matrix source number
 * and the RTEMS vector). This only actually succeeds once the BSP's
 * chip_definitions.h/irq_mappings[] table has an entry for whatever source
 * bt.c is allocating - not yet added (see README's Phase 2 section) - RTEMS
 * will simply reject unrecognized vectors at install time until it is.
 */
#include "esp_intr_alloc.h"
#include <rtems/rtems/intr.h>
#include <stdlib.h>

struct intr_handle_data_s {
    rtems_vector_number vector;
    rtems_interrupt_handler handler;
    void *arg;
};

esp_err_t esp_intr_alloc(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t *ret_handle)
{
    (void) flags; /* see header: not yet mapped onto anything RTEMS-side */

    intr_handle_t h = malloc(sizeof(*h));
    if (h == NULL) {
        return ESP_FAIL;
    }
    h->vector  = (rtems_vector_number) source;
    h->handler = (rtems_interrupt_handler) handler;
    h->arg     = arg;

    rtems_status_code sc = rtems_interrupt_handler_install(
        h->vector, "BT", RTEMS_INTERRUPT_UNIQUE, h->handler, h->arg
    );
    if (sc != RTEMS_SUCCESSFUL) {
        free(h);
        return ESP_FAIL;
    }

    if (ret_handle != NULL) {
        *ret_handle = h;
    }
    return ESP_OK;
}

esp_err_t esp_intr_free(intr_handle_t handle)
{
    if (handle == NULL) {
        return ESP_FAIL;
    }
    rtems_status_code sc = rtems_interrupt_handler_remove(handle->vector, handle->handler, handle->arg);
    free(handle);
    return (sc == RTEMS_SUCCESSFUL) ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_intr_enable(intr_handle_t handle)
{
    if (handle == NULL) {
        return ESP_FAIL;
    }
    rtems_status_code sc = rtems_interrupt_vector_enable(handle->vector);
    return (sc == RTEMS_SUCCESSFUL) ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_intr_disable(intr_handle_t handle)
{
    if (handle == NULL) {
        return ESP_FAIL;
    }
    rtems_status_code sc = rtems_interrupt_vector_disable(handle->vector);
    return (sc == RTEMS_SUCCESSFUL) ? ESP_OK : ESP_FAIL;
}
