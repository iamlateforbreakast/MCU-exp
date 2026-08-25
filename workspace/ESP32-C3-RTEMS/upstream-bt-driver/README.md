# Linking ESP-IDF's BLE controller into RTEMS's `esp32c3db` BSP

**Status: Phase 1 and Phase 2 drafted in full (`esp_timer`
+`esp_timer_start_periodic`, `esp_intr_alloc`, the BSP interrupt-vector
patch in `bsp-patch/`, and PHY init's supporting shims - `_lock_*`,
`esp_deep_sleep_register_phy_hook`, `portENTER/EXIT_CRITICAL_SAFE`;
vendoring `esp_phy`'s own real source and the register-level PHY
clock-enable work are the two things still open there). Phase 3 (the
hardware smoke test) has its API recon done and its Kconfig-macro blocker
resolved (`sdkconfig-compat.h`, see below) - the smoke-test app itself
isn't drafted yet, and can't be run without real ESP32-C3 hardware
regardless.

**Build-confirmed 2026-08-25** (`esp32c3-rtems-dev` container, real
`riscv-rtems7-gcc` against the real installed `esp32c3db` BSP headers, after
applying `bsp-patch/` to a fresh RTEMS `main` checkout and `./waf install`ing
it - see `bsp-patch/README.md`): all 8 `freertos-compat/src/*.c` files
compile clean with `-Wall -Wextra`. `critical.c`, `esp_intr_alloc.c`,
`esp_sleep.c`, `queue.c`, `semphr.c`, and `task.c` needed no changes.
`esp_timer.c` and `lock.c` each needed a missing
`#include <rtems/rtems/object.h>` for `rtems_build_name` (now fixed).

`lock.c` also hit a real, confirmed blocker beyond the missing include:
this toolchain's own `<sys/lock.h>`
(`$RTEMS_ROOT/riscv-rtems7/include/sys/lock.h`) does not declare the
`_lock_t`/`_lock_acquire()`-style FreeBSD/newlib-upstream retargetable-locking
API this shim assumed it would (per the file's own "not confirmed" comment,
now resolved). Instead it defines an entirely different mechanism -
`_LOCK_T` typedef'd to `struct _Mutex_Control`, `__lock_acquire`/
`__lock_release` macros wrapping RTEMS's own `_Mutex_Acquire`/`_Mutex_Release`
directly. Fixed by adding `freertos-compat/include/sys/lock.h`, which
`#include_next`s the toolchain's real `<sys/lock.h>` (needed transitively by
newlib's own `sys/reent.h` for `_LOCK_RECURSIVE_T` - a first attempt at
wholesale-replacing the header instead of extending it broke every file
that includes `stdlib.h`/`time.h`, a real mistake caught by recompiling
everything, not just `lock.c`, after the change) and appends the `_lock_t`
API on top. Relies on `-Iinclude` being searched before the BSP's
`-isystem` path, which GCC does unconditionally regardless of flag order -
already this repo's own convention, but now load-bearing for correctness,
not just header-organization.

**Major milestone, same session (2026-08-25): `bt.c` itself is now vendored
and compiles clean.** See `vendor/README.md` for the full detail - summary:
`bt.c`/`esp_bt.h` and ~25 transitively-required ESP-IDF headers (pure
declarations/macros, no C source beyond `bt.c` itself) are vendored in
`vendor/`, and compiling `bt.c` against the real toolchain for the first
time found three more real, confirmed bugs in `freertos-compat`
(`SemaphoreHandle_t`/`TaskHandle_t` needed to be pointer types, not bare
`rtems_id`, plus several missing FreeRTOS/port functions) - all fixed.
`libbtdm_app.a` (the closed blob) was fetched and real `nm`-cross-checked
against `bt.o`'s needs (the step Phase 0/3 could only postpone until now) -
92% of what the blob needs is confirmed satisfiable via real ESP-IDF ROM
linker scripts, 14 symbols remain genuinely unresolved. Not linked yet, and
still no NimBLE/esp_phy source vendored - see `vendor/README.md`'s "Not
vendored / still open" for the honest remaining list.

This
directory tracks integrating ESP-IDF's BLE stack directly
into the RTEMS `esp32c3db` image (single chip, no second-chip HCI-UART bridge).
Unlike the other `upstream-*-driver/` directories here, this isn't a
register-level peripheral driver you write from scratch - the actual radio
link-layer/baseband code is a prebuilt library, and the open C code sitting
on top of it is written directly against the FreeRTOS API. So the real task
is: give that C code an RTEMS-backed implementation of the FreeRTOS API it
already calls, then link the prebuilt library in unmodified.

The ESP32-C3 is BLE 5.0-only (no classic Bluetooth).

## Phase 0 recon findings (against real ESP-IDF source)

Done by sparse-cloning `github.com/espressif/esp-idf` at tag **`v5.3.1`**
(same version already pinned in `Containerfile.esp32c3` for this repo's plain
ESP-IDF container, for consistency) plus `github.com/espressif/esp32c3-bt-lib`
(the submodule it points at), both fetched with `--filter=blob:none` /
`--filter=blob:limit=1k` partial clones so the actual `.a` binaries were never
downloaded - only source, headers, and metadata (LICENSE/README/CMakeLists)
were inspected. Re-check all of this against whatever IDF tag is actually
used once real implementation starts, since none of it is guaranteed stable
across IDF versions.

**Confirmed - the split between open and closed code:**
- `components/bt/controller/esp32c3/bt.c` (1845 lines, `SPDX-License-Identifier:
  Apache-2.0`) is the open "controller frontend" - BLE controller
  init/enable/sleep/power-management glue. It is compiled from source, so it
  can be recompiled against a different OS's headers without touching IDF's
  build system.
- The actual link-layer/baseband code is prebuilt: `components/bt/CMakeLists.txt`
  (~line 864-867) links `esp32c3`'s controller against `libbtdm_app.a`, found
  via `target_link_directories(... "controller/lib_esp32c3_family/esp32c3")`.
  `lib_esp32c3_family` is a git submodule pointing at
  `github.com/espressif/esp32c3-bt-lib`, which contains only
  `esp32c3/libbtdm_app.a`, `esp32c3/libbtdm_app_flash.a`, plus an `esp32s3/`
  counterpart, LICENSE, and README.
- NimBLE's host-side porting layer,
  `components/bt/porting/npl/freertos/src/npl_os_freertos.c` (1259 lines), is
  **also** open source and **also** written directly against the plain
  FreeRTOS API (events built on queues, mutexes, counting semaphores,
  software timers) - not through some separate abstraction layer.

**Confirmed - licensing is not a blocker:** `esp32c3-bt-lib`'s own
`LICENSE`/`README.md` state the prebuilt `.a` files are distributed under
Apache License 2.0 ("Object form" per the license's own terminology),
copyright Espressif Systems 2021, "provided under the same license as the
parent esp-idf project." Same Apache-2.0 header on `bt.c` and
`npl_os_freertos.c`. Standard Apache-2.0 attribution/NOTICE obligations
apply, but there is no separate proprietary EULA restricting use to an
unmodified ESP-IDF build. (This is the opposite of what an earlier draft of
this doc assumed before checking - worth remembering when re-verifying
against whatever IDF tag is actually used.)

**Corrected architecture (supersedes the original two-shim plan):** `bt.c`
calls the real FreeRTOS API directly by name (`xTaskCreatePinnedToCore`,
`xQueueSend`, `xSemaphoreTake`, `portENTER_CRITICAL`, etc.) plus a handful of
ESP-IDF subsystem APIs (`esp_intr_alloc`, `esp_timer_*`, `esp_phy_*`,
`esp_pm_lock_*`, `esp_ipc_call_blocking`). Since `npl_os_freertos.c` is built
on that same plain FreeRTOS API, **one shared `freertos-compat/` shim
(implementing the real FreeRTOS API against RTEMS primitives) is enough to
compile both `bt.c` and `npl_os_freertos.c` unmodified** - there's no need
for two separate bespoke shims ("osi-shim" + "npl-rtems") as originally
sketched.

**Correction, found during Phase 2 recon:** the claim above that there's "no
dynamic OSI function table" was only half right. `bt.c` does define a
`struct osi_funcs_t` (`bt.c:155`, populated at `bt.c:349` as
`osi_funcs_ro`, copied into a heap instance at `bt.c:1399-1404`) and hands
it to the closed blob - so the blob *does* call through a vtable, exactly
the pattern originally assumed in Phase 0 before it was (wrongly) walked
back. What was right: every function `bt.c` puts *into* that table
(`interrupt_alloc_wrapper`, and presumably the task/queue/semaphore
wrappers, though those weren't individually re-grepped this session) is
itself just a thin call into the plain FreeRTOS/`esp_*` API this README
already tracks - so `freertos-compat` still doesn't need to model the
vtable's struct layout, only the functions `bt.c`'s own wrappers call. The
practical consequence: the blob supplies some of its own parameters at
*runtime* through this vtable rather than `bt.c` naming them statically -
see the interrupt-source discussion below, which is a direct result of this.

**Confirmed - the exact API surface `bt.c` needs** (extracted by grepping
every FreeRTOS/`esp_`-prefixed call site in `bt.c`, 31 distinct
functions/macros):

```
xTaskCreatePinnedToCore   vTaskDelete
xQueueCreate   xQueueSend   xQueueReceive   xQueueSendFromISR   xQueueReceiveFromISR
xSemaphoreCreateMutex   xSemaphoreCreateCounting
xSemaphoreTake   xSemaphoreGive   xSemaphoreTakeFromISR   xSemaphoreGiveFromISR
portENTER_CRITICAL   portEXIT_CRITICAL   portENTER_CRITICAL_ISR   portEXIT_CRITICAL_ISR
esp_intr_alloc
esp_timer_create   esp_timer_delete   esp_timer_start_once   esp_timer_stop
esp_phy_enable   esp_phy_disable   esp_phy_modem_init   esp_phy_modem_deinit
esp_pm_lock_create   esp_pm_lock_acquire   esp_pm_lock_release   esp_pm_lock_delete
esp_ipc_call_blocking
```

All of these except `xTaskCreatePinnedToCore` are standard, publicly
documented FreeRTOS API (real prototypes confirmed in
`components/freertos/FreeRTOS-Kernel/include/freertos/{task,queue,semphr}.h`
at this IDF tag - not IDF-specific). `xTaskCreatePinnedToCore` is IDF's SMP
extension of `xTaskCreate` with an added `xCoreID` affinity parameter
(confirmed signature: `BaseType_t xTaskCreatePinnedToCore(TaskFunction_t,
const char * const, const configSTACK_DEPTH_TYPE, void * const, UBaseType_t,
TaskHandle_t * const, const BaseType_t xCoreID)`); on the single-core ESP32-C3
this collapses to a plain task-create call that ignores `xCoreID`, so the
RTEMS shim doesn't need to model core affinity at all.

**Confirmed - PHY calibration can avoid needing an NVS port:** IDF's
`ESP_PHY_CALIBRATION_AND_DATA_STORAGE` Kconfig option
(`components/esp_phy/Kconfig`) defaults to `y` (load/store calibration data in
NVS). Disabling it does a full RF calibration on every boot instead, with no
NVS/partition dependency - the option's own help text confirms this. Since
`esp32c3db` has no NVS/flash-partition support in this repo yet either, Phase
2 (below) should target this "no storage" mode; a full calibration every
boot costs boot time but needs nothing else built first.

**Not yet done - still needs real ground truth before Phase 3:** the actual
undefined-symbol list `libbtdm_app.a` exports/imports (would need
`riscv32-esp-elf-nm` run against the real `.a`, which wasn't fetched here to
keep this recon lightweight - only its LICENSE/README were pulled without
the binary blobs). Confirm this once inside a container that can actually
fetch and link the library, since `bt.c`'s own call sites already give a
correct high-level picture but not the blob's exact linkage requirements
(e.g. any additional ROM function symbols it expects to be present).

## Phase 1 - `freertos-compat/` (drafted, unbuilt, untested)

**What's here:** `freertos-compat/include/freertos/{FreeRTOS,task,queue,semphr,portmacro}.h`
- drop-in-compatible headers `bt.c`/`npl_os_freertos.c` `#include` unmodified
- plus `freertos-compat/src/{task,queue,semphr,critical}.c` implementing them
against real RTEMS Classic API primitives. Files mirror their intended final
layout (an include dir + src dir a real build would compile), same
`upstream-*-driver` convention as `../upstream-gpio-driver/`.

Signatures below were confirmed by sparse-cloning `github.com/RTEMS/rtems` at
`main` (`esp32c3db` isn't in any tagged release) into a throwaway `/tmp`
scratch dir, never committed.

**Mapping table** (FreeRTOS call → RTEMS primitive, Confirmed against real
RTEMS `main` source unless noted):

| FreeRTOS | RTEMS | Notes |
| --- | --- | --- |
| `xTaskCreatePinnedToCore` | `rtems_task_create` + `rtems_task_start` | Priority inverted (RTEMS: 1=highest, unlike FreeRTOS's highest-number=highest); `xCoreID` ignored (single-core); name truncated to 4 chars via `rtems_build_name` |
| `vTaskDelete` | `rtems_task_delete` | `NULL` handle → `RTEMS_SELF` |
| `xQueueCreate` | `rtems_message_queue_create` | Handle wraps `{id, item_size}` - RTEMS's `_send` needs an explicit per-call size, unlike a FreeRTOS queue's fixed item size |
| `xQueueSend(FromISR)` | `rtems_message_queue_send` | **Semantic mismatch, not resolved**: RTEMS send never blocks (returns `RTEMS_TOO_MANY` if full) where FreeRTOS blocks up to the timeout - `xTicksToWait` is currently ignored on the send path |
| `xQueueReceive(FromISR)` | `rtems_message_queue_receive` | ISR variant forces `RTEMS_NO_WAIT` (RTEMS only allows non-blocking receive from ISR context) |
| `xSemaphoreCreateMutex` | `rtems_semaphore_create(RTEMS_BINARY_SEMAPHORE\|RTEMS_INHERIT_PRIORITY\|RTEMS_PRIORITY)` | Priority-inheritance mutex - **not ISR-safe in RTEMS**, but `bt.c`'s own mutex calls (checked via grep) are never paired with a `*FromISR` variant, so this doesn't need to be |
| `xSemaphoreCreateCounting` | `rtems_semaphore_create(RTEMS_COUNTING_SEMAPHORE)` | RTEMS counting semaphores have no separate max-count ceiling; `uxMaxCount` is unused |
| `xSemaphoreTake/Give(FromISR)` | `rtems_semaphore_obtain/release` | Counting semaphore is ISR-safe per RTEMS docs |
| `portENTER/EXIT_CRITICAL(+_ISR)` | `rtems_interrupt_disable/enable` | **Unverified**: real IDF's exact macro/argument signature wasn't independently re-checked this session (only that the call names exist, via grep) - modeled as taking a `portMUX_TYPE*` per IDF's SMP convention. `rtems_interrupt_disable/enable` themselves are RTEMS's oldest, most stable interrupt API, not re-verified this session either but very low risk |
| `portYIELD_FROM_ISR` | no-op | RTEMS reschedules automatically on interrupt exit - unverified against this BSP's actual interrupt-exit path until Phase 3's hardware test |

**Open risks carried in the code as comments, not just here:**
- The blocking-send mismatch above - needs checking against `bt.c`'s real
  call sites once it's actually vendored in; may need an auxiliary counting
  semaphore for space-available signaling if a blocking send turns out to
  matter.
- `portENTER_CRITICAL`'s exact signature is a modeled assumption, not a
  confirmed one - re-check against real ESP-IDF `portmacro.h` before trusting
  it.
- RTEMS task names are packed 4-character values, not retained strings -
  fine for `bt.c`/NimBLE's debug-only use of task names, but worth knowing.

**Not part of Phase 1** (see "Phase 2 findings" below for why): `esp_intr_alloc`,
`esp_timer_*`, `esp_phy_*`, `esp_pm_lock_*`, `esp_ipc_call_blocking`.

## Phase 2 - `esp_timer`/`esp_intr_alloc` (drafted); PHY init and the BSP
interrupt-vector patch (not started)

**What's here, drafted this session:**
`freertos-compat/include/{esp_err,esp_timer,esp_intr_alloc}.h` +
`freertos-compat/src/{esp_timer,esp_intr_alloc}.c`. Same unbuilt/untested
status as Phase 1.

**`esp_timer` mapping** (`esp_timer_create/start_once/stop/delete`, real
signatures confirmed against `components/esp_timer/include/esp_timer.h` at
IDF v5.3.1 this session):

| ESP-IDF | RTEMS | Notes |
| --- | --- | --- |
| `esp_timer_create` | `rtems_timer_create` | Lazily starts a global `rtems_timer_initiate_server()` on first call (idempotent - a second call returning `RTEMS_INCORRECT_STATE` is treated as already-running, not an error). Rejects `create_args->dispatch_method != ESP_TIMER_TASK` - `ESP_TIMER_ISR` dispatch isn't supported |
| `esp_timer_start_once` | `rtems_timer_server_fire_after` | Runs the callback in the **timer server task's context**, not clock-tick ISR context (unlike plain `rtems_timer_fire_after` - see Phase 1's finding). `timeout_us`→ticks conversion reads `rtems_clock_get_ticks_per_second()` at runtime rather than assuming a fixed rate |
| `esp_timer_stop` | `rtems_timer_cancel` | |
| `esp_timer_delete` | `rtems_timer_delete` | |

`rtems_timer_service_routine_entry`'s exact parameter list
(`void (*)(rtems_id, void *)`, used by the trampoline in `esp_timer.c`) is
RTEMS's documented convention but wasn't independently re-grepped against
source this session - flagged in the code, re-check before trusting it.

**`esp_intr_alloc` mapping** (real signature confirmed against
`components/esp_hw_support/include/esp_intr_alloc.h`/`esp_intr_types.h` at
IDF v5.3.1 this session): `esp_intr_alloc`/`esp_intr_free` map onto
`rtems_interrupt_handler_install`/`_remove` (`RTEMS_INTERRUPT_UNIQUE`, per
Phase 1's precedent-check against `clockdrv_systimer.c`), using the IDF
`source` parameter directly as the RTEMS vector number - the same
convention this BSP's own `irq_mappings[]` table already uses (e.g.
`GPIO_PROCPU_INTR=16` is both the interrupt-matrix source number and the
RTEMS vector). `esp_intr_enable`/`esp_intr_disable` map onto
`rtems_interrupt_vector_enable`/`_disable`, **not independently
re-confirmed this session** (RTEMS's well-known public vector-enable API,
but not re-grepped) - flagged in the code.

This mapping is intentionally generic - it doesn't hardcode a BT/Wi-Fi
vector number, so it compiles without the still-missing BSP patch below.
It will only actually succeed at runtime once that patch exists; until
then RTEMS rejects the unrecognized vector at install time.

**Confirmed this session - real ESP32-C3 interrupt-matrix source numbers**
(`components/soc/esp32c3/include/soc/interrupts.h` at IDF v5.3.1, the
`interrupt_source_t` enum, values 0-16 - the enum's own comment says "this
table is decided by hardware, don't touch this"):

```
0  ETS_WIFI_MAC_INTR_SOURCE
1  ETS_WIFI_MAC_NMI_SOURCE
2  ETS_WIFI_PWR_INTR_SOURCE
3  ETS_WIFI_BB_INTR_SOURCE
4  ETS_BT_MAC_INTR_SOURCE      ("will be cancelled" per the enum's own doc comment)
5  ETS_BT_BB_INTR_SOURCE
6  ETS_BT_BB_NMI_SOURCE
7  ETS_RWBT_INTR_SOURCE        (classic-BT baseband - C3 has no classic BT)
8  ETS_RWBLE_INTR_SOURCE       (BLE baseband - best-guess candidate, see below)
9  ETS_RWBT_NMI_SOURCE
10 ETS_RWBLE_NMI_SOURCE
...
15 ETS_UHCI0_INTR_SOURCE       (= RTEMS's UHCI0_INTR = 15 - numbering matches exactly)
16 ETS_GPIO_INTR_SOURCE        (= RTEMS's GPIO_PROCPU_INTR = 16 - confirms the two enumerations are the same numbering)
```

Sources 15/16 lining up exactly with RTEMS's existing `UHCI0_INTR`/
`GPIO_PROCPU_INTR` values confirms these are literally the same
interrupt-matrix numbering RTEMS's `chip_definitions.h` already (partially)
uses - so sources 4-10 above are the direct `#define` values a BSP patch
would add, not numbers needing further translation.

**Not fully confirmed - which specific source `bt.c`'s blob requests at
runtime.** `bt.c` itself never names the interrupt source constant (grepped
`interrupt_alloc_wrapper()`, `bt.c:495`): `source` is a plain `int` the
*closed blob* supplies at runtime through the `osi_funcs_t.interrupt_alloc`
callback (see the `osi_funcs_t` correction above) - it isn't visible in
open source at all. Best-guess candidate: **`ETS_RWBLE_INTR_SOURCE` (8)** -
"RWBLE" (RivieraWaves BLE IP) directly matches ESP32-C3 being BLE-only, its
doc comment carries no deprecation caveat unlike `BT_MAC_INTR_SOURCE`'s
"will be cancelled", and `RWBT`/`BT_MAC` read as classic-BT-only
identifiers this chip shouldn't need. This is reasoned inference, not a
runtime-confirmed fact - the real value can only be confirmed once Phase 3
can actually run the blob (e.g. logging whichever `source` value arrives at
a real `esp_intr_alloc`) or by disassembling `libbtdm_app.a` (out of
scope). The BSP patch below should define all of sources 4-10, not just 8,
since the exact runtime value isn't nailed down.

**Drafted this session - the actual BSP patch**, `bsp-patch/`:
`esp32c3db`'s `bsps/riscv/esp32/irq/irq_c3.c` `irq_mappings[]` table
(`{ .peripheral_int = <source>, .cpu_int = <1-31> }`) has all 31 `cpu_int`
lines already assigned to existing peripherals - but per this session's
recon of the table itself, **sharing a `cpu_int` line is an existing,
working pattern** (`SW_INTR_0..3` all share `cpu_int=1`,
`EFUSE_INTR`+`LEDC_INTR` share `7`, `TG_WDT_INTR`+`TG1_WDT_INTR` share
`14`) - the dispatch code resolves interrupts down to the individual
peripheral source by scanning the interrupt-matrix status register, not
just the shared `cpu_int` line, so a new BT entry doesn't need a free line.
`bsp-patch/` has the `#define`s for sources 4-10 and an `irq_mappings[]`
entry sharing `cpu_int=7` (with `EFUSE_INTR`/`LEDC_INTR`, both rare in a
system without active LEDC PWM) - as fragments to merge into the two real
upstream files, not full copies of them (see `bsp-patch/README.md` for the
exact integration steps), register-level work in the same vein as
`../upstream-gpio-driver/`.

**PHY init - supporting shims drafted this session; vendoring `esp_phy`
itself and the register-level clock-enable work still open.** Real
signatures confirmed this session
(`components/esp_phy/include/esp_phy_init.h`, `src/phy_init.c` at IDF
v5.3.1): `void esp_phy_enable(esp_phy_modem_t modem)` /
`esp_phy_disable(esp_phy_modem_t modem)` (bt.c passes `PHY_MODEM_BT = 2`),
`void esp_phy_modem_init(void)` / `esp_phy_modem_deinit(void)`. Confirmed
the "no NVS" path works exactly as Phase 1 assumed: with
`CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE` undefined,
`esp_phy_load_cal_and_init()` (called internally by `esp_phy_enable()` on
first enable) skips NVS entirely and calls
`register_chipv7_phy(init_data, cal_data, PHY_RF_CAL_FULL)` directly; with
`CONFIG_ESP_PHY_INIT_DATA_IN_PARTITION` also off (default), `init_data`
comes from a **compiled-in static default array**
(`esp_phy_get_init_data()` returns `&phy_init_data`, declared by
`phy_init_data.h`), not a flash partition - so this genuinely needs zero
NVS/partition support, confirming Phase 0's assumption.

**Scope grew, but the follow-up recon (this session) resolved most of
it:**

- **`esp_phy` itself is open source, same as `bt.c`** - `phy_init.c` and
  `phy_common.c` (`components/esp_phy/src/`) are plain Apache-2.0 C, not
  part of any closed blob. The plan here is the same as for `bt.c`: vendor
  these files unmodified, don't reimplement `esp_phy_enable`/etc.
  ourselves - `freertos-compat` only needs to supply what *they* call.
  There's a separate closed blob for the actual RF/PHY calibration/tuning
  work, `components/esp_phy/lib` (a submodule, confirmed to exist this
  session, same pattern as `esp32c3-bt-lib`) - its license wasn't checked
  yet (do that before vendoring, same treatment Phase 0 gave
  `esp32c3-bt-lib`).
- **`phy_track_pll_init`/`phy_track_pll`/`phy_track_pll_deinit` and the
  antenna functions are open C** (`components/esp_phy/src/phy_common.c`,
  confirmed by grepping their actual `void phy_track_pll(void) { ... }`
  definitions, not just declarations) - not inside the closed blob as
  guessed earlier. `phy_track_pll_init` itself just calls
  `esp_timer_create`/`esp_timer_start_periodic` - meaning it depends on
  `freertos-compat`'s own `esp_timer` shim, which didn't have a periodic
  variant yet (RTEMS timers are inherently one-shot, confirmed in Phase 2).
  **Added this session**: `esp_timer_start_periodic` in
  `freertos-compat/include/esp_timer.h` + `src/esp_timer.c` - the
  trampoline re-arms itself via another `rtems_timer_server_fire_after()`
  call on each expiry when a nonzero period is set.
- **`_lock_acquire`/`_lock_release`/`_lock_t`** (used by
  `esp_phy_enable`'s `s_phy_access_lock`) is confirmed to be newlib's own
  standard retargetable-locking API
  (`components/newlib/platform_include/sys/lock.h`, guarded by
  `_RETARGETABLE_LOCKING`, wrapping the toolchain's real `<sys/lock.h>` via
  `#include_next`) - **not** something RTEMS's own `cpukit` already
  provides (grepped `cpukit/libcsupport` + `cpukit/include` for
  `_lock_acquire`/`_RETARGETABLE_LOCKING` this session: zero hits: only
  RTEMS's differently-named `rtems_interrupt_lock_acquire`/
  `rtems_termios_device_lock_acquire`). **Added this session**:
  `freertos-compat/src/lock.c` implements `_lock_init(_recursive)`/
  `_lock_close(_recursive)`/`_lock_acquire(_recursive)`/
  `_lock_try_acquire(_recursive)`/`_lock_release(_recursive)` against RTEMS
  semaphores - deliberately does **not** ship its own `sys/lock.h` (that
  would shadow the toolchain's real one); it just implements the functions
  the real header already declares. **Not confirmed**: whether the actual
  `riscv-rtems7-*` toolchain built by `Containerfile.esp32c3-rtems` (via
  rtems-source-builder, a separate build outside RTEMS's own source tree,
  unreachable by this session's recon technique) enables
  `_RETARGETABLE_LOCKING` in its newlib build at all - if it doesn't, these
  symbol names may not be the toolchain's real retarget point. Needs
  checking against the actual built toolchain.
- **`esp_deep_sleep_register_phy_hook`** confirmed to be pure bookkeeping
  (`components/esp_hw_support/sleep_modes.c`: appends the callback to a
  fixed-size array, invoked only if deep sleep is ever entered) - safe to
  no-op, since this RTEMS port has no deep-sleep subsystem at all and isn't
  building one for BLE bring-up. **Added this session**:
  `freertos-compat/include/esp_sleep.h` + `src/esp_sleep.c`, a one-function
  stub that always returns `ESP_OK`.
- **`esp_phy_common_clock_enable()`** (called at the top of
  `esp_phy_enable`) resolves to `wifi_bt_common_module_enable()`
  (`components/esp_hw_support/periph_ctrl.c`) - confirmed to branch on
  `SOC_MODEM_CLOCK_IS_INDEPENDENT` between `modem_clock_module_enable()`
  and a simpler ref-counted `periph_ll_wifi_bt_module_enable_clk()`
  register write guarded by `portENTER_CRITICAL_SAFE`/`_EXIT` (a variant
  not previously in this shim - **added this session** to
  `freertos-compat/include/freertos/portmacro.h`, aliased to the same
  enter/exit critical implementation as the plain `portENTER_CRITICAL`,
  since this shim's version is already safe from both task and ISR
  context). **Not yet resolved**: which of the two branches ESP32-C3 takes,
  and the actual register-level clock-enable work either implies - this is
  new register-level recon, in the same vein as `../upstream-gpio-driver/`,
  not yet done.

Tick rate used by `esp_timer.c`'s ms→ticks conversion:
`CONFIGURE_MICROSECONDS_PER_TICK` (default 10000µs = 100Hz, confirmed in
`cpukit/include/rtems/confdefs/clock.h` during Phase 1's recon) - read at
runtime via `rtems_clock_get_ticks_per_second()`, not assumed.

## Architecture

```
RTEMS application (GAP/GATT)
        |
NimBLE host (portable C, vendor from components/bt/host/nimble at the pinned IDF tag)
        |
npl_os_freertos.c (unmodified - already just plain FreeRTOS API calls)
        |
VHCI glue (in-process function calls between host and controller, no UART)
        |
bt.c (esp32c3 controller frontend, unmodified) + libbtdm_app.a (prebuilt, Apache-2.0)
        |
freertos-compat/  <-- NEW shim: the 31-function surface above, implemented
                       against RTEMS Classic API (tasks, message queues,
                       semaphores/mutexes), the BSP's existing RISC-V irq
                       driver (for esp_intr_alloc), and its existing SYSTIMER
                       clock driver (for esp_timer)
```

## Phased plan

**Phase 0 - recon (done above).** Re-verify against whichever IDF tag is
actually used if it drifts from v5.3.1.

**Phase 1 - `freertos-compat/` (drafted).** Task/queue/semaphore/critical-
section surface against RTEMS Classic API primitives - see the "Phase 1"
section above for the mapping table, exact files, and open risks. This was
the bulk of the well-grounded work, since both the controller and (later)
the NimBLE host build on it unmodified.

**Phase 2 - drafted in full.** `esp_timer_*` (including
`esp_timer_start_periodic`, needed by PHY's own `phy_track_pll_init`) maps
onto a new `rtems_timer_server` task (not plain `rtems_timer_fire_after`);
`esp_intr_alloc` maps onto the BSP's existing RISC-V irq driver, now paired
with `bsp-patch/`'s actual BT/Wi-Fi interrupt-vector `chip_definitions.h`/
`irq_mappings[]` additions so it works end-to-end (modulo the inferred, not
confirmed, choice of `RWBLE_INTR`). PHY init
(`ESP_PHY_CALIBRATION_AND_DATA_STORAGE=n`, full calibration every boot, no
NVS needed) turned out to need more than the 4 originally-scoped functions
once its real call graph was traced, but nearly all of that extra surface
(`_lock_*`, `esp_deep_sleep_register_phy_hook`, `portENTER/EXIT_CRITICAL_SAFE`)
is now drafted too - see the "Phase 2" section above for all of this in
detail, including the two pieces still open (vendoring `esp_phy`'s own real
source, and the register-level PHY clock-enable work).

**Phase 3 - controller-only smoke test (hard go/no-go gate). Recon done
this session; a real new blocker was found before the smoke-test app can
even be drafted, let alone run.** The plan: vendor `bt.c` + link
`libbtdm_app.a` against the Phase 1/2 shim, and verify a minimal VHCI
loopback - send an HCI Reset command via `esp_vhci_host_send_packet`,
confirm a valid HCI Command Complete event comes back **on real hardware**
(this repo's sandbox has none - see "What this session could and couldn't
do" below). Also run `riscv32-esp-elf-nm -u` on the real `libbtdm_app.a` at
this point and diff against the shim's exported symbols, to close the "not
yet done" recon gap from Phase 0/2 before trusting anything past this
point.

**Confirmed this session - the actual VHCI/controller-init API**
(`components/bt/include/esp32c3/include/esp_bt.h` at IDF v5.3.1, small and
clean):
```c
typedef struct {
    void (*notify_host_send_available)(void);
    int  (*notify_host_recv)(uint8_t *data, uint16_t len);
} esp_vhci_host_callback_t;

bool     esp_vhci_host_check_send_available(void);
void     esp_vhci_host_send_packet(uint8_t *data, uint16_t len);
esp_err_t esp_vhci_host_register_callback(const esp_vhci_host_callback_t *callback);

esp_err_t esp_bt_controller_init(esp_bt_controller_config_t *cfg);
esp_err_t esp_bt_controller_enable(esp_bt_mode_t mode);
```

**Kconfig-generated config surface blocker - resolved 2026-08-25.**
`esp_bt_controller_config_t`'s default-init macro
(`BT_CONTROLLER_INIT_CONFIG_DEFAULT()`) and `bt.c` itself reference
distinct `CONFIG_*` macros (`CONFIG_BT_CTRL_MODE_EFF`,
`CONFIG_BT_CTRL_BLE_MAX_ACT_EFF`, `CONFIG_ESP_PHY_ENABLED`,
`CONFIG_FREERTOS_UNICORE`, etc.) that a real ESP-IDF build gets for free
from Kconfig/menuconfig, generated into `sdkconfig.h`. Vendoring
`bt.c`/`esp_bt.h` into an RTEMS build with no Kconfig system at all means
**none of these exist** unless something provides them - a genuinely new
category of missing surface (build configuration, not an OS API to shim)
that nothing in Phase 0-2 accounted for.

**`sdkconfig-compat.h`** (this directory) now supplies all of them, for a
minimal ESP32-C3 + NimBLE + BLE-only + no-coexistence + no-power-management
profile - the same one this section originally proposed. Resolved by
sparse-cloning the real `github.com/espressif/esp-idf` at the pinned
**v5.3.1** tag (real internet access, unlike the sandbox that drafted the
original ~45 estimate) and reading the actual Kconfig files, not
guessing - each macro in the header cites the exact `Kconfig`/`Kconfig.in`
file and line that produced its value. Two real surprises fell out of
that recon, not visible from `bt.c`'s call sites alone:
- The true count is **43, not ~45** - `CONFIG_BTDM_CONTROLLER_MODEM_SLEEP`
  (part of the original grep-based estimate) only ever appears inside a
  comment in `esp_bt.h`, never in a real `#if`/`#ifdef`.
- **`CONFIG_MAC_BB_PD`, `CONFIG_SW_COEXIST_ENABLE`, and their dead parent
  `CONFIG_BT_CTRL_HW_CCA`, are not real Kconfig options anywhere in
  v5.3.1's source tree at all** - a real stock ESP-IDF build for esp32c3
  never defines them either, so the `#if` blocks in `bt.c` guarded by them
  are permanently dead code in this IDF version (likely uncleaned
  leftovers from an earlier chip generation), not something this shim was
  missing.

See `sdkconfig-compat.h`'s own header comment for the full per-macro
citation table, including the one deliberate deviation from a real
build's default (`CONFIG_ESP_COEX_ENABLED` really defaults to `y` on
esp32c3, overridden to undefined here to avoid vendoring the `esp_coex`
component for a minimal smoke test).

**What the original session could and couldn't do:** that pass's sandbox
had no ESP32-C3 hardware, no built RTEMS toolchain, and no internet access
to check real Kconfig defaults - so Phase 3's actual smoke-test
application wasn't drafted then, to avoid writing code that couldn't
actually build as described. What *was* done: confirming the real
VHCI/controller-init API above (so the eventual smoke-test app's structure
is grounded in real signatures, not guessed), and surfacing the
Kconfig-surface blocker before it could silently undermine a
drafted-but-uncompilable smoke test - now resolved by `sdkconfig-compat.h`
above. `bt.c`/NimBLE source and `libbtdm_app.a` still aren't vendored into
this repo, so Phase 3's smoke-test app itself still isn't drafted - that's
the next real step, not a blocker anymore. The actual go/no-go
hardware test - and everything after it (Phase 4/5) - stays exactly what
it always was: something only real hardware and a real RTEMS checkout can
verify, not something this environment can simulate or claim.

**Phase 4 - NimBLE host:** vendor `components/bt/host/nimble` (portable C)
and `npl_os_freertos.c` unmodified, on top of the working VHCI transport from
Phase 3.

**Phase 5 - example app:** `workspace/ESP32-C3-RTEMS/examples/ble_adv` (or
similar) - advertise + a minimal GATT service, matching this repo's existing
example README pattern (build/flash steps, hardware-verification status
called out honestly, not claimed until actually run).

## Open risks

- The 31-function surface and the "no OSI table" architecture above are
  specific to IDF v5.3.1's `bt.c`; re-verify if a different tag is used.
- `libbtdm_app.a`'s exact linkage requirements beyond `bt.c`'s visible call
  sites are unconfirmed until Phase 3's `nm` step.
- Apache-2.0 attribution/NOTICE obligations apply when redistributing
  `libbtdm_app.a`, `bt.c`, and NimBLE - straightforward, but don't drop the
  NOTICE file when vendoring.
- No NVS-backed PHY calibration means a full RF calibration every boot
  (Phase 2), which is slower than stock IDF's default and hasn't been
  measured yet.
- Single-core, single-radio hardware already carrying RTEMS's own
  console/clock/irq load - no dual-core AMP fallback exists here the way it
  might on classic (dual-core) ESP32.

## Integration steps (once Phase 1 code exists)

Not yet applicable - no shim code has been written. Once it exists, follow
the same pattern as `../upstream-gpio-driver/README.md`: files here mirror
their intended path inside an RTEMS/vendored-component checkout, with exact
`waf`/CMake integration steps documented and a Confirmed/Unverified status
kept current as each phase actually gets built and run.
