

## GCC toolchain for the AI Thinker BW16 (RTL8720DN)

sudo apt update
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi gdb-arm-none-eabi build-essential

## Install Realtek SDK

Clone the SDK: Download or clone the Ameba-AIoT/ameba-rtos-d repository from GitHub.

Linux/macOS: Add the following to your ~/.bashrc or ~/.zshrc:
export RTK_TOOLCHAIN_DIR="/path/to/your/arm-none-eabi/bin".

## Path Realtek SDK

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


 




