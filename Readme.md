# Various microcontroller development project

This repo contains a series of processes to support the development of software for various MCUs,
each targeted from its own container.

Docker files are provided:
- One to build rtems 7 and compile binary image with SPARC and RISC architectures
- One with Renode to execute the compiled binaries
- One with the ESP-IDF toolchain to develop for the ESP32-C3

## Building RTEMS 7

TBC

## Compiling a binary image for SPARC and RISCV

TBC

```
podman compose up -d
podman exec -it rtems-dev /bin/bash
```

Convert an exe to elf:

```
riscv-rtems6-objcopy sample.exe sample.elf
riscv-rtems6-readelf -h sample.elf | grep "Entry point address"
```

## Executing a binary image with Renode

https://www.maskset.net/blog/2025/08/27/renode-docker-setup-on-ubuntu-24.04/

## ESP32-C3 development

```
podman compose up -d esp32c3-dev
podman exec -it esp32c3-dev /bin/bash
idf.py set-target esp32c3
idf.py build
```

See `workspace/ESP32-C3/ESP32-C3.md` for details, including flashing over the board's USB-CDC
serial port.

