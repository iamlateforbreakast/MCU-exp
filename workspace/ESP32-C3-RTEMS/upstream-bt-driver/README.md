# Linking ESP-IDF's BLE controller into RTEMS's `esp32c3db` BSP

**Status: design doc + Phase 0 recon only. No shim code written yet, nothing
built.** This directory tracks integrating ESP-IDF's BLE stack directly into
the RTEMS `esp32c3db` image (single chip, no second-chip HCI-UART bridge).
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

**Phase 1 - `freertos-compat/`:** implement the 31-function surface above
against RTEMS Classic API primitives. This is the bulk of the work and the
part most worth getting right first, since both the controller and (later)
the NimBLE host build on it unmodified.

**Phase 2 - `esp_intr_alloc`/`esp_timer`/PHY-init:** wire `esp_intr_alloc` to
the BSP's existing RISC-V irq driver, `esp_timer_*` to its existing SYSTIMER
clock driver, and PHY init with `ESP_PHY_CALIBRATION_AND_DATA_STORAGE=n`
(full calibration every boot, no NVS needed).

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
