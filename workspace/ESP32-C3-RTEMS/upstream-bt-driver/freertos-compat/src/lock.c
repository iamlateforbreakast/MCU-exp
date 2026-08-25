/*
 * newlib's standard retargetable-locking API (`_lock_t`/`_lock_acquire`/
 * `_lock_release`/etc.) - declared by `../include/sys/lock.h` in this
 * shim, NOT by the toolchain's own `<sys/lock.h>`. PHY init needs this:
 * `esp_phy_enable()`'s `s_phy_access_lock` (real ESP-IDF
 * `components/esp_phy/src/phy_init.c`) is a plain `_lock_t` used via
 * `_lock_acquire`/`_lock_release`.
 *
 * Confirmed 2026-08-25 (compiling against the real riscv-rtems7-gcc +
 * installed esp32c3db BSP in esp32c3-rtems-dev): this toolchain's real
 * `<sys/lock.h>` does NOT declare `_lock_t`/`_lock_acquire()` at all - it
 * implements a different, RTEMS-specific mechanism instead (`_LOCK_T`/
 * `__lock_acquire` macros over `struct _Mutex_Control`), confirming the
 * risk this comment used to flag as unverified. So this shim now ships
 * its own `sys/lock.h` (see that file's header comment for why shadowing
 * the toolchain's version is safe here) rather than relying on the
 * toolchain to already provide the type this file implements against.
 *
 * Confirmed this session: RTEMS's own cpukit does NOT implement this API
 * (grepped `_lock_acquire`/`_RETARGETABLE_LOCKING` across
 * `cpukit/libcsupport` and `cpukit/include` - zero hits, only RTEMS's own
 * differently-named `rtems_interrupt_lock_acquire`/
 * `rtems_termios_device_lock_acquire`), so this shim is genuinely needed,
 * not redundant with something RTEMS already provides.
 *
 * NOT confirmed: whether real ESP-IDF code explicitly calls
 * `_lock_init()` on statically-declared locks like `s_phy_access_lock`
 * (`static _lock_t s_phy_access_lock;`, presumably zero/NULL-initialized)
 * before first use, or relies on newlib's common lazy-init-on-first-acquire
 * convention for a NULL lock. `_lock_acquire`/`_lock_try_acquire` below
 * lazily initialize a NULL lock rather than silently no-op on it, to be
 * correct either way - silently skipping the lock on NULL would be a real
 * bug if the lazy-init convention is what's actually relied on, not just
 * an unconfirmed assumption. The lazy-init itself isn't concurrency-safe
 * against two simultaneous first-acquires (a second, harder problem
 * newlib's own reference implementations also have to solve) - acceptable
 * here since PHY init's own call pattern is expected to be single-threaded
 * at that point, but flagged rather than silently assumed safe.
 */
#include <sys/lock.h>
#include <rtems/rtems/sem.h>
#include <rtems/rtems/object.h>
#include <stdlib.h>

/*
 * `_LOCK_T` is an opaque pointer type (`struct __lock *`) per newlib's
 * convention - point it at our own small struct rather than newlib's
 * platform-specific `struct __lock` layout (that layout, per ESP-IDF's own
 * `sys/lock.h`, is sized to fit a FreeRTOS mutex - irrelevant here since
 * we define what `struct __lock` contains for this build).
 */
struct __lock {
    rtems_id sem;
};

static void lock_init_common(_lock_t *plock, rtems_attribute extra_attributes)
{
    struct __lock *lock = malloc(sizeof(*lock));
    if (lock == NULL) {
        *plock = NULL;
        return;
    }
    rtems_status_code sc = rtems_semaphore_create(
        rtems_build_name('l', 'o', 'c', 'k'),
        1,
        RTEMS_BINARY_SEMAPHORE | extra_attributes,
        0,
        &lock->sem
    );
    if (sc != RTEMS_SUCCESSFUL) {
        free(lock);
        *plock = NULL;
        return;
    }
    *plock = lock;
}

void _lock_init(_lock_t *plock)
{
    lock_init_common(plock, RTEMS_PRIORITY);
}

void _lock_init_recursive(_lock_t *plock)
{
    /* Priority-inheritance binary semaphore isn't itself recursive in
     * RTEMS - nesting support would need an owner+count wrapper. Not
     * implemented: no confirmed call site needs the recursive variant
     * (only plain _lock_acquire/_lock_release, used by phy_init.c's
     * s_phy_access_lock, were confirmed this session). */
    lock_init_common(plock, RTEMS_PRIORITY | RTEMS_INHERIT_PRIORITY);
}

void _lock_close(_lock_t *plock)
{
    if (plock == NULL || *plock == NULL) {
        return;
    }
    rtems_semaphore_delete((*plock)->sem);
    free(*plock);
    *plock = NULL;
}

void _lock_close_recursive(_lock_t *plock)
{
    _lock_close(plock);
}

void _lock_acquire(_lock_t *plock)
{
    if (plock == NULL) {
        return;
    }
    if (*plock == NULL) {
        _lock_init(plock);
        if (*plock == NULL) {
            return;
        }
    }
    rtems_semaphore_obtain((*plock)->sem, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
}

void _lock_acquire_recursive(_lock_t *plock)
{
    _lock_acquire(plock);
}

int _lock_try_acquire(_lock_t *plock)
{
    if (plock == NULL) {
        return -1;
    }
    if (*plock == NULL) {
        _lock_init(plock);
        if (*plock == NULL) {
            return -1;
        }
    }
    return (rtems_semaphore_obtain((*plock)->sem, RTEMS_NO_WAIT, RTEMS_NO_TIMEOUT) == RTEMS_SUCCESSFUL) ? 0 : -1;
}

int _lock_try_acquire_recursive(_lock_t *plock)
{
    return _lock_try_acquire(plock);
}

void _lock_release(_lock_t *plock)
{
    if (plock == NULL || *plock == NULL) {
        return;
    }
    rtems_semaphore_release((*plock)->sem);
}

void _lock_release_recursive(_lock_t *plock)
{
    _lock_release(plock);
}
