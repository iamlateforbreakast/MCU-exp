format:
upload_image_tool_windows.exe "{runtime.tools.ameba_d_tools.path}" "{serial.port}" "{board}" "{build.auto_upload_mode}" "{build.erase_flash}" "1500000" {upload.verbose}

usage example:
upload_image_tool_windows.exe C:\Users\username\AppData\Local\Arduino15\packages\realtek\tools\ameba_d_tools\1.0.8 COM3 ameba_rtl8721d Disable Disable

-------------------------------------------

To program the AI Thinker BW16 (RTL8720DN) using the Realtek Standard SDK, you must use the AmebaD SoC SDK hosted on GitHub. This method bypasses the Arduino framework entirely, giving you low-level access to the Cortex-M33 (High Power) and Cortex-M23 (Low Power) cores. 
1. Download the SDK and Tools
Clone the SDK: Download or clone the Ameba-AIoT/ameba-rtos-d repository from GitHub.
Get the Image Tool: The Windows-based utility for flashing firmware is located within the SDK folder at ambd_sdk\tools\AmbaD\Image_tool\image_tool.exe.
Compiler Toolchain: For a C/C++ environment, you typically need the GCC toolchain for ARM (Cortex-M33). Realtek provides specific toolchain patches (e.g., for Windows, Linux, or macOS) on their software summary page. 

2. Set Up the Development Environment
Place Your Code: Your custom C source code should be placed in the project directory: ambd_sdk\project\realtek_amebaD_va0_example\src\src_hp\main.c.
GCC/Command Line: You can also use the provided Makefiles in the GCC-RELEASE folder to compile your project from a terminal. 

3. Build and Compile
Build Project: In IAR, select Project > Rebuild All.
Output Files: After a successful build, the SDK generates three critical .bin files in the asdk\image folder:
km0_boot_all.bin (Low Power core bootloader)
km4_boot_all.bin (High Power core bootloader)
km0_km4_image2.bin (The main application image) 

4. Flash the Firmware (Using Image Tool)
Enter Download Mode: Connect the BW16 via USB-C. Press and hold the Burn button, press and release the RST button, then release the Burn button.
Configure Image Tool:
Chip Select: Choose AmebaD (8721D), which is compatible with the RTL8720DN chip on the BW16.
Select Port: Choose the COM port assigned to your board at 115200 baud.
Load Images: Click Browse and select the three .bin files mentioned above. Ensure the target addresses are set to the SDK defaults.
Download: Click the Download button. Once finished, press RST on the board to run your code. 
Do you need help setting up the GCC toolchain specifically for a Linux or macOS environment, or would you like to see a basic C code example for the BW16?


To set up the GCC toolchain for the AI Thinker BW16 (RTL8720DN) in 2026, you will need the standard ARM cross-compiler and the specific build environment required by the Realtek AmebaD SDK. 
1. Download and Install the ARM GNU Toolchain
The BW16 uses an ARM Cortex-M33 (High Power) and Cortex-M23 (Low Power) core. You need the arm-none-eabi-gcc toolchain. 
Linux (Ubuntu/Debian):
bash
sudo apt update
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi gdb-arm-none-eabi build-essential
Use code with caution.

2. Configure the SDK Environment Variables
The SDK needs to know where your toolchain is located.
Locate the SDK: Navigate to your cloned ameba-rtos-d directory.
Set the Path:
Linux/macOS: Add the following to your ~/.bashrc or ~/.zshrc:
export RTK_TOOLCHAIN_DIR="/path/to/your/arm-none-eabi/bin".
 
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

Output: Successful builds will generate the required .bin files in the asdk/image subdirectories.

4. Flash the Firmware
Once compiled, use the AmebaD Image Tool (Windows) to upload.
Tool Location: ambd_sdk/tools/AmebaD/Image_Tool/ImageTool.exe.
Process: Select the generated images (km0_boot_all.bin, km4_boot_all.bin, and km0_km4_image2.bin) and click Download while the board is in Download Mode (Hold Burn + Press RST). 
