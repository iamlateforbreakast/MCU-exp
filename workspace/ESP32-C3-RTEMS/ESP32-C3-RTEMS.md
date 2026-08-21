# ESP32-C3 RTEMS development

The `esp32c3-rtems-dev` container (see `Dockerfile.esp32c3-rtems` / `compose.yaml`)
builds RTEMS for the ESP32-C3 using RTEMS's `esp32c3db` BSP (`riscv/esp32c3db`).

**Status: draft, partially build-tested.** `esp32c3db` was merged into RTEMS's git
`main` branch via
[merge request !1160](https://gitlab.rtems.org/rtems/rtos/rtems/-/merge_requests/1160)
and is documented under RTEMS's development docs (targeting the upcoming RTEMS 7);
it has not shipped in a tagged RTEMS release. `Dockerfile.esp32c3-rtems` therefore
builds the RSB toolchain and RTEMS kernel from source instead of a released
`rtems-source-builder` bset.

A real `docker build` got through package install and user setup, then into the
actual `sb-set-builder 7/rtems-riscv` toolchain step - confirming the bset name and
RSB's mirror-fallback logic are correct - but every upstream source host it needs
(`dl.rtems.org`, `www.kernel.org`, `ftpmirror.gnu.org`, `sourceware.org`,
`gcc.gnu.org`, `gitlab.rtems.org`) was unreachable from that sandbox's egress
policy, which only permits GitHub/PyPI-style hosts. So the toolchain build itself
has never completed - it needs to run somewhere with normal internet access (a
workstation or CI runner). Cross-check each step against the
[riscv BSPs page](https://docs.rtems.org/docs/main/user/bsps/bsps-riscv.html)
before relying on it, and expect to iterate on the Dockerfile once it can actually
build end-to-end.

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

Once the kernel/BSP install above succeeds, an application is built and linked
against it with the `riscv-rtems7-gcc` toolchain, following the same
RTEMS-configuration-object + link pattern used in `rtems_ubuntu_build.md`'s SPARC/
RISC-V examples, but targeting `riscv/esp32c3db` instead of `sparc/leon3` or
`riscv/rv64imafdc`.

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
