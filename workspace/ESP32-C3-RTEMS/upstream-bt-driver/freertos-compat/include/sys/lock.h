/*
 * `_lock_t`/`_lock_acquire`/etc. - the FreeBSD-derived newlib
 * "retargetable locking" API real ESP-IDF code (`components/esp_phy/src/
 * phy_init.c`'s `s_phy_access_lock`) calls directly by including
 * `<sys/lock.h>`, expecting the toolchain to already declare it.
 *
 * Confirmed 2026-08-25 (compiling against the real riscv-rtems7-gcc +
 * installed esp32c3db BSP in esp32c3-rtems-dev): this toolchain's own
 * `<sys/lock.h>` does NOT declare this API at all - it implements a
 * completely different, RTEMS-specific mechanism instead (`_LOCK_T`
 * typedef'd to `struct _Mutex_Control`, `__lock_acquire`/`__lock_release`
 * macros calling RTEMS's own `_Mutex_Acquire`/`_Mutex_Release` directly),
 * used internally by newlib's own already-built `libc.a` (malloc, sinit,
 * etc.) - unrelated to and not a superset of the lowercase API here.
 *
 * This supersedes `../../src/lock.c`'s original header comment, which
 * deliberately did NOT ship this file "to avoid shadowing the toolchain's
 * real one" - that caution turned out to be justified for a different
 * reason than expected: a first attempt at wholesale-replacing
 * `<sys/lock.h>` broke the build everywhere, because newlib's own
 * `sys/reent.h` (pulled in transitively by `stdlib.h`/`time.h`/etc.)
 * itself does `#include <sys/lock.h>` and needs the toolchain's real
 * `_LOCK_RECURSIVE_T` to define `_flock_t` - confirmed by the resulting
 * compile errors across every file that includes `stdlib.h`/`time.h`.
 * Fixed by using `#include_next` to extend the real header instead of
 * replacing it: this file is found first on the include path (per
 * `-Iinclude` searched before the BSP's `-isystem` path, GCC's behavior
 * regardless of flag order on the command line), pulls in the toolchain's
 * actual `<sys/lock.h>` via `#include_next` so everything newlib's other
 * headers need is still declared, then appends the `_lock_t` API on top -
 * only intercepting this file for code compiled against this shim, not
 * newlib's own prebuilt `libc.a` internals (already object code,
 * unaffected by this header either way).
 *
 * Deliberately narrow: only the plain (non-recursive-mutex-global) API
 * surface `phy_init.c`'s confirmed usage needs (a single local static
 * `_lock_t`, not newlib's own internal `__lock___*_mutex` locks) - no
 * `__lock___malloc_recursive_mutex`-style externs, since nothing in this
 * build relies on this header backing newlib's own internals.
 */
#ifndef _FREERTOS_COMPAT_SYS_LOCK_H_
#define _FREERTOS_COMPAT_SYS_LOCK_H_

#include_next <sys/lock.h>

struct __lock;
typedef struct __lock *_lock_t;

void _lock_init(_lock_t *lock);
void _lock_init_recursive(_lock_t *lock);
void _lock_close(_lock_t *lock);
void _lock_close_recursive(_lock_t *lock);
void _lock_acquire(_lock_t *lock);
void _lock_acquire_recursive(_lock_t *lock);
int  _lock_try_acquire(_lock_t *lock);
int  _lock_try_acquire_recursive(_lock_t *lock);
void _lock_release(_lock_t *lock);
void _lock_release_recursive(_lock_t *lock);

#endif /* _FREERTOS_COMPAT_SYS_LOCK_H_ */
