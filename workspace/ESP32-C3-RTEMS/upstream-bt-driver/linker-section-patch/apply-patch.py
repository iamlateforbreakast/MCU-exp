#!/usr/bin/env python3
"""
Applies the BT/BLE section-placement patch (README.md in this directory)
to an already-built esp32c3db BSP's installed linkcmds.base. Idempotent:
matches by the exact real text these edits sit next to, not by line
number (linkcmds.base is waf-generated, not a fixed-line-count file), and
skips silently if already applied.

Usage: python3 apply-patch.py $RTEMS_ROOT/riscv-rtems7/esp32c3db/lib/linkcmds.base

Validated with a real link 2026-08-26 - see README.md's "Validated with a
real link" section for the exact addresses this produced.
"""
import sys

def patch(content):
    if "_bt_data_start" in content:
        print("Already patched - no changes made.")
        return content, False

    text_anchor = "    *(.text .stub .text.* .gnu.linkonce.t.*)\n"
    if text_anchor not in content:
        sys.exit("ERROR: .text anchor not found - linkcmds.base format may have changed, see README.md")
    content = content.replace(
        text_anchor,
        text_anchor + "    *(.iram1 .iram1.*)\n    *(.coexiram .coexiram.*)\n",
        1,
    )

    data_anchor = "    *(.data .data.* .gnu.linkonce.d.*)\n    SORT(CONSTRUCTORS)"
    if data_anchor not in content:
        sys.exit("ERROR: .data anchor not found - linkcmds.base format may have changed, see README.md")
    data_insert = (
        "    _bt_data_start = .;\n"
        "    *bt.o(.data .data.* .gnu.linkonce.d.*)\n"
        "    _bt_data_end = .;\n"
        "    _bt_controller_data_start = .;\n"
        "    *libbtdm_app.a:*(.data .data.* .gnu.linkonce.d.*)\n"
        "    _bt_controller_data_end = .;\n"
        "    *(.data .data.* .gnu.linkonce.d.*)\n"
        "    *(.dram1 .dram1.*)"
    )
    content = content.replace(data_anchor, data_insert + "\n    SORT(CONSTRUCTORS)", 1)

    bss_anchor = "    *(.dynbss)\n    *(.bss .bss.* .gnu.linkonce.b.*)"
    if bss_anchor not in content:
        sys.exit("ERROR: .bss anchor not found - linkcmds.base format may have changed, see README.md")
    bss_insert = (
        "    *(.dynbss)\n"
        "    _bt_bss_start = .;\n"
        "    *bt.o(.bss .bss.* .gnu.linkonce.b.*)\n"
        "    _bt_bss_end = .;\n"
        "    _bt_controller_bss_start = .;\n"
        "    *libbtdm_app.a:*(.bss .bss.* .gnu.linkonce.b.*)\n"
        "    _bt_controller_bss_end = .;\n"
        "    *(.bss .bss.* .gnu.linkonce.b.*)"
    )
    content = content.replace(bss_anchor, bss_insert, 1)

    return content, True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {sys.argv[0]} <path-to-linkcmds.base>")
    path = sys.argv[1]
    with open(path) as f:
        original = f.read()
    patched, changed = patch(original)
    if changed:
        with open(path, "w") as f:
            f.write(patched)
        print(f"Patched {path}")
