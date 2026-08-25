# Vendored ESP-IDF source (Apache-2.0)

**Status: `bt.c` compiles clean (2026-08-25)** against the real `riscv-rtems7-gcc` +
installed `esp32c3db` BSP + `freertos-compat` shim + `sdkconfig-compat.h`, in
`esp32c3-rtems-dev`. Not linked yet - `libbtdm_app.a` isn't vendored into this
repo (see "Linking" below for why, and what's already confirmed about it).

Files here mirror their real path inside an ESP-IDF checkout
(`components/<component>/...`), copied unmodified (SPDX headers intact) from
`github.com/espressif/esp-idf` at the pinned tag **v5.3.1**, same tag this
repo's other recon has used throughout. Two categories:

- **`bt/controller/esp32c3/bt.c`, `bt/include/esp32c3/include/esp_bt.h`** -
  the actual controller frontend this whole directory exists to integrate
  (see `../README.md`).
- **Everything else** - transitively-required ESP-IDF headers `bt.c` (or a
  header it includes) `#include`s, discovered by iteratively compiling
  `bt.c` against the real toolchain and vendoring whatever the compiler
  reported missing, one file at a time, always preferring the esp32c3-specific
  variant when a header exists per-chip (confirmed necessary once done wrong:
  an early pass grabbed `esp32c2`'s `interrupt_reg.h` by accident since it's
  alphabetically first - caught by the resulting build errors, not by luck).
  All are pure declarations/macros/register-bitfield headers with zero
  runtime C source of their own - no `.c` files were vendored here beyond
  `bt.c` itself.

## Build recipe

Confirmed working (`esp32c3-rtems-dev`, 2026-08-25):

```sh
riscv-rtems7-gcc -march=rv32imc -mabi=ilp32 -Wall -Wextra \
  -include esp_intr_alloc.h \
  -I../build-include -I../freertos-compat/include \
  -Icomponents/bt/include/esp32c3/include \
  -Icomponents/esp_common/include \
  -Icomponents/esp_rom/include -Icomponents/esp_rom/include/esp32c3 \
  -Icomponents/riscv/include \
  -Icomponents/soc/esp32c3/include \
  -Icomponents/esp_system/include \
  -Icomponents/esp_hw_support/include -Icomponents/esp_hw_support/port/esp32c3/include \
  -Icomponents/esp_phy/include \
  -Icomponents/heap/include \
  -isystem $RTEMS_ROOT/riscv-rtems7/esp32c3db/lib/include \
  -c components/bt/controller/esp32c3/bt.c -o bt.o
```

(paths relative to this `vendor/` directory; run from `upstream-bt-driver/`
with `-Ivendor/components/...` instead, as this session did).

**`-include esp_intr_alloc.h` is required and not a hack.** `bt.c` calls
`esp_intr_alloc`/`esp_intr_free`/`esp_intr_enable`/`esp_intr_disable` and uses
`intr_handle_t` (`bt.c:150`, `:492-523`) without ever `#include`-ing a header
that declares them - confirmed this is genuinely true of upstream IDF too,
not a gap in this vendoring: grepped the real, unmodified `esp32c3/bt.c` in
the real esp-idf tree and its own transitive include chain (`riscv/
interrupt.h`, `esp_private/interrupt_intc.h`, `esp_private/periph_ctrl.h`,
`esp_private/esp_clk.h`) - none of them include the real `esp_intr_alloc.h`
either. (Other chip variants' `bt.c`, e.g. `esp32`/`esp32c2`, DO include it
directly - esp32c3's just doesn't.) Real ESP-IDF's own build evidently
relies on this resolving some other way (implicit-declaration was only a
warning, not an error, on the compilers/flags IDF historically targeted;
modern GCC defaults to erroring on it). `freertos-compat/include/
esp_intr_alloc.h` already has the exact right declarations (confirmed
against real IDF source in Phase 2, see `../README.md`) - `-include` just
makes them visible to this specific translation unit, same effect a stock
IDF build gets some other way.

`sdkconfig.h` is not `sdkconfig-compat.h`'s own filename because `bt.c`
`#include`s the former by name (matching every real ESP-IDF file) -
`../build-include/sdkconfig.h` is a one-line forwarding stub
(`#include "../sdkconfig-compat.h"`) so the real filename resolves without
renaming the documented, cited `sdkconfig-compat.h` this repo's README
already points to.

## What compiling bt.c for the first time found in `freertos-compat`

Three real, confirmed bugs in the previously-drafted shim, only surfaced by
actually compiling real `bt.c` against it (not visible from Phase 0-2's
call-site recon alone) - all fixed as part of this session, see the
individual files' own header comments for detail:

- **`SemaphoreHandle_t`/`TaskHandle_t` were bare `rtems_id` (integers), not
  pointers.** Real FreeRTOS's versions of both are pointer types, and `bt.c`
  relies on that - e.g. `semphr->handle = (void *)xSemaphoreCreateCounting(...)`
  (`bt.c:555`) then passes that `void *` straight back into
  `xSemaphoreTakeFromISR`. Fixed to `rtems_id *` (heap-allocated), mirroring
  `QueueHandle_t`'s already-correct pointer-based pattern in `queue.h`.
- **Missing FreeRTOS/port surface**: `vSemaphoreDelete`, `vQueueDelete`,
  `vPortYield`, `xPortInIsrContext`, `portTICK_PERIOD_MS` - none drafted in
  Phase 0-2 because the 31-function list there was built from grepping
  `bt.c`'s calls into `esp_*`/FreeRTOS APIs specifically, and these are
  either plain-FreeRTOS calls that grep pass didn't specifically enumerate
  or (for the port functions) came from files not yet vendored at the time.
- **`portYIELD_FROM_ISR` argument-count mismatch**: modeled as taking one
  argument (SMP-kernel convention); `bt.c`'s one real call site
  (`bt.c:546`) uses the older zero-argument form. Now variadic.

New shim surface added, all new files (not fixes to existing ones):
`esp_heap_caps.h`/`esp_heap_caps_init.h` + `src/heap_caps.c` (backed by
plain `malloc`/`calloc` - see that header's own comment for why IDF's
capability-aware multi-region allocator doesn't need porting here),
`esp_log.h` (macros over `printf`), `esp_pm.h`/`esp_ipc.h` (deliberately
empty - see each file's comment for why `bt.c`'s calls into them are
compiled out for this profile), plus `ESP_ERR_*` constants added to
`esp_err.h` (real values copied from IDF) and `freertos/FreeRTOSConfig.h`
(so `esp_task.h`'s `configMAX_PRIORITIES` resolves).

## Linking - not attempted, but the real requirement is now known exactly

`libbtdm_app.a` (the closed baseband/link-layer blob, Apache-2.0 "Object
form" per `esp32c3-bt-lib`'s own LICENSE, same recon as Phase 0) was fetched
from `github.com/espressif/esp32c3-bt-lib` at commit `0a08c4b` (2026-05-25)
with real internet access this session - **not committed into this repo**
(large binaries, same reasoning this repo already applies to not vendoring
RTEMS itself: document the exact fetch source/commit instead of storing the
bytes). Not linked against `bt.o` this session either - that needs a real
link step (RTEMS application image, not just object files) beyond what was
attempted here.

**What Phase 0/3 flagged as a postponed step - "run `nm -u` against the real
blob and diff against the shim's exported symbols" - is now done:**
compiled `bt.o` (this vendoring) needs 88 undefined symbols total. Of those,
most are either already implemented in `freertos-compat` (will resolve once
actually linked together, not yet attempted) or standard libc (`malloc`,
`memcpy`, `printf`, `__assert_func`, `__udivdi3` - all provided by RTEMS's
newlib). Cross-referencing `libbtdm_app.a`'s own real `nm` output (real
`riscv-rtems7-nm` reads Espressif's RISC-V `.a` fine, despite being a
different vendor toolchain than the one that built it - same ELF/RISC-V
target, no incompatibility found) against `bt.o` found **zero direct
symbol-name overlap** between the two - confirming `bt.c` and the blob only
talk through the runtime `osi_funcs_t` vtable (`bt.c:155`, `:349`, `:1399`),
never by calling each other's functions by name directly, exactly as
`../README.md`'s architecture section already described.

The blob's own 185 real external needs (undefined across its 102 object
files, minus symbols the archive resolves internally) were checked against
ESP-IDF's real ROM linker scripts
(`components/esp_rom/esp32c3/ld/esp32c3.rom*.ld`, same v5.3.1 tag) - **171 of
185 (92%) are real, fixed ROM addresses already defined there**
(`PROVIDE(sym = 0x4000....)` style), confirming the closed blob mostly calls
directly into the ESP32-C3's boot ROM, not into anything this repo would
need to implement. This is the same category of gap as `ets_rom_layout_p`
(`../README.md`'s Phase 2 section) at much larger scale - the RTEMS BSP's
link step needs these ROM address definitions added (a real linker-script
integration task, register-level in the same vein as the interrupt-vector
`bsp-patch/`, not yet attempted).

**14 symbols remain genuinely unresolved** even after checking every
`esp32c3.rom*.ld` file (including the per-revision `eco3`/`eco7` overlays)
and both `libbtdm_app.a` and `libbtdm_app_flash.a`:

```
bt_bb_set_max_gain   bt_bb_set_rx_sense   bt_bb_tx_cca_set
bt_bb_v2_init_cmplx  bt_track_pll_cap
r_llc_le_ping_restart
r_lld_adv_start_update_filter_policy  r_lld_con_rx_channel_assess
r_lld_res_list_priv_mode_update  r_lld_scan_try_sched  r_lld_test_stop
r_rwbtdm_isr_wrapper  r_rwip_assert  r_rwip_wakeup_end
```

Interesting detail, not yet resolved: several of these (`r_llc_le_ping_restart`,
`r_lld_adv_start_update_filter_policy`, `r_rwip_assert`, ...) **do** appear in
`esp32c3.rom.ld`, but commented out (e.g. `/* r_rwip_assert = 0x4000147c; */`)
- meaning real stock IDF deliberately does NOT link against that ROM address
for these specific functions. Whatever real IDF's build actually satisfies
these 14 with (a different `.ld` fragment not yet checked, symbols compiled
directly into IDF's own bt component objects, or something else) isn't
identified yet - flagged as open, not guessed at.

## Not vendored / still open

- `libbtdm_app.a`/`libbtdm_app_flash.a` (see "Linking" above).
- NimBLE host source (`components/bt/host/nimble`) and
  `npl_os_freertos.c` - Phase 4 per `../README.md`.
- `esp_phy`'s own real source (`phy_init.c`/`phy_common.c`) - `bt.c` compiles
  against the header-only `esp_phy_init.h`/`esp_private/phy.h` vendored here,
  but the functions themselves (`esp_phy_enable` etc.) aren't implemented or
  vendored, so nothing calling them can link yet.
- The ROM linker-script integration this session's `nm` cross-check found
  necessary (171+ `PROVIDE()`-style symbol definitions, plus resolving the
  14 genuinely-missing ones) - comparable in scope to `../bsp-patch/`, not
  started.
