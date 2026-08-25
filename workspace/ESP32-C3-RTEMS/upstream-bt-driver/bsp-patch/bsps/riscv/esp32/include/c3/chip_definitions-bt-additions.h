/*
 * NOT a standalone header - a fragment meant to be merged directly into
 * the existing `#define`s in bsps/riscv/esp32/include/c3/chip_definitions.h
 * (the real upstream RTEMS file this repo doesn't vendor), next to its
 * existing UHCI0_INTR/GPIO_PROCPU_INTR/etc. block. See ../../../../README.md
 * ("Not started - the actual BSP patch" section) for why these values and
 * not others.
 *
 * Values confirmed against real ESP-IDF v5.3.1 source
 * (components/soc/esp32c3/include/soc/interrupts.h, the
 * `interrupt_source_t` enum - "this table is decided by hardware, don't
 * touch this" per its own doc comment). Matches this BSP's own numbering:
 * chip_definitions.h's existing UHCI0_INTR=15/GPIO_PROCPU_INTR=16 line up
 * exactly with the same enum's ETS_UHCI0_INTR_SOURCE=15/
 * ETS_GPIO_INTR_SOURCE=16, confirming these are the same interrupt-matrix
 * numbering, not values needing translation.
 *
 * All of 4-10 are defined even though only RWBLE_INTR is wired into
 * irq_mappings[] initially (see chip_definitions.h's own naming
 * convention: names, not raw numbers, are what's used at call sites) -
 * bt.c's closed blob supplies the actual source number it wants for
 * esp_intr_alloc() at runtime (not visible in open source - see README),
 * so RWBLE_INTR is a reasoned best guess, not a confirmed fact. Having the
 * others defined makes wiring a different one into irq_mappings[] a
 * one-line fix if Phase 3's hardware test shows this guess was wrong.
 */
#define BT_MAC_INTR   4  /* ETS_BT_MAC_INTR_SOURCE - IDF's own doc comment says "will be cancelled" */
#define BT_BB_INTR    5  /* ETS_BT_BB_INTR_SOURCE */
#define BT_BB_NMI     6  /* ETS_BT_BB_NMI_SOURCE */
#define RWBT_INTR     7  /* ETS_RWBT_INTR_SOURCE - classic-BT baseband; ESP32-C3 has no classic BT */
#define RWBLE_INTR    8  /* ETS_RWBLE_INTR_SOURCE - BLE baseband; best-guess candidate for what bt.c's blob actually requests */
#define RWBT_NMI      9  /* ETS_RWBT_NMI_SOURCE */
#define RWBLE_NMI    10  /* ETS_RWBLE_NMI_SOURCE */
