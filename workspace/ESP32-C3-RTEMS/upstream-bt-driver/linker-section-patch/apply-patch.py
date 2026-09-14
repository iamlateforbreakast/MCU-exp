#!/usr/bin/env python3
"""
Applies the BT/BLE section-placement patch (README.md in this directory)
to an already-built esp32c3db BSP's installed linkcmds.base. Idempotent:
matches by the exact real text these edits sit next to, not by line
number (linkcmds.base is waf-generated, not a fixed-line-count file), and
skips silently if already applied.

Usage: python3 apply-patch.py $RTEMS_ROOT/riscv-rtems7/esp32c3db/lib/linkcmds.base
       python3 apply-patch.py $RTEMS_ROOT/riscv-rtems7/esp32c3db/lib/linkcmds

Validated with a real link 2026-08-26 - see README.md's "Validated with a
real link" section for the exact addresses this produced.
"""
import sys

def patch(content):
    if "_bt_data_start" in content:
        print("Already patched - no changes made.")
        return content, False

    # .iram1/.coexiram go into .fast_text, NOT .text (corrected 2026-09-14).
    # Putting them in .text left them flash-mapped and executing via XIP, and
    # that was measured to be the reason BLE never transmits: the BT
    # controller's ISR (btdm_rw_run) ran at 1314 us mean / 2912 us max, versus
    # a scheduler programming budget of 3 half-slots (~940 us), so every
    # advertising event missed its deadline. Real IDF puts this same code in
    # IRAM (btdm_rw_run at 0x40384216 there, 0x42062ec8 here). RTEMS's
    # .fast_text section already has a load/VMA split and bsps/riscv/esp32/
    # start/bspstart.c already memcpys it at boot - it was simply aliased to
    # flash. See the companion edit to `linkcmds` in patch_regions().
    fast_anchor = "    *(.bsp_fast_text)\n"
    if fast_anchor not in content:
        sys.exit("ERROR: .bsp_fast_text anchor not found - linkcmds.base format may have changed, see README.md")
    fast_insert = (
        "    *(.iram1 .iram1.*)\n"
        "    *(.coexiram .coexiram.*)\n"
        "    /* Platform hot path, mirroring what real IDF keeps in .iram0.text\n"
        "     * (~70 KB there versus ~24 KB of blob .iram1 alone). Every BT\n"
        "     * interrupt goes through RTEMS dispatch, and running that from XIP\n"
        "     * flash costs the BLE scheduler its ~940 us programming budget.\n"
        "     * NOT memcpy/memset: bspstart.c populates .fast_text by calling\n"
        "     * them, so a copy living here is absent when that runs and the\n"
        "     * board TG0WDT boot-loops. They must stay flash-resident. */\n"
        "    *librtemsbsp.a:irq_c3*.o(.text .text.*)\n"
    )
    content = content.replace(fast_anchor, fast_anchor + fast_insert, 1)

    # irq_c3 must also be EXCLUDEd from the generic .text rule, which appears
    # EARLIER in this script and would otherwise win - ld assigns each input
    # section to the FIRST output section matching it. Two subtleties, both
    # found the hard way: EXCLUDE_FILE applies only to the section pattern
    # IMMEDIATELY following it, so it has to be repeated before each one; and
    # RTEMS is built with -ffunction-sections, so its code lives in
    # .text.<name> and an exclusion covering only `.text` silently misses it.
    text_anchor = "    *(.text .stub .text.* .gnu.linkonce.t.*)\n"
    if text_anchor not in content:
        sys.exit("ERROR: .text anchor not found - linkcmds.base format may have changed, see README.md")
    ex = "EXCLUDE_FILE(*irq_c3*.o)"
    content = content.replace(
        text_anchor,
        "    *(%s .text %s .stub %s .text.* %s .gnu.linkonce.t.*)\n" % (ex, ex, ex, ex),
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


def patch_regions(content):
    """Edits `linkcmds` (the region file) rather than `linkcmds.base`.

    Carves the top 64 KB of the BSP's RAM region out and re-exposes it through
    the ESP32-C3's instruction-bus alias so .fast_text can actually execute
    from SRAM. On this chip SRAM1 is reachable from both buses at a fixed
    offset - soc.h: SOC_DIRAM_DRAM_LOW 0x3FC80000, SOC_DIRAM_IRAM_LOW
    0x40380000, so SOC_I_D_OFFSET is 0x700000 - meaning DRAM 0x3fcc0000 and
    IRAM 0x403c0000 are the same physical memory. Shrinking RAM to 0x40000 and
    giving IRAM the 0x10000 above it keeps the two disjoint.

    FAST_TEXT_LOAD is pointed at DATA_FLASH_RAW (not CODE_FLASH_RAW) because
    bspstart.c's copy_from_flash_offset() resolves load addresses through the
    data-flash window at 0x3c000000 - the same path the already-working .data
    copy uses.
    """
    if "IRAM :" in content:
        print("Region file already patched - no changes made.")
        return content, False

    ram_anchor = "  RAM : ORIGIN = 0x3fc80000, LENGTH = 0x50000\n"
    if ram_anchor not in content:
        sys.exit("ERROR: RAM region not found - linkcmds format may have changed, see README.md")
    content = content.replace(
        ram_anchor,
        "  RAM : ORIGIN = 0x3fc80000, LENGTH = 0x40000\n"
        "  IRAM : ORIGIN = 0x403c0000, LENGTH = 0x10000\n",
        1,
    )

    for old, new in (
        ('REGION_ALIAS ("REGION_FAST_TEXT", CODE_FLASH_MAPPED);',
         'REGION_ALIAS ("REGION_FAST_TEXT", IRAM);'),
        ('REGION_ALIAS ("REGION_FAST_TEXT_LOAD", CODE_FLASH_RAW);',
         'REGION_ALIAS ("REGION_FAST_TEXT_LOAD", DATA_FLASH_RAW);'),
    ):
        if old not in content:
            sys.exit(f"ERROR: alias not found: {old}")
        content = content.replace(old, new, 1)

    return content, True


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {sys.argv[0]} <path-to-linkcmds.base>")
    path = sys.argv[1]
    with open(path) as f:
        original = f.read()
    patched, changed = (patch_regions if path.endswith("linkcmds") else patch)(original)
    if changed:
        with open(path, "w") as f:
            f.write(patched)
        print(f"Patched {path}")
