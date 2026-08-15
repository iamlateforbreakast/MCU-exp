# ESP32-C3 development

The `esp32c3-dev` container (see `Dockerfile.esp32c3` / `compose.yaml`) provides a RISC-V
ESP-IDF toolchain scoped to the ESP32-C3 target.

## Usage

```
podman compose up -d esp32c3-dev
podman exec -it esp32c3-dev /bin/bash
```

`esp-idf/export.sh` is sourced automatically for interactive shells, so `idf.py` is available
directly.

## Creating and building a project

```
cp -r $IDF_PATH/examples/get-started/hello_world ~/workspace/hello_world
cd ~/workspace/hello_world
idf.py set-target esp32c3
idf.py build
```

## Flashing and monitoring

The ESP32-C3 exposes a native USB-CDC serial port (typically `/dev/ttyACM0` on Linux). Pass it
through to the container via the `devices` entry in `compose.yaml`, then:

```
idf.py -p /dev/ttyACM0 flash monitor
```
