
# RP2040 development

The `rp2040-dev` container (see `Dockerfile.rp2040` / `compose.yaml`) provides the
arm-none-eabi GCC toolchain and the Pico SDK (cloned to `$PICO_SDK_PATH`) for the RP2040.

## Usage

```
podman compose up -d rp2040-dev
podman exec -it rp2040-dev /bin/bash
```

## Building a project

```
cp -r $PICO_SDK_PATH/../pico-examples/hello_world ~/workspace/hello_world 2>/dev/null || true
cd ~/workspace/hello_world
cmake -S . -B build -G Ninja
cmake --build build
```

## Flashing

Hold BOOTSEL while plugging in the board (or while resetting it) so it mounts as a USB mass
storage device, then copy the built `.uf2` file onto it:

```
cp build/hello_world.uf2 /media/$USER/RPI-RP2/
```

# Install SDK (manual, outside the container)

`git clone https://github.com/raspberrypi/pico-sdk.git`
