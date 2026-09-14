/*
 * Implementation of the serial diagnostics declared in ble_diag.h. Compiled
 * to nothing unless BLE_DIAG=1 - see that header for what each piece is for
 * and why it exists.
 */
#include "ble_diag.h"

#if BLE_DIAG

#include <rtems/bspIo.h>
#include <rtems/config.h>
#include <rtems/score/cpu.h>
#include <rtems/score/isr.h>
#include <rtems/stackchk.h>

/* ESP32-C3 interrupt matrix (soc/reg_base.h DR_REG_INTERRUPT_BASE, and the
 * register offsets irq_c3.c itself uses). Source N's routing register is at
 * base + N*4 and holds the CPU interrupt number it is mapped to; 0 means
 * "not routed anywhere". */
#define DIAG_INT_MATRIX_BASE   0x600c2000u
#define DIAG_INT_CPU_ENABLE    0x104u
#define DIAG_INT_CPU_TYPE      0x108u
#define DIAG_INT_CPU_PRI_BASE  0x118u
#define DIAG_INT_CPU_THRESH    0x194u

static inline uint32_t ble_diag_reg(uint32_t off)
{
    return *(volatile uint32_t *) (uintptr_t) (DIAG_INT_MATRIX_BASE + off);
}

void ble_diag_dump_intr_matrix(const char *tag)
{
    printk("DIAG intr-matrix [%s]: enable=0x%08x type=0x%08x thresh=%u\n",
           tag, (unsigned) ble_diag_reg(DIAG_INT_CPU_ENABLE),
           (unsigned) ble_diag_reg(DIAG_INT_CPU_TYPE),
           (unsigned) ble_diag_reg(DIAG_INT_CPU_THRESH));

    /* Sources 0-16 cover the whole BT/BLE block (4-10: BT_MAC, BT_BB,
     * BT_BB_NMI, RWBT, RWBLE, RWBT_NMI, RWBLE_NMI) plus enough neighbours to
     * see what else shares a line. Only non-zero entries are printed - an
     * unrouted source is the interesting absence, and printing 17 zeros
     * every time would bury it. */
    for (uint32_t src = 0; src <= 16; src++) {
        uint32_t cpu_int = ble_diag_reg(src * 4u) & 0x1fu;
        if (cpu_int != 0) {
            printk("DIAG intr-matrix [%s]: source %u -> cpu_int %u (pri %u)\n",
                   tag, (unsigned) src, (unsigned) cpu_int,
                   (unsigned) ble_diag_reg(DIAG_INT_CPU_PRI_BASE + cpu_int * 4u));
        }
    }
}

/*
 * The blob reaches all of its own state through a handful of pointer
 * globals sitting at the top of DRAM (0x3fcdf834+), above RTEMS's RamEnd
 * (0x3fcd0000) - so they are outside every region RTEMS knows about, and
 * this port only PROVIDEs their addresses in
 * rom-linker-patch/btdm-rom-symbols.ld. Nothing here ever writes them; the
 * blob/ROM is supposed to. `rwip_param` in particular is a 3-entry
 * get/set/del function-pointer struct that r_lld_init calls twice
 * (0x42011f00 and 0x42011f72, via `lw a5,0(rwip_param); jalr a5`), and
 * ESP32-C3 SRAM powers up with random content - so if nothing initialises
 * it on this port, those are calls through whatever the RAM happened to
 * hold. Print them so "initialised or not" stops being a guess.
 */
void ble_diag_dump_blob_globals(const char *tag)
{
    static const struct { const char *name; uint32_t addr; unsigned words; }
    tbl[] = {
        { "rwip_param",       0x3fcdfc04u, 3u },
        { "r_plf_funcs_p",    0x3fcdff80u, 1u },
        { "r_osi_funcs_p",    0x3fcdff84u, 1u },
        { "r_modules_funcs_p",0x3fcdff88u, 1u },
        { "r_ip_funcs_p",     0x3fcdff8cu, 1u },
        { "p_lld_env",        0x3fcdff9cu, 1u },
        { "rwip_rf",          0x3fcdfbc8u, 4u },
        /* Programming lead time the scheduler must respect when pushing a
         * radio event; r_sch_prog_ble_push_hack's BLE_ERR_<target>_<..>_<now>
         * deadline miss is exactly what too small a value here produces. */
        { "rwip_prog_delay",  0x3fcdfc00u, 1u },
        { "sch_prog_env",     0x3fcdfaa0u, 4u },
        { "sch_slice_params", 0x3fcdfa38u, 2u },
    };

    for (unsigned i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
        const volatile uint32_t *p = (const volatile uint32_t *) (uintptr_t) tbl[i].addr;

        printk("DIAG blob-global [%s]: %-18s @%08x =", tag, tbl[i].name,
               (unsigned) tbl[i].addr);
        for (unsigned w = 0; w < tbl[i].words; w++) {
            printk(" %08x", (unsigned) p[w]);
        }
        printk("\n");
    }
}

/*
 * The blob's four dispatch tables are heap blocks it mallocs through this
 * port's own wrapper (sizes visible in the boot log's malloc_internal
 * lines), with the pointer globals above holding their bases. r_lld_init's
 * crash-adjacent call at 0x42012050 goes through r_ip_funcs_p+568, so dump
 * every table whole and resolve the entries offline with addr2line - an
 * entry that does not land on a real symbol start is a table this port
 * failed to populate correctly. Word-per-line would be thousands of lines;
 * 8 words per line keeps it to ~130 lines at 115200 baud.
 */
void ble_diag_dump_words(const char *name, uint32_t base, unsigned bytes)
{
    const volatile uint32_t *p = (const volatile uint32_t *) (uintptr_t) base;
    unsigned words = bytes / 4u;

    printk("DIAG table %s @%08x (%u bytes):\n", name, (unsigned) base, bytes);
    for (unsigned i = 0; i < words; i += 8u) {
        printk("  %04u:", i);
        for (unsigned j = 0; j < 8u && i + j < words; j++) {
            printk(" %08x", (unsigned) p[i + j]);
        }
        printk("\n");
    }
}

/* Counters kept by the BT ISR trampoline in
 * ../../upstream-bt-driver/freertos-compat/src/esp_intr_alloc.c. */
extern volatile uint32_t diag_bt_isr_count;
extern volatile uint32_t diag_bt_isr_sp_in;
extern volatile uint32_t diag_bt_isr_sp_out;
extern volatile uint32_t diag_bt_isr_sp_mismatch;

static uint32_t ble_diag_read_mtval(void)
{
    uint32_t v;
    /* -march=...\_zicsr (see the Makefile) makes csrr available. mtval is
     * only written by the trap itself and nothing between the trap and this
     * extension takes another one, so it still holds this trap's value. */
    __asm__ volatile ("csrr %0, mtval" : "=r" (v));
    return v;
}

void ble_diag_fatal_extension(
    rtems_fatal_source source,
    bool always_set_to_false,
    rtems_fatal_code code
)
{
    (void) always_set_to_false;

    uintptr_t isr_lo = (uintptr_t) _ISR_Stack_area_begin;
    uintptr_t isr_hi = isr_lo + rtems_configuration_get_interrupt_stack_size();

    printk("\n=== DIAG fatal ===\n");
    printk("source  %d\n", (int) source);
    printk("mtval   0x%08x\n", (unsigned) ble_diag_read_mtval());
    printk("in_isr  %d\n", (int) rtems_interrupt_is_in_progress());
    printk("bt-isr  count=%u sp_in=%08x sp_out=%08x first_mismatch_sp=%08x\n",
           (unsigned) diag_bt_isr_count, (unsigned) diag_bt_isr_sp_in,
           (unsigned) diag_bt_isr_sp_out, (unsigned) diag_bt_isr_sp_mismatch);
    ble_diag_dump_blob_globals("fatal");
    printk("isrstk  0x%08x..0x%08x\n", (unsigned) isr_lo, (unsigned) isr_hi);

    if (source == RTEMS_FATAL_SOURCE_EXCEPTION) {
        const CPU_Exception_frame *f = (const CPU_Exception_frame *) code;
        uintptr_t sp = (uintptr_t) f->sp;

        printk("sp      0x%08x (%s)\n", (unsigned) sp,
               (sp >= isr_lo && sp < isr_hi) ? "ON interrupt stack"
                                             : "not on interrupt stack");

        /* ESP32-C3 internal SRAM is 0x3fc80000-0x3fce0000. Bounds-check
         * before dereferencing: a smashed frame can leave `sp` pointing at
         * unmapped space, and faulting again inside the fatal handler would
         * lose the dump entirely.
         *
         * Dump BELOW sp as well as above (2026-09-14): a RISC-V epilogue is
         * `lw ra,N(sp); addi sp,sp,M; ret`, so by the time the bad `ret`
         * traps, sp has already been popped past the frame - the saved-`ra`
         * slot that actually held the garbage is at a NEGATIVE offset from
         * the trap-time sp. An upward-only dump cannot show the corruption,
         * which is why the first capture didn't. */
        if (sp >= 0x3fc80000u + 1024u && sp < 0x3fce0000u - 512u
            && (sp & 3u) == 0u) {
            const uint32_t *w = (const uint32_t *) (sp - 512u);

            printk("stack %08x-%08x (sp at offset 0):\n",
                   (unsigned) (sp - 512u), (unsigned) (sp + 256u));
            for (unsigned i = 0; i < 192u; i += 4u) {
                printk("  %+05d  %08x %08x %08x %08x\n", (int) (i * 4u) - 512,
                       (unsigned) w[i], (unsigned) w[i + 1],
                       (unsigned) w[i + 2], (unsigned) w[i + 3]);
            }
        } else {
            printk("stack: not dumped (sp outside SRAM or misaligned)\n");
        }
    }

    /* Per-task stack bounds and high-water usage, straight from RTEMS's own
     * stack checker - the direct way to see whether the trap-time sp is even
     * inside the btCo task's stack. */
    if (source != RTEMS_FATAL_SOURCE_EXIT) {
        rtems_stack_checker_report_usage();
    }

    printk("=== end DIAG fatal ===\n");
}


uint32_t ble_diag_bt_isr_count(void)
{
    return diag_bt_isr_count;
}

extern volatile uint32_t diag_bt_isr_max_cycles;

uint32_t ble_diag_bt_isr_max_cycles(void)
{
    return diag_bt_isr_max_cycles;
}

extern void diag_cycle_counter_enable(void);

void ble_diag_cycle_counter_enable(void)
{
    diag_cycle_counter_enable();
}

/* ROM console redirect. ets_install_putc1 is PROVIDEd at its real ROM
 * address by ../../upstream-bt-driver/rom-linker-patch/btdm-rom-symbols.ld. */
extern void ets_install_putc1(void (*p)(char c));

static volatile uint32_t diag_rom_line_count;
static volatile uint32_t diag_rom_lines_in_isr;

/* The blob's BLE_ERR deadline-miss message comes through here, so this sink
 * is also a free probe of WHICH CONTEXT the failing scheduler-programming
 * path runs in - ISR or task. That decides where to look next. */
static void diag_rom_putc_sink(char c)
{
    if (c == '\n') {
        diag_rom_line_count++;
        if (rtems_interrupt_is_in_progress()) {
            diag_rom_lines_in_isr++;
        }
    }
}

uint32_t ble_diag_rom_lines_in_isr(void)
{
    return diag_rom_lines_in_isr;
}

extern volatile uint32_t diag_bt_isr_total_cycles;
extern volatile uint32_t diag_bt_isr_slow_count;

uint32_t ble_diag_bt_isr_mean_us(void)
{
    uint32_t n = diag_bt_isr_count;
    return n ? (diag_bt_isr_total_cycles / n) / 160u : 0u;
}

uint32_t ble_diag_bt_isr_slow_count(void)
{
    return diag_bt_isr_slow_count;
}

extern volatile uint32_t diag_crit_max_task_cycles;
extern volatile uint32_t diag_crit_max_isr_cycles;
extern volatile uint32_t diag_crit_max_task_caller;
extern volatile uint32_t diag_crit_max_isr_caller;
extern volatile uint32_t diag_crit_count;

uint32_t ble_diag_crit_max_task_us(void)
{
    return diag_crit_max_task_cycles / 160u;   /* 160 MHz */
}

uint32_t ble_diag_crit_max_isr_us(void)
{
    return diag_crit_max_isr_cycles / 160u;
}

uint32_t ble_diag_crit_max_task_caller(void)
{
    return diag_crit_max_task_caller;
}

uint32_t ble_diag_crit_max_isr_caller(void)
{
    return diag_crit_max_isr_caller;
}

uint32_t ble_diag_crit_count(void)
{
    return diag_crit_count;
}

extern volatile uint32_t diag_bt_isr_sp_mismatch;

void ble_diag_reset_counters(void)
{
    diag_crit_max_task_cycles = 0;
    diag_crit_max_isr_cycles = 0;
    diag_crit_max_task_caller = 0;
    diag_crit_max_isr_caller = 0;
    diag_crit_count = 0;

    diag_bt_isr_max_cycles = 0;
    diag_bt_isr_total_cycles = 0;
    diag_bt_isr_slow_count = 0;
    diag_bt_isr_count = 0;
    diag_bt_isr_sp_mismatch = 0;
}

void ble_diag_rom_console_mute(void)
{
    ets_install_putc1(diag_rom_putc_sink);
}

uint32_t ble_diag_rom_lines(void)
{
    return diag_rom_line_count;
}

#endif /* BLE_DIAG */
