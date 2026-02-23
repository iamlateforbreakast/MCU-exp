# Luckfox Pico mini B description


# Install Luckfox SDK

`sudo dnf install podman podman-docker podman-compose`

Enable the Podman socket if you plan to use tools that expect a background daemon

`sudo systemctl enable --now podman.socket`

Pull the docker image:

`sudo docker pull luckfoxtech/luckfox_pico:1.0`
