How to install renode in fedora 43

Pull renode image:

`podman pull antmicro/renode:latest`

Run Headless (Recommended for CI/Scripts):

`podman run -it --rm -p 1234:1234 antmicro/renode:latest`

Run with GUI (Interactive):

```
xhost +local:podman # Grant permission to local containers
podman run -it --rm \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix:Z \
  antmicro/renode:latest
```

Volumes & Permissions: In Podman's rootless mode, files mounted from your host might have permission issues if the container expects "root" access. Using the --userns=keep-id flag can often solve this.

Networking: If you need to connect from your host to a Renode port (like telnet), ensure you map the port using -p 1234:1234
