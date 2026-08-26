# ESP32-C3 RTEMS development

The `esp32c3-rtems-dev` container (see `Containerfile.esp32c3-rtems` / `compose.yaml`)
builds RTEMS for the ESP32-C3 using RTEMS's `esp32c3db` BSP (`riscv/esp32c3db`).

**Status: draft, image build-verified.** `esp32c3db` was merged into RTEMS's git
`main` branch via
[merge request !1160](https://gitlab.rtems.org/rtems/rtos/rtems/-/merge_requests/1160)
and is documented under RTEMS's development docs (targeting the upcoming RTEMS 7);
it has not shipped in a tagged RTEMS release. `Containerfile.esp32c3-rtems` therefore
builds the RSB toolchain and RTEMS kernel from source instead of a released
`rtems-source-builder` bset.

Confirmed on CI (a runner with real internet access, unlike the sandbox this was
drafted in):
[this run](https://github.com/iamlateforbreakast/MCU-exp/actions/runs/32550028020)
built the image successfully end-to-end and passed its sanity check. The full
`riscv-rtems7` toolchain -
binutils 2.47, gdb 17.2, gcc 15.2.0 + newlib, rtems-tools 7 - builds (~65 minutes);
the RTEMS kernel build targeting `riscv/esp32c3db` succeeds; esptool installs; and
OpenOCD builds with its internal jimtcl. Four real bugs were found and fixed along
the way: the upstream `7/rtems-riscv` bset pulls in an unrelated, broken
`devel/sis-2-1` (SPARC/ERC32 simulator) package, excluded by building from a local
copy of the bset; OpenOCD needs both `git clone --recursive` (for its bundled
`jimtcl` submodule) *and* `--enable-internal-jimtcl` at configure time, since per
`configure.ac` that build path is opt-in regardless of submodule presence; and pip
silently falls back to a user-site install as the non-root `builder` user, which
wasn't on `PATH` (fixed by adding `~/.local/bin` alongside `$RTEMS_ROOT/bin`).
What's still unverified: the usage instructions below (building/linking an
application, flashing, debugging) - only the image build and sanity check have
actually run. Cross-check each step against the
[riscv BSPs page](https://docs.rtems.org/docs/main/user/bsps/bsps-riscv.html)
before relying on it.

This is distinct from `workspace/ESP32-C3/ESP32-C3.md`, which targets the same chip
via Espressif's ESP-IDF/FreeRTOS stack instead of RTEMS, and from the `rtems-dev`
container, which builds RTEMS for SPARC/leon3 and generic RISC-V under QEMU/Renode
rather than for real ESP32-C3 hardware.

## Usage

```
podman compose up -d esp32c3-rtems-dev
podman exec -it esp32c3-rtems-dev /bin/bash
```

`$RTEMS_ROOT/bin` (the `riscv-rtems7-*` toolchain and RTEMS's build of OpenOCD) is
on `PATH` automatically.

## Building an application

`examples/hello_world` is a minimal RTEMS app (console output only) built and
linked against the `esp32c3db` BSP with the `riscv-rtems7-gcc` toolchain,
following the same RTEMS-configuration-object + link pattern used in
`rtems_ubuntu_build.md`'s SPARC/RISC-V examples:

```
cd ~/workspace/examples/hello_world
make
```

This produces `hello_world.exe`. RTEMS's own pkgconfig support is documented
upstream as experimental and inconsistent across BSPs, so rather than hardcode a
guessed `.pc` filename, the Makefile searches `$RTEMS_ROOT` at build time for
whatever `*esp32c3db*.pc` file the BSP install actually produced and derives
`PKG_CONFIG_PATH`/the package name from it. If it can't find one, the Makefile
fails with a pointer to run `find $RTEMS_ROOT -name '*.pc'` yourself to see what's
actually installed.

**`examples/dmips_benchmark` - confirmed working on real hardware**: runs the
public-domain Dhrystone 2.1 benchmark (ported from the copy vendored at
`workspace/Luckfoxpico/luckfox-pico/sysdrv/source/uboot/u-boot/lib/dhry/`)
and prints Dhrystones/second and DMIPS (Dhrystones/second / 1757, the VAX
11/780 reference) over the console. Console/clock only, like `hello_world` -
no GPIO driver dependency, so it builds and links against the stock
`esp32c3db` BSP install with no extra integration steps. It self-calibrates
the run count off the wall clock (ramping until a run takes at least 2s), so
it doesn't need the ESP32-C3's actual clock speed hardcoded anywhere. Flashed
and run on real hardware (160MHz single-core ESP32-C3, QFN32 rev v0.4) on
2026-08-24: 800000 runs / 5.996s, 133423.6 Dhrystones/second, **75.94 DMIPS**
(~0.47 DMIPS/MHz). The end-of-run sanity values matched the Dhrystone 2.1
reference program's own documented expected results exactly (`Int_Glob=5`,
`Bool_Glob=1`, `Ch_1_Glob=A`, `Ch_2_Glob=B`), confirming the port is
behaviorally correct, not just building.

**`examples/gpio_led_blink` - builds, not yet run on hardware**: `esp32c3db`
upstream still only implements `start`, `irq`, `clock` (SYSTIMER), and `console`
(UART0/USB-Serial) drivers itself, no GPIO driver. A register-level GPIO driver
targeting RTEMS's generic `bsp/gpio.h` API is drafted in `upstream-gpio-driver/`
(register offsets/bits checked against Espressif's real headers). Its README's
"Integration steps" - copying the driver into an RTEMS checkout, patching two
`spec/build/bsps/riscv/esp32/*.yml` files plus `bsps/riscv/esp32/include/bsp.h`,
then `./waf configure --prefix=$RTEMS_ROOT --rtems-bsps=riscv/esp32c3db && ./waf
&& ./waf install` - were confirmed working end-to-end in `esp32c3-rtems-dev` on
2026-08-23: the BSP builds clean and `examples/gpio_led_blink` links to a `.exe`
with no warnings. Because `~/kernel` in the container is cloned fresh each image
build and isn't persisted, that integration currently needs to be re-applied
per-container (or land in `Containerfile.esp32c3-rtems`/upstream) until then - see
the driver README for the exact steps. Still untested against real hardware.

## Bluetooth (BLE)

`esp32c3db` has no Bluetooth driver of any kind. `upstream-bt-driver/README.md`
tracks linking ESP-IDF's BLE controller directly into this BSP - vendoring
the open `bt.c` controller frontend and ESP-IDF's Apache-2.0-licensed
prebuilt `libbtdm_app.a` against a new `freertos-compat` shim, rather than a
from-scratch register-level driver like the peripherals above. Phase 1 and
Phase 2 (task/queue/semaphore/critical-section, `esp_timer`, `esp_intr_alloc`,
the BT/Wi-Fi interrupt-vector BSP patch in `upstream-bt-driver/bsp-patch/`,
and PHY init's supporting shims) are drafted and build-confirmed against the
real toolchain/BSP headers in `esp32c3-rtems-dev` - see
`upstream-bt-driver/README.md`'s status header for the exact fixes that
took. The Kconfig-macro gap Phase 3 originally hit is resolved too:
`upstream-bt-driver/sdkconfig-compat.h` supplies all 43 `CONFIG_*` macros
`bt.c` needs (not ~45 - two of the original estimate weren't real symbols),
each confirmed against real ESP-IDF v5.3.1 Kconfig source.

**`bt.c` plus the entire `esp_phy` AND `efuse` subsystems are now vendored
and compile clean (2026-08-26)** - `upstream-bt-driver/vendor/` has
`bt.c`, 20 real support files, and ~60 transitively required headers.
Two closed Espressif blobs (`libbtdm_app.a` and the PHY calibration
`libphy.a`) were fetched (not committed - large binaries, documented
fetch commits instead) and `nm`-cross-checked; a validated linker
fragment (`upstream-bt-driver/rom-linker-patch/`) supplies the ROM
addresses both blobs need. A full test-link of everything leaves 99
undefined symbols, almost all libgcc/libm/newlib runtime helpers a real
executable link resolves automatically. Three "this pulls in a big
subsystem, must be hard" assessments turned out wrong once actually
attempted (HAL-layer register access, `esp_phy`'s NVS/wifi/sleep
dependencies, `efuse`'s own API layer) - worth remembering before
assuming something's out of scope. **BSP linker-script section placement
also done and validated with a real link** (`upstream-bt-driver/
linker-section-patch/`) - the 8 `_bt_*`/`_bt_controller_*` symbols real
IDF's `ldgen` tool generates, plus a broader `.iram1.*`/`.dram1.*`
section-handling gap the first real link attempt surfaced (used
throughout the vendored code via `IRAM_ATTR`/`DRAM_ATTR`, not BLE-specific).
**`examples/ble_vhci_smoke` - links and runs on real hardware, hangs in
real RF calibration (2026-08-26)**: a controller-only smoke test (init,
enable, HCI Reset over VHCI) is the first successful full executable
link of this whole effort - `bt.c` + `esp_phy` + `efuse` + the two
closed blobs (`libbtdm_app.a`, `libphy.a`) + the ROM-symbol linker
fragment, into a real statically-linked RISC-V `.exe`. Getting there
found and fixed a real, consequential bug: the blobs had been fetched
with an unpinned `git clone` (whatever the default branch tip was that
day), version-skewed against the IDF v5.3.1 source `bt.c` is vendored
from. Pinning to the exact submodule commits real IDF v5.3.1 uses
(read from esp-idf.git's own `.gitmodules`) resolved 11 of what had
looked like 14 genuinely-missing ROM/blob symbols, and the ROM-symbols
linker fragment itself needed regenerating against the correctly-pinned
blob (it had been built by nm-ing the same wrong commit) - it grew from
~180 to ~950 real PROVIDE()'d ROM addresses in the process (mostly
ROM-resident AES/ECC crypto for BLE Secure Connections pairing that the
newer, wrong blob commit didn't need). Three symbols
(`coex_pti_v2`, `bt_bb_v2_init_cmplx`, `bt_bb_tx_cca_set`) are stubbed
as no-ops after exhaustive real-source investigation found them in no
real Espressif-published artifact at all (see
`upstream-bt-driver/freertos-compat/src/bb_coex_stubs.c` for the full
citation trail) - a real, cited unknown, not a validated equivalence.

Flashed and run on the real board (QFN32 rev v0.4): boots, runs real
RTEMS + the app, calls into the real closed blobs successfully - the
blob's own build-version log (`BT controller compile version
[aa16a46]`, matching the pinned commit exactly) and the real PHY blob's
own version banner (`phy_version 1180,01f2a49,Jun 4 2024,16:34:25`)
both print from genuine blob code executing on real silicon. It then
hung inside `register_chipv7_phy()` - the closed PHY blob's RF
calibration entry point - confirmed by bracketing it with diagnostic
prints. **Root-caused and fixed**: real ESP-IDF's clock-tree bring-up
(`rtc_clk_init()` - XTAL frequency write, BBPLL enable/configure, CPU
clock source switch) was never called anywhere in this RTEMS port,
since RTEMS's own startup replaces ESP-IDF's and nothing else called
it. The closed PHY blob calls back into this port's `rtc_clk.c` and
into a ROM BBPLL-calibration routine that was polling a PLL-lock bit
that could never assert without that bring-up. Vendored real IDF
v5.3.1's `rtc_clk_init.c` (a small, self-contained file - all its
`rtc_clk_*` dependencies were already vendored in `rtc_clk.c`, just
never called; only needed 3 new ROM `PROVIDE()`s -
`esp_rom_set_cpu_ticks_per_us`, `esp_rom_regi2c_write{,_mask}`) and
called it once, early, from `examples/ble_vhci_smoke/init.c` before BT
init, with values matching this board's confirmed real config (40MHz
XTAL, 160MHz CPU) rather than the real default macro's literal 80MHz,
to avoid changing the CPU speed RTEMS itself already brought the board
up at. Reflashed and confirmed on real hardware: the diagnostic prints
now show `register_chipv7_phy()` actually **returning** - RF
calibration completes for real.

Execution then hit a new, distinct problem: a real assert from the
closed blob's own internal code (`BLE assert emi.c 164, param
00000000 00001000` - "emi" = the blob's internal Exchange
Memory/buffer-pool allocator). Investigated at length (2026-08-26):

- Every `esp_bt_controller_config_t` field (`CONFIG_BT_CTRL_*` values
  feeding `BT_CONTROLLER_INIT_CONFIG_DEFAULT()`) checked correct
  against real IDF defaults - ruled out.
- Both memory-allocation callbacks the blob calls into (`osi_funcs_t`'s
  `_malloc` and `_malloc_internal`) instrumented and confirmed healthy
  with real numbers - `malloc_free_space()` shows 233KB free right
  before the assert, and every one of the disassembly-traced EM-region
  allocation sizes (328/1080/616/200/1260/540/750/102/272×9 bytes)
  returns a real non-NULL address. Heap exhaustion is definitively
  ruled out, not just inferred.
- Cross-checked against Zephyr's ESP32-C3 BLE port
  (`zephyrproject-rtos/hal_espressif`/`zephyr`) - its driver code is
  structurally identical to this port's approach, and it pins
  `esp32c3-bt-lib` at commit `0a08c4b32f3666003080b662a1a61794da24ff0f`,
  which turned out to be exactly real ESP-IDF's own `master` branch pin
  (not Zephyr-specific). The original blob/`bt.c` version-skew fix
  earlier had paired `bt.c` with an *older* blob (v5.3.1's own pin) -
  correctly self-consistent, but not what real production firmware
  (or Zephyr) actually ships. Re-vendored `bt.c`/`esp_bt.h` from the
  exact real-IDF commit (`8da824cd0ede2d6c7317c5a65504bce78762b67b`)
  that paired with this newer blob commit, re-fetched the blob at that
  pin, and regenerated the ROM-symbols fragment against it (1064
  matched ROM addresses, up from 953) - full writeup in
  `upstream-bt-driver/vendor/README.md`. This got further: real,
  richer BT-controller log lines never seen before (`Using main XTAL
  as clock source`, `Feature Config, ADV:1, BLE_50:1, DTM:1, SCAN:1,
  CCA:0, SMP:1, CONNECT:1`) and a compile-version string matching the
  new blob's own commit message exactly - but it still hits the same
  assert (now at internal line 331, same `00000000 00001000` params -
  a shifted line number from the newer blob's source, not a different
  bug) and this time the assert genuinely **traps**: RTEMS's own fatal-
  exception handler caught a real RISC-V breakpoint (`ebreak`,
  `mcause=3`) and printed a full register dump, decoded via
  `riscv-rtems7-addr2line` to land inside the blob's own
  `r_assert_param` (flash-resident in this newer blob, not a ROM
  function like the older blob's build had it) - real, concrete PC-
  level visibility, just not (yet) a full call-stack backtrace back to
  the actual failing check inside `emi.c`.

A JTAG attempt (ephemeral `podman run --device .../usb/<bus>/<dev>
--security-opt label=disable`, since plain compose `devices:` +
SELinux denies raw USB nodes) got `openocd` attached to the chip's
built-in USB-JTAG, but `halt`/examination failed even on a clean
`reset halt` before any of this port's code ran - a JTAG/config
problem independent of the actual bug (exact error: `Timed out after
5s waiting for busy to go low (abstractcs=...)`, reproduced at
multiple adapter speeds and 2 different openocd config compositions -
a real tooling blocker, not yet resolved, would need a different probe
or openocd build to get a full backtrace at the assert site).

**Decisive control experiment**: built `workspace/ESP32-C3/
ble_controller_probe/`, a real ESP-IDF v5.3.1 project (the existing
`esp32c3-dev` container, not the RTEMS one) doing the exact same
minimal controller-only sequence, using the exact same closed-blob
commit this port's original pairing used. **It passed cleanly on the
same real board** - proving the blob/hardware is fine and the bug is
in this RTEMS port's platform layer. Its log directly revealed two
real, previously-undiscovered bugs (both fixed): `CONFIG_ESP_PHY_
CALIBRATION_AND_DATA_STORAGE` and `CONFIG_ESP_MAC_ADDR_UNIVERSE_BT`
(+ siblings) both default to enabled on real ESP32-C3 and were never
defined in `sdkconfig-compat.h` - the MAC one meant `mac_addr.c`'s
`ESP_MAC_BT` case was compiled out of this port's build entirely,
explaining a real, previously-unexplained "mac type is incorrect"
symptom. Fixed, confirmed on hardware (real factory MAC now reads
correctly) - but the assert persists unchanged. A follow-up test
temporarily matched this port's blob+bt.c to the exact real-IDF-tested
pairing while keeping both fixes - still hit the identical assert,
conclusively proving the remaining gap is genuine RTEMS-runtime
behavior, not any remaining source or config difference.

**JTAG finally working, real backtrace obtained.** The debug module
needs a genuine power cycle (unplug/replug USB) - a soft RTS-pin reset
(what every `esptool.py` flash ends in) leaves it permanently wedged;
confirmed by elimination across openocd builds, containers, and
firmware state before finding this. Once attached, OpenOCD's own
`halt`/`resume` don't re-wedge it, but `reset halt` is a core-only
reset that doesn't reproduce real boot-time behavior - added a
temporary startup delay to `init.c` to give a safe attach window
instead. Caught the real crash live (breakpoint at `r_assert_param`,
before the actual trap fired) and walked the stack manually to get a
real call chain: `btdm_controller_on_reset -> r_rwip_reset ->
r_hci_init/r_rwble_init -> r_lld_init -> r_lld_core_init ->
r_emi_get_mem_addr_by_offset -> [assert]`. Disassembled that function
in full: the assert is a hardware-register-vs-static-table consistency
check entirely inside the closed blob (`0x60031204+region_id*4`'s
current value vs. a value baked into the blob's own `.rodata` lookup
table).

**Root cause quantitatively confirmed** by comparing a live register
read against the real-IDF control experiment at the same point. The
failing check is for EM region 3, register `0x60031210`
(`offset=0x1000`, `region_id=3`). On the RTEMS run, `mdw 0x60031210`
reads `0x00000000` - never written. On an identical real-IDF run,
same board, same blob commit, the same register reads `0x10027c61`,
and `(0x10027c61 >> 18) << 2 = 0x1000` matches the blob's own expected
value in `em_base_reg_lut[4]` exactly. So the closed blob's
`r_emi_em_base_init()` simply never programs region 3's hardware
register on this RTEMS port, while it does on real IDF with the
identical blob. The 200-byte allocation that maps to region 3
succeeds (`DIAG: malloc_internal(200) -> ...` logs fine) - only the
register write is missing, and that write happens entirely inside
closed-blob code we can't see or step through further.

**Kconfig fix applied, real but insufficient.** The four feature-gate
macros above (`CONFIG_BT_CTRL_BLE_SCAN`, `_SECURITY_ENABLE`,
`_MIN_CONN_INTERVAL_ENABLE`, `_DTM_ENABLE`) were verified against real
IDF's own `Kconfig.in` at this exact commit - none of them are real
Kconfig options at all for this profile, so a real IDF build never
defines them and `esp_bt.h`/`bt.c` fall through to their `0` (off)
defaults. This port had them hardcoded to `1`. Flipped to `0` in
`sdkconfig-compat.h`; rebuilt and reflashed - the runtime feature-config
log now matches real IDF exactly (`DTM:0, SCAN:0, SMP:0`), confirming
the fix is correct. **But the crash persists** - only the assert's line
number moved (`emi.c 164` -> `emi.c 331`), same register (`0x60031210`),
same region 3. Kept the fix (it's independently correct) but it isn't
the root cause.

**Refined timing, from careful log re-reading (no JTAG needed for
this part):** the EM-region malloc/register-write sequence
(`r_emi_em_base_init`, all 13 allocations incl. the region-3 200-byte
one) happens once, during `esp_bt_controller_init()`, and completes
successfully. The assert now fires later, during
`esp_bt_controller_enable()`, *after* PHY calibration runs - matching
the JTAG-derived call chain's `btdm_controller_on_reset -> r_rwip_reset
-> ... -> r_emi_get_mem_addr_by_offset -> [assert]` (`rwip_reset` reads
as "RW IP stack reset", i.e. an enable-time reset of the whole BT
stack state, not part of init). `bt.c` itself only calls
`periph_module_reset(PERIPH_BT_MODULE)` once, inside `init()`, before
the EM writes - so nothing at the RTEMS-visible source level re-resets
the peripheral between init and enable. Cross-checked against Zephyr's
`hal_espressif` driver for the same blob commit
(`components/bt/controller/esp32c3/bt.c` in
github.com/zephyrproject-rtos/hal_espressif) - functionally identical
call structure, same single reset call, no extra step. So whatever
clears or fails to preserve region 3's register between init and
enable happens entirely inside the closed blob's own
`btdm_controller_on_reset`/`r_rwip_reset` path - a second genuine
diagnosability boundary, this time on the *enable* side rather than
the init side.

**Live single-instruction JTAG verification attempted, blocked by
tooling, not hardware.** Tried to catch the exact moment right after
the region-3 `sw` instruction in `r_emi_em_base_init` executes (to see
whether the write ever lands at all, vs. lands and is later cleared).
Two attempts, two power cycles, both inconclusive: raw telnet's
`openocd` process died mid-session with no error in its log; a
completely fresh, telnet-free GDB session failed at the initial RSP
handshake (`Remote replied unexpectedly to 'vMustReplyEmpty'`),
ruling out "telnet pollution" as the earlier session's cause - this
GDB build and this OpenOCD's remote-serial-protocol implementation
appear to be genuinely incompatible for `target remote`, independent
of history. See `esp32c3_jtag_debugging` memory for the full detail;
this is now a known, real gap in the toolchain for anything needing
`continue`/`stepi` over GDB, not just a documented ambiguity in
telnet's `resume`.

See `upstream-bt-driver/vendor/README.md`
for the full build recipe and blob-version-pinning writeup, and
`upstream-bt-driver/README.md` for the full mapping table, real
ESP32-C3 interrupt-source numbers, and phased plan.

## Flashing and monitoring

The BSP boots directly from flash via the ESP32-C3's direct-boot header (no 2nd
stage bootloader) - meaning the boot ROM just starts executing raw code mapped
at flash offset 0, with none of Espressif's own app-image header/segment-table
format involved. Confirmed 2026-08-24 while flashing `dmips_benchmark`:
**`esptool elf2image` doesn't work here** and shouldn't be used - it fails with
`Segment loaded at 0x42000100 lands in same 64KB flash mapping as segment
loaded at 0x42000000` on every RTEMS `.exe` in this BSP (reproduced on both
esptool 5.3.1 and 4.7.0), because it builds Espressif's app-image format from
ELF *sections* and refuses to merge `.start`/`.text` into one flash-mapped
segment since they carry different section flags (`.start` is `WAX`, `.text`
is `AX`) even though they're one contiguous `PT_LOAD` program header. Instead,
turn the linked `.exe` into a flat binary with `objcopy` and flash that
directly at offset 0 - this is what actually produced the working
`oled_1in3_sh1106_spi.bin` (verified byte-for-byte identical to
`objcopy`'s output from that example's `.exe`):

```
riscv-rtems7-objcopy -O binary <name>.exe <name>.bin
esptool.py --chip esp32c3 -p /dev/ttyACM0 write_flash 0x0 <name>.bin
```

To monitor the console (UART0/USB-Serial-JTAG) afterward, open
`/dev/ttyACM0` at 115200 8N1 with any serial terminal (e.g. `screen
/dev/ttyACM0 115200`, `python3 -m serial.tools.miniterm /dev/ttyACM0
115200`, or `esptool.py --chip esp32c3 -p /dev/ttyACM0 monitor`).

## Debugging

The BSP docs call for a development build of OpenOCD (built from source in the
container image) for JTAG debugging over the chip's built-in USB-JTAG interface or
an external probe. Pass the relevant serial/JTAG device through via the `devices`
entry in `compose.yaml`.

## Firmware output

`compose.yaml` also mounts a separate `firmware/ESP32-C3-RTEMS` host directory to
`~/firmware` inside the container - copy built images there so they're easy to find
on the host for flashing.
