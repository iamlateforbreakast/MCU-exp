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

**`bt.c` itself is now vendored and compiles clean too (2026-08-25)** -
`upstream-bt-driver/vendor/` has `bt.c`/`esp_bt.h` plus ~25 transitively
required ESP-IDF headers, and compiling it for the first time found (and
fixed) three more real bugs in `freertos-compat`. `libbtdm_app.a` (the
closed blob) was fetched and `nm`-cross-checked against what `bt.o` actually
needs: 92% of the blob's own external requirements are now a real, validated
linker-script fragment (`upstream-bt-driver/rom-linker-patch/`). 6 more real
ESP-IDF support files were then vendored and confirmed compiling clean
(`esp_clk.c`/`hw_random.c`/`mac_addr.c`/`periph_ctrl.c`/`rtc_clk.c`/
`rtc_time.c`) - correcting an earlier assessment that this needed new
register-level driver work (their HAL-layer headers turned out to be pure
register-access code, same as headers already vendored, not OS-dependent
driver logic). A full test-link (`bt.o` + the blob + these 6 files + the ROM
fragment) leaves **70 undefined symbols, down from 88**. Not linked into an
actual RTEMS application yet - no NimBLE host source, no `esp_phy` source
(real, open, but pulls in NVS/sleep-retention this profile hasn't touched),
no efuse subsystem (small remaining piece `mac_addr.c` itself needs), no
BSP-side linker-script placement for `bt.c`'s own `.bss`/`.data` sections
(real IDF generates this via its own build-time `ldgen` tool, not something
this session replicated),
and no example app - nothing BLE-related runs on hardware yet. See
`upstream-bt-driver/vendor/README.md` for the full build recipe
and exact remaining-work list, and `upstream-bt-driver/README.md` for the
full mapping table, real ESP32-C3 interrupt-source numbers, and phased plan.

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
