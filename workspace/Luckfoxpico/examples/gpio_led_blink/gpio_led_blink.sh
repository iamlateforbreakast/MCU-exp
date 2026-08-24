#!/bin/sh
# Drives three LEDs on the Luckfox Pico, one after another, using the kernel sysfs
# GPIO interface. Runs directly on the board (over SSH/serial console), not in the
# luckfoxpico-dev container - that container is for cross-building the SDK/rootfs,
# not for running on-target scripts.
#
# Adjust LED_PINS below to match how the LEDs are actually wired - these are
# placeholders. Luckfox's sysfs pin number = (controller * 32) + (port_letter * 8) + pin,
# with port A=0, B=1, C=2, D=3 - e.g. GPIO1_C7 = (1 * 32) + (2 * 8) + 7 = 55.
# See ../../luckfoxpic.md and https://wiki.luckfox.com for your board's pinout.

LED_PINS="55 56 57"

cleanup() {
    for pin in $LED_PINS; do
        [ -d "/sys/class/gpio/gpio$pin" ] && echo 0 > "/sys/class/gpio/gpio$pin/value"
    done
    exit 0
}
trap cleanup INT TERM

for pin in $LED_PINS; do
    [ -d "/sys/class/gpio/gpio$pin" ] || echo "$pin" > /sys/class/gpio/export
    echo out > "/sys/class/gpio/gpio$pin/direction"
    echo 0 > "/sys/class/gpio/gpio$pin/value"
done

active_index=0
count=0
for pin in $LED_PINS; do
    count=$((count + 1))
done

while true; do
    i=0
    for pin in $LED_PINS; do
        if [ "$i" -eq "$active_index" ]; then
            echo 1 > "/sys/class/gpio/gpio$pin/value"
        else
            echo 0 > "/sys/class/gpio/gpio$pin/value"
        fi
        i=$((i + 1))
    done
    active_index=$(( (active_index + 1) % count ))
    sleep 0.3
done
