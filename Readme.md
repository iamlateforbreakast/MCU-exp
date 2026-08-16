# Various microcontroller development project

This repo contains a series of processes to support the development of software for various MCUs,
each targeted from its own container.

Docker files are provided:
- One to build rtems 7 and compile binary image with SPARC and RISC architectures
- One with Renode to execute the compiled binaries
- One with the ESP-IDF toolchain to develop for the ESP32-C3
- One with the Pico SDK toolchain to develop for the RP2040
- One with the Realtek AmebaD SDK toolchain to develop for the RTL8720DN (BW16)
- One based on the vendor image to develop for the Luckfox Pico (RV1103/RV1106)
- One with the Ai-M6X SDK toolchain to develop for the AI Thinker M62 (M62-M2-I-Kit)

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

## RP2040 development

```
podman compose up -d rp2040-dev
podman exec -it rp2040-dev /bin/bash
cmake -S . -B build -G Ninja
cmake --build build
```

See `workspace/RPI2040/RaspberryPico.md` for details, including flashing over the board's
BOOTSEL mass-storage mode.

## RTL8720DN (BW16) development

```
podman compose up -d rtl8720dn-dev
podman exec -it rtl8720dn-dev /bin/bash
cd ~/ameba-rtos/project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp && make all
cd ../project_hp && make all
```

See `workspace/RTL8720DN/RTL8720DN.md` for details, including flashing over USB-serial.

## Luckfox Pico development

```
git clone https://github.com/LuckfoxTECH/luckfox-pico.git workspace/Luckfoxpico/luckfox-pico
podman compose up -d luckfoxpico-dev
podman exec -it luckfoxpico-dev /bin/bash
```

See `workspace/Luckfoxpico/luckfoxpic.md` for details. This container runs privileged
(needed for the Buildroot SDK's rootfs build) - only use it on a trusted host.

## AI Thinker M62 development

```
podman compose up -d aim62-dev
podman exec -it aim62-dev /bin/bash
cd $AIM62_SDK_PATH/examples/helloworld
```

See `workspace/AI-MF62-AiThinker/AI-MF62-AiThinker.md` for details.

