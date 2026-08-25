# Luckfox Pico mini B description

# Luckfox Pico development container

The `luckfoxpico-dev` container (see `Containerfile.luckfoxpico` / `compose.yaml`) wraps the
vendor's official `luckfoxtech/luckfox_pico:1.0` image. Unlike the other MCU containers in
this repo, the Buildroot-based SDK is a multi-GB tree that isn't baked into the image -
clone it into the bind-mounted workspace instead (see below), matching the vendor's own
`-v /path/to/luckfox-pico:...` usage.

The container runs `privileged: true` because Buildroot's rootfs build needs loop-mount/
chroot access - only run this service on a trusted host.

## Usage

```
git clone https://github.com/LuckfoxTECH/luckfox-pico.git workspace/Luckfoxpico/luckfox-pico
podman compose up -d luckfoxpico-dev
podman exec -it luckfoxpico-dev /bin/bash
```

Inside the container, the SDK is available under `~/workspace/luckfox-pico`.

## GPIO LED example

`examples/gpio_led_blink/gpio_led_blink.sh` drives three LEDs in sequence via the sysfs
GPIO interface - it runs directly on the board (over SSH/serial), not in this
cross-build container. See `examples/gpio_led_blink/README.md` for usage and a
`libgpiod` alternative.

## ST7789 SPI LCD example

`examples/lcd_st7789_spi/` drives a 1.14" ST7789 SPI TFT from Linux userspace via
`/dev/spidev0.0`. Needs `spi0` enabled in the board dts first (disabled by default) - see
`examples/lcd_st7789_spi/README.md` for the kernel rebuild/reflash steps, wiring, and
build/deploy instructions (no ssh/scp on this image, only telnet).

## Firmware output

`compose.yaml` also mounts a separate `firmware/Luckfoxpico` host directory to
`~/firmware` inside the container - copy the Buildroot-built images there so they're
easy to find on the host (Buildroot's own output images land under `output/images/`
within the SDK):

```
cp ~/workspace/luckfox-pico/output/images/*.img ~/firmware/
```

---

# Manual setup notes (outside the container)

# Install Luckfox SDK

`sudo dnf install podman podman-docker podman-compose`

Enable the Podman socket if you plan to use tools that expect a background daemon

`sudo systemctl enable --now podman.socket`

Pull the docker image:

`sudo docker pull luckfoxtech/luckfox_pico:1.0`

Install the SDK

`git clone https://github.com/LuckfoxTECH/luckfox-pico.git`

Run container

```
podman run -it --name luckfox_devel --privileged \
  -v /path/to/luckfox-pico:/home/luckfox-pico:Z \
  luckfoxtech/luckfox_pico:1.0 /bin/bash
```
