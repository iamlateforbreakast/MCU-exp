# Using the AI Thinker M62-M2-I-Kit
---------------------------------

# AI-M62 development container

The `aim62-dev` container (see `Dockerfile.aim62` / `compose.yaml`) provides a baseline
riscv64-unknown-elf toolchain plus the Ai-M6X SDK (cloned to `$AIM62_SDK_PATH`, submodules
initialized under `tools/`). Per the SDK's own convention, projects should use the cross
compiler from `$AIM62_SDK_PATH/tools` rather than the distro-installed one.

## Usage

```
podman compose up -d aim62-dev
podman exec -it aim62-dev /bin/bash
cd $AIM62_SDK_PATH/examples/helloworld
```

## Flashing

Pass the board's USB-serial device through via the commented `devices` entry in
`compose.yaml`.

---

# Manual setup notes (outside the container)

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



