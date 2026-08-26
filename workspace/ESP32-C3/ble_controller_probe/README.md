# BLE controller probe (real-ESP-IDF control experiment)

A real ESP-IDF v5.3.1 project (build with the `esp32c3-dev` container -
see `../ESP32-C3.md`) used as a control experiment for
`../../ESP32-C3-RTEMS/upstream-bt-driver/`'s "BLE assert emi.c ..."
investigation - not a general-purpose example.

Does the exact same minimal sequence as
`../../ESP32-C3-RTEMS/examples/ble_vhci_smoke/init.c`: init the BLE
controller, enable it, register a VHCI callback, send one HCI Reset,
check the response. No NimBLE/Bluedroid host
(`CONFIG_BT_CONTROLLER_ONLY=y`).

**Confirmed on real hardware (2026-08-26): PASS.** This container's IDF
checkout is a `--recursive` clone of the v5.3.1 tag, so its
`lib_esp32c3_family` submodule pin (`bfdfe8f851c99ced8316b133b
0a90deb92efd`) is the exact same closed-blob commit the RTEMS port's
first, self-consistent pairing used - proving that pairing (and the
board/blob) is fine, and the assert the RTEMS port hits with it is
caused by something in that port's platform layer, not the blob. See
`../../ESP32-C3-RTEMS`'s `esp32c3_rtems_ble_driver_status` memory/docs
for the full writeup and what it led to fixing.

```
idf.py set-target esp32c3
idf.py build
```

Flash from the host (not inside the container - see `../ESP32-C3.md`'s
"Flashing and monitoring" section for why):
```
python -m esptool --chip esp32c3 -p /dev/ttyACM0 -b 460800 --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 2MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/ble_controller_probe.bin
```
