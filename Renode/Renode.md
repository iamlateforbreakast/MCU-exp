How to install renode in fedora 43

Pull renode image:

`podman pull antmicro/renode:latest`

Run Headless (Recommended for CI/Scripts):

`podman run -it --rm -p 1234:1234 antmicro/renode:latest`

Run with GUI (Interactive):

This is not working due to X11 connection to wayland

```
xhost +local:podman # Grant permission to local containers
podman run -it --rm \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix:Z \
  antmicro/renode:latest
```

Volumes & Permissions: In Podman's rootless mode, files mounted from your host might have permission issues if the container expects "root" access. Using the --userns=keep-id flag can often solve this.

Networking: If you need to connect from your host to a Renode port (like telnet), ensure you map the port using -p 1234:1234

In renode start

`include @script/single-node/zynqmp_linux.resc`

`start`

In headless mode, standard graphical UART windows are disabled. You can still view or interact with UART output using these three primary methods: 
Renode - documentation
Renode - documentation
 +1
1. Direct Console Switching (Easiest)
If you start Renode with the --no-gui flag, it enters a "console headless" mode where the terminal is shared between the Monitor and UART. 
Read the Docs
Read the Docs
Default View: You are typically placed directly in the UART session.
Switch to Monitor: Press <ESC>.
Switch back to UART: Enter uart_connect sysbus.uart in the Monitor (replacing sysbus.uart with your actual peripheral path). 
Read the Docs
Read the Docs
 +1
2. Network Socket (Best for Podman) 
You can expose the UART over a network port and connect to it using telnet from your host. This is ideal for containers since you can map the port to your host machine. 
Renode - documentation
Renode - documentation
 +1
In your .resc file or Monitor:
bash
emulation CreateServerSocketTerminal 12345 "externalUART"
connector Connect sysbus.uart0 externalUART
Use code with caution.
From your host terminal:
bash
telnet localhost 12345
Use code with caution.
 
3. Log Redirection
In headless mode, UART output is often redirected to the Renode log by default. You can also force it to a specific file on the host for easier analysis. 
Renode - documentation
Renode - documentation
 +1
Log to a file:
bash
(machine-0) sysbus.uart0 CreateFileBackend @/path/to/output.log
Use code with caution.
View in real-time:
Open another terminal and use tail -f /path/to/output.log.
