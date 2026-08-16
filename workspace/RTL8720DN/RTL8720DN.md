# RTL8720DN (BW16) development container

The `rtl8720dn-dev` container (see `Dockerfile.rtl8720dn` / `compose.yaml`) provides the
arm-none-eabi GCC toolchain, the Realtek AmebaD SDK (cloned to `~/ameba-rtos`), and the
`ameba_bw16_autoflash` flash tool (cloned to `~/ameba_bw16_autoflash`).

## Usage

```
podman compose up -d rtl8720dn-dev
podman exec -it rtl8720dn-dev /bin/bash
```

## Building a project

The BW16 requires building for two separate cores (KM0 for Low Power, KM4 for High Power):

```
cd ~/ameba-rtos/project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp
make all
cd ../project_hp
make all
```

## Flashing

Pass the board's USB-serial device through via the commented `devices` entry in
`compose.yaml`, then use `~/ameba_bw16_autoflash` (see its own README) or the Realtek
`upload_image_tool_linux` against `/dev/ttyUSB0` as described below.

---

# Manual setup notes (outside the container)

How to compile projects for the AI Thinker BW16 (RTL8720DN) on Fedora 42 in WSL

## GCC toolchain for ARM M0 and M3

```
sudo dnf update
sudo dnf install gcc-arm-none-eabi binutils-arm-none-eabi gdb-arm-none-eabi build-essential
```

## Install the flash tool

flash tool
https://github.com/jojoling/ameba_bw16_autoflash/blob/main/README.md

## Install Realtek SDK

Option 1: Clone the old SDK: Download or clone the Ameba-AIoT/ameba-rtos-d repository from GitHub.

Option 2: Clone the new SDK: Download or clone the (Ameba-AIoT/ameba-rtos repository)[https://github.com/Ameba-AIoT/ameba-rtos]

Add the following to your ~/.bashrc.d in a file sdk_rtl8720dn.bashrc

```
export RTK_TOOLCHAIN_DIR="/path/to/your/arm-none-eabi/bin".
```

## Compile the Realtek SDK

Install python

```
sudo dnf install python3 python3-pip python3-venv
sudo dnf install openssl-devel ncurses-devel
```

## Create project

## Compile project
3. Build the Project
The BW16 requires building for two separate cores (KM0 for Low Power and KM4 for High Power). 
Low Power Core (KM0):
bash
cd project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp
make all
Use code with caution.

High Power Core (KM4):
bash
cd ../project_hp
make all
Use code with caution.
Place Your Code: Your custom C source code should be placed in the project directory: ambd_sdk\project\realtek_amebaD_va0_example\src\src_hp\main.c.
GCC/Command Line: You can also use the provided Makefiles in the GCC-RELEASE folder to compile your project from a terminal. 

Output Files: After a successful build, the SDK generates three critical .bin files in the asdk\image folder:
km0_boot_all.bin (Low Power core bootloader)
km4_boot_all.bin (High Power core bootloader)
km0_km4_image2.bin (The main application image) 

## Flash binaries on board

Consider a directory 

Linux usage : upload_image_tool_linux . /dev/ttyUSB0 --verbose=3 --baudrate=115200

Windows usage : upload_image_tool_windows.exe C:\Users\username\AppData\Local\Arduino15\packages\realtek\tools\ameba_d_tools\1.0.8 COM3 ameba_rtl8721d Disable Disable

-------------------------------------------

To program the AI Thinker BW16 (RTL8720DN) using the Realtek Standard SDK, you must use the AmebaD SoC SDK hosted on GitHub. This method bypasses the Arduino framework entirely, giving you low-level access to the Cortex-M33 (High Power) and Cortex-M23 (Low Power) cores. 


To set up the GCC toolchain for the AI Thinker BW16 (RTL8720DN) in 2026, you will need the standard ARM cross-compiler and the specific build environment required by the Realtek AmebaD SDK. 
1. Download and Install the ARM GNU Toolchain
The BW16 uses an ARM Cortex-M33 (High Power) and Cortex-M23 (Low Power) core. You need the arm-none-eabi-gcc toolchain. 
Linux (Ubuntu/Debian):
bash


 




