# GPIO LED blink (Luckfox Pico)

Runs on the board itself (not in the `luckfoxpico-dev` container, which is for
cross-building the SDK/rootfs).

## sysfs (works out of the box on most images)

```
chmod +x gpio_led_blink.sh
scp gpio_led_blink.sh root@<board-ip>:/root/
ssh root@<board-ip> /root/gpio_led_blink.sh
```

## libgpiod alternative

If `libgpiod` is enabled in the board's Buildroot config, the same effect can be done
per-LED with the `gpioset` CLI tool (see `gpioinfo` to find the right chip/line), e.g.:

```
gpioset gpiochip1 7=1   # LED on
gpioset gpiochip1 7=0   # LED off
```

Adjust the chip/line numbers and `LED_PINS` in `gpio_led_blink.sh` to match your
board's actual wiring - see https://wiki.luckfox.com for your model's pinout.
