/*
 * Stub definitions for three symbols that remain genuinely unresolved
 * after exhaustive real-source investigation (2026-08-26):
 *
 *   coex_pti_v2        - called unconditionally from bt.c's
 *                         esp_bt_controller_enable() (not gated behind
 *                         CONFIG_SW_COEXIST_ENABLE, unlike every other
 *                         coex_*() call in that file)
 *   bt_bb_v2_init_cmplx - called from the closed libbtdm_app.a blob's
 *                         btdm_controller_task (arch_main.o)
 *   bt_bb_tx_cca_set    - called from the closed blob's
 *                         r_rf_rw_v9_le_init and esp_ble_internal_test_reset
 *
 * What was checked before concluding these need stubs (see
 * ../../vendor/README.md's "Blob version pinning" section for the full
 * writeup): the vendored bt.c is byte-identical to real ESP-IDF v5.3.1
 * (diffed directly). All three names were searched for - as defined
 * *and* undefined symbols, case-insensitively - in:
 *   - libbtdm_app.a and esp-phy-lib's libphy.a, both re-fetched pinned
 *     to the *exact* submodule commits real IDF v5.3.1 uses
 *     (bfdfe8f851c9.../06e7625de197... - see ../../examples/
 *     ble_vhci_smoke/Makefile), not just "whatever HEAD is" (pinning
 *     this correctly did resolve 11 of the original 14 "missing"
 *     symbols, which really were just version skew - these 3 are what's
 *     left after that real fix).
 *   - esp-coex-lib's libcoexist.a, pinned the same way.
 *   - every real esp32c3.rom*.ld file (base + eco3 + eco7 overlays) at
 *     the matching esp_rom component revision.
 * None of the three appear anywhere in that set. Cross-checked against
 * a real, independent report of the same class of problem -
 * espressif/esp-idf issue #13113, "libcoexist functions missing" (a
 * different symbol, `btdm_rf_bb_reg_init`, on ESP32 classic, but the
 * same "official IDF build hits an undefined blob symbol" shape) -
 * which Espressif closed as internally resolved with no public
 * explanation, i.e. this class of blob/source drift is real and has
 * bitten official IDF users too, not just this port.
 *
 * Rationale for stubbing rather than blocking on it further:
 *   - coex_pti_v2(): per bt.c's own call-site comment ("Notice the init
 *     order: esp_phy_enable() -> bt_bb_v2_init_cmplx() -> coex_pti_v2()"),
 *     this resets/refreshes the RF-arbitration priority table shared
 *     with Wi-Fi. With no Wi-Fi component in this BT-only smoke test,
 *     there is nothing to arbitrate against - a no-op is very likely
 *     equivalent to whatever the real function would do here.
 *   - bt_bb_v2_init_cmplx()/bt_bb_tx_cca_set(): baseband-hardware setup
 *     calls inside the closed controller task and RF init path. A
 *     no-op stub here is a genuine, cited unknown, not a validated
 *     equivalence - it may leave BLE RF performance degraded or
 *     incorrect. This is exactly the kind of thing this repo's
 *     convention is to mark UNTESTED and let real hardware decide
 *     rather than reason about further from source alone - see
 *     ../../examples/ble_vhci_smoke/init.c's own status header.
 *
 * If this smoke test fails on real hardware in a way traceable to RF
 * baseband init, these three are the first place to look.
 */

void coex_pti_v2(void)
{
}

void bt_bb_v2_init_cmplx(void)
{
}

void bt_bb_tx_cca_set(void)
{
}

/*
 * Added 2026-08-26 after re-vendoring bt.c from real ESP-IDF commit
 * 8da824cd0ede2d6c7317c5a65504bce78762b67b (paired with the newer
 * esp32c3-bt-lib commit 0a08c4b32f3666003080b662a1a61794da24ff0f - see
 * sdkconfig-compat.h's "Re-vendoring bt.c" note and
 * rom-linker-patch/btdm-rom-symbols.ld's regeneration note). Two more
 * genuinely-unresolvable symbols surfaced with the newer, correctly-
 * paired blob: `bt_bb_set_rx_sense`/`bt_bb_set_max_gain`, both called
 * from the blob's `esp_ble_internal_test_reset` (production DTM test-
 * mode reset path) - same investigation method as the three above
 * (checked as defined/undefined, case-insensitively, in libbtdm_app.a/
 * libphy.a/libcoexist.a all at their correct paired commits, and in
 * every real ROM ld variant - absent from all of them). Real production
 * IDF/Zephyr firmware built against this exact blob commit must resolve
 * these somehow (they're not optional in the real build), but no public
 * Espressif artifact provides a definition - flagged as a genuine
 * unknown, not a validated equivalence, matching the other three.
 */
void bt_bb_set_rx_sense(void)
{
}

void bt_bb_set_max_gain(void)
{
}

/*
 * Also added 2026-08-26, same re-vendoring: 9 more symbols the blob's
 * function-pointer tables (r_ip_funcs_ro/r_modules_funcs_ro/
 * r_plf_funcs_ro) reference, all real ROM addresses per
 * esp32c3.rom.ld but COMMENTED OUT there (i.e. real IDF's own ROM
 * linker script deliberately does not resolve them to ROM, the same
 * "real IDF must resolve this some other way we can't see" situation as
 * above, not a gap unique to this port). One of these,
 * `r_rwbtdm_isr_wrapper`, is referenced from a function-pointer table
 * entry that (per its name) may be installed as a real interrupt
 * handler - a no-op stub is a real, flagged risk for that one
 * specifically if this port's BLE use ever reaches the code path that
 * installs it (this smoke test's basic HCI Reset round-trip over VHCI
 * does not exercise connection/scan/DTM-test-mode logic, so it should
 * not reach any of these 9 in practice - re-check first if a future,
 * fuller BLE example hangs or misbehaves).
 */
void r_lld_test_stop(void)
{
}

void r_lld_res_list_priv_mode_update(void)
{
}

void r_lld_con_rx_channel_assess(void)
{
}

void r_llc_le_ping_restart(void)
{
}

void r_lld_adv_start_update_filter_policy(void)
{
}

void r_lld_scan_try_sched(void)
{
}

void r_rwip_assert(void)
{
}

void r_rwip_wakeup_end(void)
{
}

void r_rwbtdm_isr_wrapper(void)
{
}
