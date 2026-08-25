/*
 * esp_timer_create/start_once/stop/delete on top of RTEMS's Timer Server
 * API (rtems_timer_initiate_server + rtems_timer_server_fire_after),
 * confirmed against real RTEMS `main` source in Phase 2's recon - see
 * ../../README.md for why the server variant is used instead of plain
 * rtems_timer_fire_after (callback context: task vs. clock-tick ISR).
 *
 * `rtems_timer_service_routine_entry`'s exact parameter list
 * (`void (*)(rtems_id, void *)`) is RTEMS's long-standing documented
 * convention but was NOT independently re-grepped against source this
 * session - re-check before trusting esp_timer_trampoline's signature.
 */
#include "esp_timer.h"
#include <rtems/rtems/timer.h>
#include <rtems/rtems/clock.h>
#include <stdlib.h>

struct esp_timer {
    rtems_id       id;
    esp_timer_cb_t callback;
    void          *arg;
};

/*
 * The timer server must be started exactly once, globally, before any
 * *_server_fire_after() call. Guarded so a second esp_timer_create() (or a
 * retry after an earlier failure) doesn't re-attempt it -
 * rtems_timer_initiate_server() itself returns RTEMS_INCORRECT_STATE on a
 * second call, which this treats as "already running", not an error.
 */
static bool s_timer_server_started = false;

static esp_err_t ensure_timer_server_started(void)
{
    if (s_timer_server_started) {
        return ESP_OK;
    }
    rtems_status_code sc = rtems_timer_initiate_server(
        RTEMS_TIMER_SERVER_DEFAULT_PRIORITY,
        4096, /* stack size, bytes - not sized against real bt.c timer callback needs yet */
        RTEMS_DEFAULT_ATTRIBUTES
    );
    if (sc != RTEMS_SUCCESSFUL && sc != RTEMS_INCORRECT_STATE) {
        return ESP_FAIL;
    }
    s_timer_server_started = true;
    return ESP_OK;
}

static rtems_timer_service_routine esp_timer_trampoline(rtems_id id, void *arg)
{
    (void) id;
    esp_timer_handle_t timer = arg;
    timer->callback(timer->arg);
}

esp_err_t esp_timer_create(const esp_timer_create_args_t *create_args, esp_timer_handle_t *out_handle)
{
    if (create_args == NULL || out_handle == NULL) {
        return ESP_FAIL;
    }
    if (create_args->dispatch_method != ESP_TIMER_TASK) {
        /* ESP_TIMER_ISR dispatch isn't supported - RTEMS's server variant
         * always dispatches from the timer server task. bt.c is expected
         * to only use the default (ESP_TIMER_TASK), per Phase 0's grep of
         * its esp_timer_* call sites, but this isn't independently
         * re-confirmed against the create_args it actually passes. */
        return ESP_FAIL;
    }

    esp_err_t err = ensure_timer_server_started();
    if (err != ESP_OK) {
        return err;
    }

    esp_timer_handle_t timer = malloc(sizeof(*timer));
    if (timer == NULL) {
        return ESP_FAIL;
    }
    timer->callback = create_args->callback;
    timer->arg      = create_args->arg;

    rtems_status_code sc = rtems_timer_create(rtems_build_name('e', 's', 'p', 't'), &timer->id);
    if (sc != RTEMS_SUCCESSFUL) {
        free(timer);
        return ESP_FAIL;
    }

    *out_handle = timer;
    return ESP_OK;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us)
{
    if (timer == NULL) {
        return ESP_FAIL;
    }

    /* timeout_us is microseconds (esp_timer's unit); RTEMS ticks are
     * whatever CONFIGURE_MICROSECONDS_PER_TICK resolves to (100Hz/10ms by
     * default per Phase 1's recon) - read the real rate at runtime rather
     * than assume it, and round up so a short timeout never fires early. */
    uint32_t ticks_per_sec = rtems_clock_get_ticks_per_second();
    uint64_t ticks64 = (timeout_us * ticks_per_sec + 999999ULL) / 1000000ULL;
    rtems_interval ticks = (ticks64 < 1) ? 1 : (rtems_interval) ticks64;

    rtems_status_code sc = rtems_timer_server_fire_after(
        timer->id, ticks, esp_timer_trampoline, timer
    );
    return (sc == RTEMS_SUCCESSFUL) ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_timer_stop(esp_timer_handle_t timer)
{
    if (timer == NULL) {
        return ESP_FAIL;
    }
    rtems_status_code sc = rtems_timer_cancel(timer->id);
    return (sc == RTEMS_SUCCESSFUL) ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_timer_delete(esp_timer_handle_t timer)
{
    if (timer == NULL) {
        return ESP_FAIL;
    }
    rtems_status_code sc = rtems_timer_delete(timer->id);
    free(timer);
    return (sc == RTEMS_SUCCESSFUL) ? ESP_OK : ESP_FAIL;
}
