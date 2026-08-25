/*
 * NOT a standalone linker script - a fragment to append into the RTEMS
 * esp32c3db BSP's own default linker script (this repo doesn't vendor
 * RTEMS - see ../../ESP32-C3-RTEMS.md), the same "fragment to merge, not
 * full copy" convention as ../bsp-patch/. See ../vendor/README.md's
 * "Linking" section for the recon this came from.
 *
 * These 172 lines are real, fixed ESP32-C3 boot-ROM addresses -
 * PROVIDE()'d symbol names the closed libbtdm_app.a blob (and one -
 * ets_rom_layout_p - that bt.c itself needs) call/read directly, never
 * implemented in software. Copied verbatim (name + address only, not
 * copyrightable data in isolation, but sourced from and cross-checked
 * against Apache-2.0-licensed files either way) from real ESP-IDF v5.3.1
 * `components/esp_rom/esp32c3/ld/esp32c3.rom*.ld` (the base file plus the
 * per-revision `eco3`/`eco7` overlays and `esp32c3.rom.api.ld`), confirmed
 * 2026-08-25 with real internet access by:
 *   1. Fetching the real closed blob from
 *      github.com/espressif/esp32c3-bt-lib at commit `0a08c4b`
 *      (2026-05-25) and running real `riscv-rtems7-nm` against it (cross-
 *      toolchain but same RISC-V ELF target - worked with no issues).
 *   2. Computing its 185 real external symbol needs (undefined across all
 *      102 object files in the archive, minus whatever the archive
 *      resolves internally) plus `ets_rom_layout_p`/`ets_delay_us`
 *      (needed by vendored `bt.c` directly, not the blob).
 *   3. Matching each of those 186 names against every line in every real
 *      `esp32c3.rom*.ld` file (not just the base one - several, e.g.
 *      `bt_track_pll_cap`'s eco3/eco7 aliases, only exist in the
 *      per-revision overlays) - 172 matched, listed below.
 *
 * PROVIDE() (not plain assignment) throughout, even where the real IDF
 * source used a bare `sym = addr;` for some of these - PROVIDE only takes
 * effect if the symbol isn't already defined some other way, the safer
 * default when merging into an unfamiliar BSP linker script.
 *
 * 14 names from the blob's own real needs could NOT be matched anywhere
 * in any esp32c3.rom*.ld file, including all per-revision overlays -
 * genuinely open, not attempted here (see ../vendor/README.md's "Linking"
 * section for the exact list and the "commented out in the real ROM
 * script" detail found for several of them - real stock IDF evidently
 * resolves these some other way not yet identified).
 *
 * Not yet done - this fragment's own integration steps (same pattern as
 * ../bsp-patch/README.md): where exactly in the BSP's linker script this
 * needs to go, and whether `esp32c3db`'s default script's existing
 * `PROVIDE`/memory-region setup conflicts with any of these addresses,
 * hasn't been checked against a real link attempt yet - flagged, not
 * assumed safe.
 */
PROVIDE ( aa_gen = 0x3fcdff78 );
PROVIDE ( adv_evt_prop2type = 0x3ff1f768 );
PROVIDE ( adv_evt_type2prop = 0x3ff1f760 );
PROVIDE ( aes_cmac_zero = 0x3ff1f750 );
PROVIDE ( aes_env = 0x3fcdff6c );
PROVIDE ( aes_k2_salt = 0x3ff1f740 );
PROVIDE ( aes_k3_salt = 0x3ff1f728 );
PROVIDE ( aes_k4_salt = 0x3ff1f714 );
PROVIDE ( BasePoint_x_256 = 0x3ff1ffdc );
PROVIDE ( BasePoint_y_256 = 0x3ff1ffbc );
PROVIDE ( bigHexP256 = 0x3ff1f6e8 );
PROVIDE ( btdm_env_p = 0x3fcdffc4 );
PROVIDE ( btdm_pwr_state = 0x3fcdff18 );
PROVIDE ( btdm_slp_err = 0x3fcdff14 );
PROVIDE ( bt_rf_coex_cfg_cb = 0x3fcdff1c );
PROVIDE ( bt_rf_coex_cfg_p = 0x3fcdffcc );
PROVIDE ( bt_rf_coex_get_dft_cfg = 0x400008dc );
PROVIDE ( bt_rf_coex_hooks_p = 0x3fcdffc8 );
PROVIDE ( byte_tx_time = 0x3ff1f6e0 );
PROVIDE ( __clzsi2 = 0x4000079c );
PROVIDE ( co_default_bdaddr = 0x3fcdf9fc );
PROVIDE ( coef_B = 0x3ff1f670 );
PROVIDE ( connect_req_dur_tab = 0x3ff1f668 );
PROVIDE ( co_null_bdaddr = 0x3ff1f6d8 );
PROVIDE ( co_phy_mask_to_value = 0x3ff1f6c8 );
PROVIDE ( co_phy_to_rate = 0x3ff1f6c4 );
PROVIDE ( co_phy_value_to_mask = 0x3ff1f6c0 );
PROVIDE ( co_rate_to_phy = 0x3ff1f6b0 );
PROVIDE ( co_sca2ppm = 0x3ff1f69c );
PROVIDE ( DebugE256PublicKey_x = 0x3ff1ff9c );
PROVIDE ( DebugE256PublicKey_y = 0x3ff1ff7c );
PROVIDE ( DebugE256SecretKey = 0x3ff1ff5c );
PROVIDE ( ECC_4Win_Look_up_table = 0x3ff1f7a0 );
PROVIDE ( ecc_env = 0x3fcdff0c );
PROVIDE ( ecc_Jacobian_InfinityPoint256 = 0x3ff1f5e4 );
PROVIDE ( em_base_reg_lut = 0x3ff1f518 );
PROVIDE ( esp_handler = 0x3fcdff04 );
PROVIDE ( esp_vendor_cmd = 0x3fcdfefc );
PROVIDE ( ets_printf = 0x40000040 );
PROVIDE ( ets_rom_layout_p = 0x3ff1fffc );
PROVIDE ( fixed_tx_time = 0x3ff1f510 );
PROVIDE ( g_adv_delay_dis = 0x3fcdfef8 );
PROVIDE ( g_bt_plf_log_level = 0x3fcdf9f4 );
PROVIDE ( g_conflict_elt = 0x3fcdfef4 );
PROVIDE ( g_eif_api = 0x3fcdfee4 );
PROVIDE ( g_event_empty = 0x3fcdfed8 );
PROVIDE ( g_max_evt_env = 0x3fcdfec4 );
PROVIDE ( g_misc_state = 0x3fcdfec0 );
PROVIDE ( g_rma_rule_db = 0x3fcdfea4 );
PROVIDE ( g_rtp_rule_db = 0x3fcdfe88 );
PROVIDE ( g_rw_controller_task_handle = 0x3fcdffc0 );
PROVIDE ( g_rw_init_sem = 0x3fcdffbc );
PROVIDE ( g_rw_schd_queue = 0x3fcdffb8 );
PROVIDE ( g_scan_forever = 0x3fcdfe85 );
PROVIDE ( g_time_msb = 0x3fcdfe84 );
PROVIDE ( h4tl_env = 0x3fcdfe5c );
PROVIDE ( hci_cmd_desc_root_tab = 0x3ff1f4d8 );
PROVIDE ( hci_env = 0x3fcdfe38 );
PROVIDE ( hci_ext_host = 0x3fcdfe34 );
PROVIDE ( hci_fc_env = 0x3fcdfe2c );
PROVIDE ( hci_rsvd_evt_msk = 0x3ff1ef48 );
PROVIDE ( hci_tl_env = 0x3fcdfe00 );
PROVIDE ( ke_env = 0x3fcdfdd0 );
PROVIDE ( ke_event_env = 0x3fcdfd90 );
PROVIDE ( ke_task_env = 0x3fcdfd14 );
PROVIDE ( llc_env = 0x3fcdfcec );
PROVIDE ( lld_adv_env = 0x3fcdfcc4 );
PROVIDE ( lld_aux_phy_to_rate = 0x3ff1ef44 );
PROVIDE ( lld_con_env = 0x3fcdfc9c );
PROVIDE ( lld_exp_sync_pos_tab = 0x3fcdfc94 );
PROVIDE ( lld_init_env = 0x3fcdffb4 );
PROVIDE ( lld_init_max_aux_dur_tab = 0x3ff1ef3c );
PROVIDE ( lld_per_adv_env = 0x3fcdfc6c );
PROVIDE ( lld_rpa_renew_env = 0x3fcdffb0 );
PROVIDE ( lld_scan_env = 0x3fcdffac );
PROVIDE ( lld_scan_map_legacy_pdu_to_evt_type = 0x3ff1ef34 );
PROVIDE ( lld_scan_max_aux_dur_tab = 0x3ff1ef2c );
PROVIDE ( lld_scan_sync_env = 0x3fcdffa8 );
PROVIDE ( lld_sync_env = 0x3fcdfc44 );
PROVIDE ( lld_sync_max_aux_dur_tab = 0x3ff1ef24 );
PROVIDE ( lld_test_env = 0x3fcdffa4 );
PROVIDE ( LLM_AA_CT1 = 0x3ff1f79c );
PROVIDE ( LLM_AA_CT2 = 0x3ff1f798 );
PROVIDE ( llm_le_adv_flow_env = 0x3fcdfc38 );
PROVIDE ( llm_local_le_feats = 0x3ff1ef1c );
PROVIDE ( llm_local_supp_cmds = 0x3ff1eeec );
PROVIDE ( max_data_tx_time = 0x3ff1eec4 );
PROVIDE ( maxSecretKey_256 = 0x3ff1eecc );
PROVIDE ( memcmp = 0x40000360 );
PROVIDE ( memcpy = 0x40000358 );
PROVIDE ( memmove = 0x4000035c );
PROVIDE ( memset = 0x40000354 );
PROVIDE ( one_bits = 0x3ff1eeb4 );
PROVIDE ( p_ble_util_buf_env = 0x3fcdffa0 );
PROVIDE ( p_lld_env = 0x3fcdff9c );
PROVIDE ( p_llm_env = 0x3fcdff98 );
PROVIDE ( __popcountsi2 = 0x40000888 );
PROVIDE ( privacy_en = 0x3fcdf9b4 );
PROVIDE ( ets_delay_us = 0x40000050 );
/*
 * esp_rom_delay_us is the symbol bt.c actually calls (via esp_rom_sys.h);
 * real ESP-IDF aliases it to the ROM's own ets_delay_us via
 * esp32c3.rom.api.ld's `PROVIDE ( esp_rom_delay_us = ets_delay_us );` -
 * added here as its own PROVIDE (GNU ld doesn't chase symbol-name aliases
 * through a plain `= othersym` the way `PROVIDE(x = y)` might imply if y
 * is itself just a name - it does work here since PROVIDE's RHS can be
 * any symbol, resolved same as a normal assignment, confirmed by this
 * fragment's own test link 2026-08-25 no longer leaving esp_rom_delay_us
 * undefined).
 */
PROVIDE ( esp_rom_delay_us = ets_delay_us );
/*
 * The five below were found needed only after vendoring esp_clk.c/
 * hw_random.c/mac_addr.c/rtc_clk.c/periph_ctrl.c (2026-08-25, second pass
 * - not part of libbtdm_app.a's own needs, part of what real esp32c3
 * open-source ESP-IDF C files need from the ROM). Same citation method:
 * matched against real esp32c3.rom*.ld, each a real PROVIDE()-based ROM
 * alias in the source, re-added here as two-hop aliases resolved to their
 * final real addresses (both hops confirmed present in the real files,
 * not assumed).
 */
PROVIDE ( ets_get_cpu_frequency = 0x40000584 );
PROVIDE ( esp_rom_get_cpu_ticks_per_us = ets_get_cpu_frequency );
PROVIDE ( ets_update_cpu_frequency = 0x40000588 );
PROVIDE ( esp_rom_set_cpu_ticks_per_us = ets_update_cpu_frequency );
PROVIDE ( rom_i2c_writeReg = 0x4000195c );
PROVIDE ( esp_rom_regi2c_write = rom_i2c_writeReg );
PROVIDE ( rom_i2c_writeReg_Mask = 0x40001960 );
PROVIDE ( esp_rom_regi2c_write_mask = rom_i2c_writeReg_Mask );
PROVIDE ( ets_printf = 0x40000040 );
PROVIDE ( esp_rom_printf = ets_printf );

/*
 * Different category from everything above: not ROM function addresses,
 * but the memory-mapped peripheral-register-struct globals real IDF's
 * `soc/*_struct.h` headers declare `extern` (e.g. `extern system_dev_t
 * SYSTEM;` in `../vendor/components/soc/esp32c3/include/soc/
 * system_struct.h:1397`) for direct `SYSTEM.some_field = ...`-style
 * register access - `rtc_time.c` (vendored 2026-08-25) is the first
 * vendored file in this repo to use this style. Addresses computed from
 * this repo's own already-vendored, cited headers (`reg_base.h`'s
 * `DR_REG_SYSTEM_BASE`/`DR_REG_TIMERGROUP0_BASE`, `soc.h`'s
 * `REG_TIMG_BASE(i)` macro), not looked up separately - self-consistent
 * with what's already in this repo, not a new external source.
 */
PROVIDE ( SYSTEM = 0x600c0000 );
PROVIDE ( TIMERG0 = 0x6001F000 );
PROVIDE ( TIMERG1 = 0x60020000 );

/*
 * Found needed after vendoring esp_phy/src/phy_common.c (2026-08-25,
 * third pass) - real two-hop ROM alias, same citation method.
 */
PROVIDE ( gpio_matrix_out = 0x400005a4 );
PROVIDE ( esp_rom_gpio_connect_out_signal = gpio_matrix_out );
PROVIDE ( phy_get_romfuncs = 0x400018fc );

PROVIDE ( r_bt_bb_isr = 0x40000b9c );
PROVIDE ( r_btdm_task_post = 0x40000c14 );
PROVIDE ( r_btdm_task_post_from_isr = 0x40000c18 );
PROVIDE ( r_btdm_task_recycle = 0x40000c1c );
PROVIDE ( r_bt_rf_coex_conn_phy_coded_data_time_limit_en_get = 0x40000ba8 );
PROVIDE ( r_bt_rtp_get_txpwr_idx_by_act = 0x40000c00 );
PROVIDE ( r_h4tl_eif_p = 0x3fcdff94 );
PROVIDE ( r_hci_register_vendor_desc_tab = 0x40000d9c );
PROVIDE ( r_hli_funcs_p = 0x3fcdff90 );
PROVIDE ( r_ip_funcs_p = 0x3fcdff8c );
PROVIDE ( r_ke_task_schedule = 0x40000e80 );
PROVIDE ( r_llc_con_upd_param_in_range = 0x40000ecc );
PROVIDE ( r_llc_hci_command_handler = 0x40000ef0 );
PROVIDE ( r_llc_llcp_tx_check = 0x40000f58 );
PROVIDE ( r_llc_loc_ch_map_proc_continue = 0x40000f5c );
PROVIDE ( r_llc_loc_con_upd_proc_continue = 0x40000f60 );
PROVIDE ( r_llc_loc_phy_upd_proc_continue = 0x40000f78 );
PROVIDE ( r_llc_rem_con_upd_proc_continue = 0x40000fb4 );
PROVIDE ( r_lld_adv_adv_data_update = 0x40000ff4 );
PROVIDE ( r_lld_adv_ext_chain_none_construct = 0x40001b50 );
PROVIDE ( r_lld_adv_ext_chain_scannable_construct = 0x40001b58 );
PROVIDE ( r_lld_adv_ext_pkt_prepare_set = 0x40001b4c );
PROVIDE ( r_lld_adv_pkt_rx_send_scan_req_evt = 0x40001040 );
PROVIDE ( r_lld_adv_scan_rsp_data_update = 0x40001050 );
PROVIDE ( r_lld_con_evt_canceled_cbk = 0x400010b0 );
PROVIDE ( r_lld_conn_dynamic_pti_process = 0x40001144 );
PROVIDE ( r_lld_con_rx = 0x400010fc );
PROVIDE ( r_lld_con_sched = 0x40001118 );
PROVIDE ( r_lld_con_stop = 0x40001124 );
PROVIDE ( r_lld_con_tx_prog_new_packet = 0x40001b74 );
PROVIDE ( r_lld_llcp_rx_ind_handler = 0x400011b0 );
PROVIDE ( r_lld_per_adv_sched = 0x400011f8 );
PROVIDE ( r_lld_res_list_clear = 0x40004638 );
PROVIDE ( r_lld_res_list_peer_update = 0x4000122c );
PROVIDE ( r_lld_res_list_rem = 0x40004680 );
PROVIDE ( r_lld_scan_process_pkt_rx = 0x40001280 );
PROVIDE ( r_lld_scan_process_pkt_rx_adv_rep = 0x40001284 );
PROVIDE ( r_llm_le_features_get = 0x400013b0 );
PROVIDE ( r_modules_funcs_p = 0x3fcdff88 );
PROVIDE ( r_osi_funcs_p = 0x3fcdff84 );
PROVIDE ( r_plf_funcs_p = 0x3fcdff80 );
PROVIDE ( r_register_esp_vendor_cmd_handler = 0x40001400 );
PROVIDE ( r_rf_txpwr_cs_get = 0x40001428 );
PROVIDE ( r_rf_txpwr_dbm_get = 0x4000142c );
PROVIDE ( r_rwble_isr = 0x40001464 );
PROVIDE ( r_rwip_prevent_sleep_clear = 0x4000149c );
PROVIDE ( r_rwip_prevent_sleep_set = 0x400014a0 );
PROVIDE ( r_sch_arb_event_start_isr = 0x400014f8 );
PROVIDE ( r_sch_plan_set = 0x40001534 );
PROVIDE ( r_sch_prog_ble_push = 0x40001b8c );
PROVIDE ( r_sch_prog_end_isr = 0x40001538 );
PROVIDE ( rwble_env = 0x3fcdfc2c );
PROVIDE ( rwip_coex_cfg = 0x3ff1eeac );
PROVIDE ( rwip_env = 0x3fcdfc10 );
PROVIDE ( rwip_param = 0x3fcdfc04 );
PROVIDE ( rwip_priority = 0x3ff1ee94 );
PROVIDE ( rwip_prog_delay = 0x3fcdfc00 );
PROVIDE ( rwip_rf = 0x3fcdfbc8 );
PROVIDE ( rw_sleep_enable = 0x3fcdfc34 );
PROVIDE ( sch_alarm_env = 0x3fcdfbc0 );
PROVIDE ( sch_arb_env = 0x3fcdfbac );
PROVIDE ( sch_plan_env = 0x3fcdfba4 );
PROVIDE ( sch_prog_env = 0x3fcdfaa0 );
PROVIDE ( sch_slice_env = 0x3fcdfa40 );
PROVIDE ( sch_slice_params = 0x3fcdfa38 );
PROVIDE ( sdk_cfg_priv_opts = 0x3fcdf96c );
PROVIDE ( strlen = 0x40000374 );
PROVIDE ( TASK_DESC_LLC = 0x3fcdfa1c );
PROVIDE ( TASK_DESC_LLM = 0x3fcdfa10 );
PROVIDE ( TASK_DESC_VSHCI = 0x3fcdfa04 );
PROVIDE ( unloaded_area = 0x3fcdfa2c );
PROVIDE ( veryBigHexP256 = 0x3ff1ee48 );
PROVIDE ( vhci_env_p = 0x3fcdff7c );
