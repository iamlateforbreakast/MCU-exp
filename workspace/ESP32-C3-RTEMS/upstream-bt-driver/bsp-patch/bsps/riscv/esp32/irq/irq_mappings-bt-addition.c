/*
 * NOT a standalone file - a fragment showing the one entry to append to
 * the existing `irq_mappings[]` array in
 * bsps/riscv/esp32/irq/irq_c3.c (the real upstream RTEMS file this repo
 * doesn't vendor). See ../../../../README.md ("Not started - the actual
 * BSP patch" section) for the full table this appends to and why sharing
 * a `cpu_int` line is safe here.
 *
 * `cpu_int = 7` is shared with the existing EFUSE_INTR/LEDC_INTR entries -
 * chosen because both are low-frequency in a system without active LEDC
 * PWM output, not because it's the only option. This is a judgment call
 * documented as such, not a hard requirement - any already-assigned line
 * works, since dispatch resolves interrupts down to the individual
 * peripheral source by scanning the interrupt-matrix status register, not
 * just the shared `cpu_int` line (confirmed this session against the real
 * irq_c3.c dispatch code).
 */
{ .peripheral_int = RWBLE_INTR, .cpu_int = 7 },
