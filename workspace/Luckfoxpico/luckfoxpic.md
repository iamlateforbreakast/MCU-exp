# Luckfox Pico mini B description

# Luckfox Pico development container

The `luckfoxpico-dev` container (see `Dockerfile.luckfoxpico` / `compose.yaml`) wraps the
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

Inside the container, the SDK is available under `~/workspace/Luckfoxpico/luckfox-pico`.

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
