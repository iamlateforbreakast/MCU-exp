# ESP32-C3 RTEMS development

The `esp32c3-rtems-dev` container (see `Dockerfile.esp32c3-rtems` / `compose.yaml`)
builds RTEMS for the ESP32-C3 using RTEMS's `esp32c3db` BSP (`riscv/esp32c3db`).

**Status: draft, image build-verified.** `esp32c3db` was merged into RTEMS's git
`main` branch via
[merge request !1160](https://gitlab.rtems.org/rtems/rtos/rtems/-/merge_requests/1160)
and is documented under RTEMS's development docs (targeting the upcoming RTEMS 7);
it has not shipped in a tagged RTEMS release. `Dockerfile.esp32c3-rtems` therefore
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

`examples/hello_world` is a minimal RTEMS app (console output only - see below for
why not GPIO) built and linked against the `esp32c3db` BSP with the
`riscv-rtems7-gcc` toolchain, following the same RTEMS-configuration-object + link
pattern used in `rtems_ubuntu_build.md`'s SPARC/RISC-V examples:

```
cd ~/workspace/examples/hello_world
make
```

This produces `hello_world.exe`. The Makefile's `BSP_PC` (the `pkg-config` name for
the BSP, `riscv-rtems7-esp32c3db`) is inferred from RTEMS's standard waf install
naming convention, not confirmed against a real build - if `pkg-config` can't find
it, run `ls $RTEMS_ROOT/lib/pkgconfig` inside the container to see the actual name
and adjust the Makefile.

**No GPIO example**: unlike every other MCU container's `examples/gpio_led_blink`,
there's no LED-blink example here. Checked directly against the BSP source
(`bsps/riscv/esp32/` in the RTEMS tree) - `esp32c3db` currently only implements
`start`, `irq`, `clock` (SYSTIMER), and `console` (UART0/USB-Serial) drivers, no
GPIO driver. The only GPIO-related thing in the BSP headers is a raw address for
Espressif's ROM `gpio_output_set()` function - usable in principle, but calling a
ROM function directly by address on untested hardware isn't something to present
as a working example. Revisit once RTEMS upstream adds a real GPIO driver for this
BSP.

## Flashing and monitoring

The BSP boots directly from flash via the ESP32-C3's direct-boot header (no 2nd
stage bootloader). Flash the built image with `esptool`:

```
esptool.py --chip esp32c3 write_flash 0x0 <image>.bin
```

## Debugging

The BSP docs call for a development build of OpenOCD (built from source in the
container image) for JTAG debugging over the chip's built-in USB-JTAG interface or
an external probe. Pass the relevant serial/JTAG device through via the `devices`
entry in `compose.yaml`.

## Firmware output

`compose.yaml` also mounts a separate `firmware/ESP32-C3-RTEMS` host directory to
`~/firmware` inside the container - copy built images there so they're easy to find
on the host for flashing.
