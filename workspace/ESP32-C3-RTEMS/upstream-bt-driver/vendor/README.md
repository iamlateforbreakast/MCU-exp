# Vendored ESP-IDF source (Apache-2.0)

**Status: `bt.c` compiles clean, and 6 more real support files are now
vendored and compile clean too (2026-08-25)**, against the real
`riscv-rtems7-gcc` + installed `esp32c3db` BSP + `freertos-compat` shim +
`sdkconfig-compat.h`, in `esp32c3-rtems-dev`. A real test-link of `bt.o` +
those 6 files' objects + the fetched (not vendored) `libbtdm_app.a` +
`../rom-linker-patch/btdm-rom-symbols.ld` (`riscv-rtems7-ld -r`, same
session) leaves only **70 undefined symbols - down from 88** - see
"Linking" below for the exact remaining set and what each one needs.

Files here mirror their real path inside an ESP-IDF checkout
(`components/<component>/...`), copied unmodified (SPDX headers intact) from
`github.com/espressif/esp-idf` at the pinned tag **v5.3.1**, same tag this
repo's other recon has used throughout. Three categories:

- **`bt/controller/esp32c3/bt.c`, `bt/include/esp32c3/include/esp_bt.h`** -
  the actual controller frontend this whole directory exists to integrate
  (see `../README.md`).
- **`esp_hw_support/{esp_clk,hw_random,mac_addr,periph_ctrl}.c` and
  `esp_hw_support/port/esp32c3/{rtc_clk,rtc_time}.c`** - real, open
  implementations of `esp_clk_xtal_freq`/`esp_clk_slowclk_cal_get`,
  `esp_random`, `esp_read_mac`, `periph_module_enable/_disable/_reset` +
  `wifi_bt_common_module_enable/_disable`, and `rtc_clk_slow_src_get` -
  six of `bt.o`'s undefined symbols. **A wrong assumption corrected**:
  these were first assessed (previous session) as needing new
  register-level driver work comparable to this repo's GPIO/SPI/I2C
  drivers, based on their headers pulling in ESP-IDF's HAL layer
  (`hal/clk_gate_ll.h` etc.) - reading that HAL layer showed it's just
  more `static inline` pure-register-access code (same category as
  `soc/rtc_cntl_reg.h`, already vendored), so these five `.c` files and
  their ~20 transitive headers turned out to be ordinary vendoring, not
  new driver work. Don't assume "pulls in `hal/`" means hard without
  actually reading the file - this was the second time this session that
  assumption was wrong (see `../README.md`'s note on the `-include
  esp_intr_alloc.h` finding for the first).
- **Everything else** - transitively-required ESP-IDF headers, discovered
  by iteratively compiling each `.c` file against the real toolchain and
  vendoring whatever the compiler reported missing, one file at a time,
  always preferring the esp32c3-specific variant when a header exists
  per-chip (confirmed necessary once done wrong: an early pass grabbed
  `esp32c2`'s `interrupt_reg.h` by accident since it's alphabetically
  first - caught by the resulting build errors, not by luck). All pure
  declarations/macros/register-bitfield headers, no further `.c` sources
  needed beyond the ones named above.

## Build recipe

Confirmed working (`esp32c3-rtems-dev`, 2026-08-25):

```sh
riscv-rtems7-gcc -march=rv32imc_zicsr -mabi=ilp32 -Wall -Wextra \
  -include esp_intr_alloc.h \
  -I../build-include -I../freertos-compat/include \
  -Icomponents/bt/include/esp32c3/include \
  -Icomponents/esp_common/include \
  -Icomponents/esp_rom/include -Icomponents/esp_rom/include/esp32c3 -Icomponents/esp_rom/esp32c3 \
  -Icomponents/riscv/include \
  -Icomponents/soc/esp32c3/include \
  -Icomponents/esp_system/include \
  -Icomponents/esp_hw_support/include -Icomponents/esp_hw_support/port/esp32c3/include -Icomponents/esp_hw_support/port/include -Icomponents/esp_hw_support/include/soc \
  -Icomponents/esp_phy/include \
  -Icomponents/heap/include \
  -Icomponents/hal/esp32c3/include -Icomponents/hal/platform_port/include -Icomponents/hal/include \
  -Icomponents/efuse/include -Icomponents/efuse/esp32c3/include \
  -isystem $RTEMS_ROOT/riscv-rtems7/esp32c3db/lib/include \
  -c components/bt/controller/esp32c3/bt.c -o bt.o
```

(paths relative to this `vendor/` directory; run from `upstream-bt-driver/`
with `-Ivendor/components/...` instead, as this session did. The `hal`/
`efuse`/extra `esp_hw_support` include dirs are only needed once compiling
the 6 support files described above, not for `bt.c` alone - included here
so this is one complete, current recipe rather than two partial ones.)

**`-march=rv32imc_zicsr` (not plain `rv32imc`) is required for `hw_random.c`**
- real, confirmed 2026-08-25: `riscv/rv_utils.h`'s CSR read
  (`csrr a5,0x7e2`, reading the hardware RNG-adjacent CSR) fails to
  assemble under plain `rv32imc` on this toolchain's binutils
  (`unrecognized opcode ... extension 'zicsr' required` - a real RISC-V
  spec split, not a code bug: `Zicsr`/`Zifencei` were carved out of the
  base ISA after `rv32imc` was originally specified, and this binutils
  version enforces the split). Every other vendored file here compiles
  identically with or without it - safe to always include.

**`mac_addr.c` additionally needs `-include assert.h`** on its own compile
line (on top of the recipe above) - it calls `assert()` (`mac_addr.c:216`)
without `#include <assert.h>` anywhere in its own real, unmodified include
chain (checked: `soc_caps.h`/`esp_rom_efuse.h`/`esp_mac.h`/`esp_efuse.h`/
`esp_efuse_table.h`, none of them include it either) - the same category of
gap as `bt.c`'s `esp_intr_alloc.h` situation below, confirmed genuinely
true of upstream IDF's own file, not something this vendoring broke.

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
(`components/esp_rom/esp32c3/ld/esp32c3.rom*.ld`, same v5.3.1 tag) - **172 of
186 (including `ets_rom_layout_p`/`esp_rom_delay_us`, bt.c's own two direct
ROM needs) are real, fixed ROM addresses already defined there**
(`PROVIDE(sym = 0x4000....)` style), confirming the closed blob mostly calls
directly into the ESP32-C3's boot ROM, not into anything this repo would
need to implement.

**Now actually done, not just proposed**: `../rom-linker-patch/btdm-rom-symbols.ld`
has all 172 as real `PROVIDE()` lines, cited and cross-checked - see that
file's own header comment for the full provenance. **Validated with a real
link test** (`riscv-rtems7-ld -r -T btdm-rom-symbols.ld bt.o
libbtdm_app.a -o combined.o`, 2026-08-25): no syntax errors (one real
mistake caught and fixed here - a double-`PROVIDE()` wrap on a line whose
source text already had one), and the resulting undefined-symbol count
dropped from 88 (`bt.o` alone) to 73. This is the same category of gap as
`ets_rom_layout_p` was on its own (`../README.md`'s Phase 2 section), just
resolved at full scale now instead of for one symbol - integrating this
fragment into the actual RTEMS BSP's own linker script (vs. this session's
standalone `-T`-flag validation) is the next step, not yet done.

**Extended further, same session, after vendoring the 6 support files
above**: linking them in surfaced 8 more real needs - 5 more ROM aliases
(`esp_rom_get_cpu_ticks_per_us`, `esp_rom_set_cpu_ticks_per_us`,
`esp_rom_regi2c_write(_mask)`, `esp_rom_printf`, each a real two-hop
`PROVIDE()` alias, same citation method) and 3 memory-mapped
peripheral-register-struct globals (`SYSTEM`, `TIMERG0`, `TIMERG1` - a
different mechanism from ROM function addresses: real IDF's `soc/
*_struct.h` headers declare these `extern` for direct `SYSTEM.field`-style
register access; addresses computed from this repo's own already-vendored
`reg_base.h`/`soc.h` macros, not a new external source). All added to the
same fragment file, re-validated with a fresh link test
(`bt.o` + `libbtdm_app.a` + the 6 support `.o` files + the fragment):
**70 undefined symbols remain**, down from 88.

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

Remaining undefined symbols after the extended ROM-fragment link test (70
total), grouped by what each actually needs - real recon done 2026-08-25
for all of these, confirmed via reading the real source, not guessed:

- **Will resolve once actually linked** (already implemented, just not
  combined in a real link yet): `freertos-compat`'s own exports
  (`xQueue*`/`xSemaphore*`/`xTaskCreatePinnedToCore`/`vTaskDelete`/
  `vPortYield`/`xPortInIsrContext`/`freertos_compat_enter/exit_critical`/
  `heap_caps_*`/`esp_intr_*`/`esp_timer_*`), and libc
  (`malloc`/`free`/`printf`/`abort`/`__assert_func`/`__udivdi3`, all in
  RTEMS's newlib already).

- **`coex_pti_v2`, `l2c_ble_link_get_tx_buf_num`** - expected, not new gaps:
  the former is `esp_coex` (deliberately excluded from this profile, see
  `../README.md`'s `CONFIG_ESP_COEX_ENABLED` note), the latter is an L2CAP
  function from the NimBLE/Bluedroid host (Phase 4, not vendored yet).

- **RESOLVED this session, corrected a wrong earlier assessment**:
  `periph_module_enable`/`_disable`/`_reset`, `esp_clk_xtal_freq`,
  `esp_clk_slowclk_cal_get`, `rtc_clk_slow_src_get`, `esp_random`,
  `esp_read_mac` were all previously flagged here as needing new
  register-level driver work because their headers pull in ESP-IDF's HAL
  layer. Actually reading `hal/clk_gate_ll.h` (and the other HAL headers
  these need) showed it's just more `static inline` pure-register-access
  code, same as the register headers already vendored - no OS dependency
  at all. All six vendored and compiling clean now (see status header
  above); two small real build-recipe requirements fell out of doing this:
  `-march=rv32imc_zicsr` (not plain `rv32imc` - `hw_random.c` needs a real
  CSR instruction plain `rv32imc` doesn't cover) and `-include assert.h`
  for `mac_addr.c` specifically (same category of gap as `bt.c`'s
  `esp_intr_alloc.h` situation - real upstream file, confirmed not
  including something it actually calls). Full detail in "Build recipe"
  above.

- **`esp_phy_enable`/`_disable`/`_modem_init`/`_modem_deinit` AND
  `esp_wifi_bt_power_domain_on`/`_off`** - all six live in the same real,
  open Apache-2.0 file: `components/esp_phy/src/phy_init.c` (1160 lines,
  confirmed by reading it this session - the power-domain pair wasn't
  previously known to live here, closing a small gap in Phase 2's original
  recon). Not vendored: `phy_init.c` `#include`s `nvs.h`/`nvs_flash.h`/
  `esp_efuse.h`/`esp_private/wifi.h`/`hal/efuse_hal.h`/`esp_private/
  sleep_retention.h` - real additional subsystems (flash/NVS, sleep-
  retention) this profile hasn't needed anything from yet, plus the efuse
  subsystem noted next. Vendoring it means either pulling all of those in
  too or carefully `#ifdef`-ing/stubbing around them - given the
  `periph_module_*` lesson above, this may turn out more tractable than
  it looks, but wasn't attempted this session (1160 lines is a lot to
  discovery-loop through in one pass).

- **`ESP_EFUSE_MAC`, `ESP_EFUSE_USER_DATA_MAC_CUSTOM`,
  `esp_efuse_get_field_size`, `esp_efuse_read_field_blob`** - the one
  real remaining piece `mac_addr.c` (vendored this session) itself needs:
  the `efuse` component's actual read implementation
  (`components/efuse/src/esp_efuse_api.c`, 355 lines, real and open,
  `#include`s `esp_efuse_utility.h` - not yet checked how much further
  that pulls in). Given the `periph_module_*` precedent, worth checking
  before assuming it's hard - not attempted this session for time reasons,
  not because it was assessed as complex.

- **`bt_bb_*`/`bt_track_pll_cap`/`r_llc_*`/`r_lld_*`/`r_rwip_*`/
  `r_rwbtdm_isr_wrapper`** (14 symbols) - genuinely unresolved even after
  checking every ROM `.ld` variant and both blob files, see "Linking"
  above.

- **`_bt_data_start`/`_bt_data_end`/`_bt_bss_start`/`_bt_bss_end`/
  `_bt_controller_data_start`/`_bt_controller_data_end`/
  `_bt_controller_bss_start`/`_bt_controller_bss_end`** - **not** ROM
  addresses (a wrong first guess this session corrected): real ESP-IDF
  generates these via its own build-time linker-fragment tool (`ldgen`),
  reading `.lf` files like `components/bt/linker_esp_ble_controller.lf`
  (checked - it maps `libble_app.a`'s `.bss`/`.data` sections into
  `dram0_bss`/`dram0_data` with a `SURROUND(bt_controller_bss)`-style
  directive, which is what actually generates the paired `_start`/`_end`
  symbols). Since this RTEMS build doesn't use `ldgen` at all, replicating
  this means hand-writing the equivalent section-placement + `PROVIDE()`
  pairs directly into the `esp32c3db` BSP's own linker script - real
  linker-script surgery where getting it wrong is a silent memory-placement
  bug, not a compile error. Not attempted this session; needs understanding
  the BSP's actual default linker script layout first, which hasn't been
  looked at yet.

- **`libbtdm_app.a`/`libbtdm_app_flash.a`** themselves (see "Linking"
  above) - fetched and analyzed, not vendored into the repo.
- **NimBLE host source** (`components/bt/host/nimble`) and
  `npl_os_freertos.c` - Phase 4 per `../README.md`.
