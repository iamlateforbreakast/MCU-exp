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
