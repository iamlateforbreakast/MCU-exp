# Using the AI Thinker M62-M2-I-Kit
---------------------------------

## Install toolchain

`sudo dnf install gcc-riscv64-linux-gnu-gnu`

## Install SDK

`git clone https://github.com/Ai-Thinker-Open/aithinker_Ai-M6X_SDK`

`cd tools`
`git submodule init`
`git submodule update .`

## Install tools

## Compile example

`cd exampls/helloworld`

cmake points to the tools directory insted of distro installed
cross compiler is riscv64-unknown-elf-gnu-gcc instead of distro



