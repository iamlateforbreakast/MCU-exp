# Linking ESP-IDF's BLE controller into RTEMS's `esp32c3db` BSP

**Status: Phase 1 drafted (`freertos-compat/`, task/queue/semaphore/critical-
section surface only) - unbuilt, untested, like every other `upstream-*-driver`
in this repo until it's dropped into a real checkout. Phase 2 onward
(`esp_intr_alloc`, `esp_timer`, PHY init, the controller/host code itself)
not started.** This directory tracks integrating ESP-IDF's BLE stack directly
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

**Corrected architecture (supersedes the original two-shim plan):** there is
no dynamic "OSI function table" the closed blob calls through. `bt.c` calls
the real FreeRTOS API directly by name (`xTaskCreatePinnedToCore`,
`xQueueSend`, `xSemaphoreTake`, `portENTER_CRITICAL`, etc.) plus a handful of
ESP-IDF subsystem APIs (`esp_intr_alloc`, `esp_timer_*`, `esp_phy_*`,
`esp_pm_lock_*`, `esp_ipc_call_blocking`). Since `npl_os_freertos.c` is built
on that same plain FreeRTOS API, **one shared `freertos-compat/` shim
(implementing the real FreeRTOS API against RTEMS primitives) is enough to
compile both `bt.c` and `npl_os_freertos.c` unmodified** - there's no need
for two separate bespoke shims ("osi-shim" + "npl-rtems") as originally
sketched.

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

## Phase 2 findings (recon done, code not started)

Two things surfaced while researching Phase 1 that reshape Phase 2's scope
beyond what was originally sketched:

- **`rtems_timer_fire_after`'s callback runs in clock-tick ISR context, not
  a task** - a mismatch with `esp_timer` callback semantics (IDF's timer
  callbacks run in a dedicated task and may do more than ISR-safe work).
  Phase 2 should use `rtems_timer_initiate_server()` +
  `rtems_timer_server_fire_after()` (task-context dispatch) instead of plain
  `rtems_timer_fire_after`.
- **No BT/Wi-Fi interrupt source is defined anywhere in `esp32c3db`'s irq
  driver today.** `bsps/riscv/esp32/irq/irq_c3.c` /
  `include/c3/chip_definitions.h` only name 32 of the RISC-V interrupt
  matrix's 63 possible sources; the unnamed range (matrix sources 0-14) is
  exactly where the real chip's `BT_MAC_INTR_SOURCE` / `BT_BB_INTR_SOURCE` /
  `RWBLE_INTR_SOURCE` etc. live. Phase 2's `esp_intr_alloc` shim can't be
  written until those vector numbers and an `irq_mappings[]` table entry are
  added first - register-level work in the same vein as
  `../upstream-gpio-driver/`, needing the same Espressif-header cross-check
  before trusting the numbers.
- Tick rate is `CONFIGURE_MICROSECONDS_PER_TICK` (default 10000µs = 100Hz,
  confirmed in `cpukit/include/rtems/confdefs/clock.h`) - Phase 2's
  ms→ticks conversion for `esp_timer_start_once` must read this rather than
  assume 1ms/tick.

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

**Phase 2 - `esp_intr_alloc`/`esp_timer`/PHY-init (not started - recon
done).** Wire `esp_intr_alloc` to the BSP's existing RISC-V irq driver
(blocked on adding the missing BT/Wi-Fi interrupt vector numbers first -
see "Phase 2 findings" above), `esp_timer_*` to a new `rtems_timer_server`
task (not plain `rtems_timer_fire_after` - see "Phase 2 findings"), and PHY
init with `ESP_PHY_CALIBRATION_AND_DATA_STORAGE=n` (full calibration every
boot, no NVS needed).

**Phase 3 - controller-only smoke test (hard go/no-go gate):** vendor
`bt.c` + link `libbtdm_app.a` against the Phase 1/2 shim, and verify a
minimal VHCI loopback - send an HCI Reset command via
`esp_vhci_host_send_packet`, confirm a valid HCI Command Complete event comes
back on real hardware. Also run `riscv32-esp-elf-nm -u` on the real
`libbtdm_app.a` at this point and diff against the Phase 1/2 shim's exported
symbols, to close the "not yet done" recon gap above before trusting
anything past this point.

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
