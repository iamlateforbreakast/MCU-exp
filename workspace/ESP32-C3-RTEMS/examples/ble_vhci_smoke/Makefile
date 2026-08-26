# Builds ble_vhci_smoke.exe: the Phase 3 controller-only BLE smoke test
# from ../../upstream-bt-driver/README.md, linking vendored bt.c +
# esp_phy + efuse (../../upstream-bt-driver/vendor/) against the two
# closed Espressif blobs (fetched, not vendored - see "Fetching the
# blobs" below) and this BSP's real esp32c3db install, with the
# linker-section-patch applied.
#
# Same riscv-rtems7-gcc + pkg-config pattern as ../hello_world/Makefile.
#
# Object files are named explicitly (not derived from source basenames)
# because three vendored sources exist as generic+chip-specific pairs
# sharing the same filename in different directories
# (esp_efuse_utility.c, esp_efuse_fields.c, efuse_hal.c - see
# ../../upstream-bt-driver/vendor/README.md) - a basename-derived object
# list would silently collide and drop half of each pair.
#
# STATUS: builds (once the blobs are fetched and the linker patch is
# applied - see below); NOT YET RUN on real hardware. Do not trust
# ../../upstream-bt-driver/README.md's "Phase 3" being marked done until
# this has actually flashed and printed a PASS on a real ESP32-C3.

RTEMS_ROOT  ?= $(HOME)/rtems/7
CC          := riscv-rtems7-gcc
BSP_PC_FILE := $(firstword $(shell find $(RTEMS_ROOT) -name '*esp32c3db*.pc' 2>/dev/null))
BSP_PC      := $(basename $(notdir $(BSP_PC_FILE)))

ifeq ($(BSP_PC_FILE),)
$(error No *esp32c3db*.pc file found under $(RTEMS_ROOT) - run \
  "find $(RTEMS_ROOT) -name '*.pc'" to see what pkgconfig files the BSP \
  install actually produced, and adjust this Makefile's BSP_PC_FILE search)
endif

BSP_PC_DIR := $(dir $(BSP_PC_FILE))
PKG_CONFIG := PKG_CONFIG_PATH=$(BSP_PC_DIR) pkg-config

BT_DRIVER := ../../upstream-bt-driver
VENDOR    := $(BT_DRIVER)/vendor/components
FRTC      := $(BT_DRIVER)/freertos-compat

INCLUDES := \
  -I$(BT_DRIVER)/build-include -I$(FRTC)/include \
  -I$(VENDOR)/bt/include/esp32c3/include \
  -I$(VENDOR)/esp_common/include \
  -I$(VENDOR)/esp_rom/include -I$(VENDOR)/esp_rom/include/esp32c3 -I$(VENDOR)/esp_rom/esp32c3 \
  -I$(VENDOR)/riscv/include \
  -I$(VENDOR)/soc/esp32c3/include -I$(VENDOR)/soc/include \
  -I$(VENDOR)/esp_system/include \
  -I$(VENDOR)/esp_hw_support/include -I$(VENDOR)/esp_hw_support/port/esp32c3/include \
  -I$(VENDOR)/esp_hw_support/port/include -I$(VENDOR)/esp_hw_support/include/soc \
  -I$(VENDOR)/esp_phy/include -I$(VENDOR)/esp_phy/esp32c3/include \
  -I$(VENDOR)/heap/include \
  -I$(VENDOR)/hal/esp32c3/include -I$(VENDOR)/hal/platform_port/include -I$(VENDOR)/hal/include \
  -I$(VENDOR)/efuse/include -I$(VENDOR)/efuse/private_include \
  -I$(VENDOR)/efuse/esp32c3/include -I$(VENDOR)/efuse/esp32c3/private_include \
  -I$(VENDOR)/bootloader_support/private_include

# -march=rv32imc_zicsr (not plain rv32imc): hw_random.c needs a real CSR
# instruction - see ../../upstream-bt-driver/vendor/README.md's Build
# recipe section. pkg-config's own --cflags/--variable=ABI_FLAGS both
# emit plain -march=rv32imc, so it's stripped and re-added.
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(BSP_PC) | sed 's/-march=[^ ]*//')
PKG_ABI    := $(shell $(PKG_CONFIG) --variable=ABI_FLAGS $(BSP_PC) | sed 's/-march=[^ ]*//')
CFLAGS  := -Wall -Wextra -O2 -g -fdata-sections -ffunction-sections \
           -march=rv32imc_zicsr $(PKG_CFLAGS) $(INCLUDES)
LDFLAGS := -march=rv32imc_zicsr $(PKG_ABI) $(shell $(PKG_CONFIG) --libs $(BSP_PC))

# object-name : source-path pairs, explicit to avoid the basename
# collisions described above.
OBJS := \
  init.o \
  critical.o esp_intr_alloc.o esp_sleep.o esp_timer.o gpio_stub.o \
  heap_caps.o lock.o nvs_stub.o queue.o semphr.o task.o \
  bt.o \
  esp_clk.o hw_random.o mac_addr.o periph_ctrl.o esp_gpio_reserve.o \
  rtc_clk.o rtc_time.o rtc_clk_init.o bootloader_soc.o bootloader_hw_init.o \
  phy_init.o phy_common.o lib_printf.o phy_callback.o phy_init_data.o \
  esp_efuse_api.o esp_efuse_utility_generic.o esp_efuse_fields_generic.o \
  esp_efuse_table.o esp_efuse_utility_c3.o esp_efuse_fields_c3.o \
  esp_efuse_api_key.o \
  efuse_hal_generic.o efuse_hal_c3.o \
  bb_coex_stubs.o

init.o: init.c
	$(CC) $(CFLAGS) -c $< -o $@
critical.o: $(FRTC)/src/critical.c
	$(CC) $(CFLAGS) -c $< -o $@
esp_intr_alloc.o: $(FRTC)/src/esp_intr_alloc.c
	$(CC) $(CFLAGS) -c $< -o $@
esp_sleep.o: $(FRTC)/src/esp_sleep.c
	$(CC) $(CFLAGS) -c $< -o $@
esp_timer.o: $(FRTC)/src/esp_timer.c
	$(CC) $(CFLAGS) -c $< -o $@
gpio_stub.o: $(FRTC)/src/gpio_stub.c
	$(CC) $(CFLAGS) -c $< -o $@
heap_caps.o: $(FRTC)/src/heap_caps.c
	$(CC) $(CFLAGS) -c $< -o $@
lock.o: $(FRTC)/src/lock.c
	$(CC) $(CFLAGS) -c $< -o $@
nvs_stub.o: $(FRTC)/src/nvs_stub.c
	$(CC) $(CFLAGS) -c $< -o $@
queue.o: $(FRTC)/src/queue.c
	$(CC) $(CFLAGS) -c $< -o $@
semphr.o: $(FRTC)/src/semphr.c
	$(CC) $(CFLAGS) -c $< -o $@
task.o: $(FRTC)/src/task.c
	$(CC) $(CFLAGS) -c $< -o $@
bt.o: $(VENDOR)/bt/controller/esp32c3/bt.c
	$(CC) $(CFLAGS) -include esp_intr_alloc.h -c $< -o $@
esp_clk.o: $(VENDOR)/esp_hw_support/esp_clk.c
	$(CC) $(CFLAGS) -c $< -o $@
hw_random.o: $(VENDOR)/esp_hw_support/hw_random.c
	$(CC) $(CFLAGS) -c $< -o $@
mac_addr.o: $(VENDOR)/esp_hw_support/mac_addr.c
	$(CC) $(CFLAGS) -include assert.h -include inttypes.h -c $< -o $@
periph_ctrl.o: $(VENDOR)/esp_hw_support/periph_ctrl.c
	$(CC) $(CFLAGS) -c $< -o $@
esp_gpio_reserve.o: $(VENDOR)/esp_hw_support/esp_gpio_reserve.c
	$(CC) $(CFLAGS) -DATOMIC_VAR_INIT'(value)=(value)' -c $< -o $@
rtc_clk.o: $(VENDOR)/esp_hw_support/port/esp32c3/rtc_clk.c
	$(CC) $(CFLAGS) -c $< -o $@
rtc_time.o: $(VENDOR)/esp_hw_support/port/esp32c3/rtc_time.c
	$(CC) $(CFLAGS) -c $< -o $@
rtc_clk_init.o: $(VENDOR)/esp_hw_support/port/esp32c3/rtc_clk_init.c
	$(CC) $(CFLAGS) -c $< -o $@
bootloader_soc.o: $(VENDOR)/bootloader_support/src/esp32c3/bootloader_soc.c
	$(CC) $(CFLAGS) -c $< -o $@
bootloader_hw_init.o: $(VENDOR)/bootloader_support/src/esp32c3/bootloader_hw_init.c
	$(CC) $(CFLAGS) -c $< -o $@
phy_init.o: $(VENDOR)/esp_phy/src/phy_init.c
	$(CC) $(CFLAGS) -include esp_attr.h -include esp_heap_caps.h -c $< -o $@
phy_common.o: $(VENDOR)/esp_phy/src/phy_common.c
	$(CC) $(CFLAGS) -include esp_attr.h -include esp_heap_caps.h -c $< -o $@
lib_printf.o: $(VENDOR)/esp_phy/src/lib_printf.c
	$(CC) $(CFLAGS) -include stdarg.h -c $< -o $@
phy_callback.o: $(VENDOR)/esp_phy/src/phy_callback.c
	$(CC) $(CFLAGS) -c $< -o $@
phy_init_data.o: $(VENDOR)/esp_phy/esp32c3/phy_init_data.c
	$(CC) $(CFLAGS) -c $< -o $@
esp_efuse_api.o: $(VENDOR)/efuse/src/esp_efuse_api.c
	$(CC) $(CFLAGS) -include inttypes.h -c $< -o $@
esp_efuse_utility_generic.o: $(VENDOR)/efuse/src/esp_efuse_utility.c
	$(CC) $(CFLAGS) -include inttypes.h -c $< -o $@
esp_efuse_fields_generic.o: $(VENDOR)/efuse/src/esp_efuse_fields.c
	$(CC) $(CFLAGS) -include inttypes.h -c $< -o $@
esp_efuse_table.o: $(VENDOR)/efuse/esp32c3/esp_efuse_table.c
	$(CC) $(CFLAGS) -include inttypes.h -c $< -o $@
esp_efuse_utility_c3.o: $(VENDOR)/efuse/esp32c3/esp_efuse_utility.c
	$(CC) $(CFLAGS) -include inttypes.h -c $< -o $@
esp_efuse_fields_c3.o: $(VENDOR)/efuse/esp32c3/esp_efuse_fields.c
	$(CC) $(CFLAGS) -include inttypes.h -c $< -o $@
esp_efuse_api_key.o: $(VENDOR)/efuse/src/efuse_controller/keys/with_key_purposes/esp_efuse_api_key.c
	$(CC) $(CFLAGS) -include inttypes.h -c $< -o $@
efuse_hal_generic.o: $(VENDOR)/hal/efuse_hal.c
	$(CC) $(CFLAGS) -include inttypes.h -c $< -o $@
efuse_hal_c3.o: $(VENDOR)/hal/esp32c3/efuse_hal.c
	$(CC) $(CFLAGS) -include inttypes.h -c $< -o $@

bb_coex_stubs.o: $(FRTC)/src/bb_coex_stubs.c
	$(CC) $(CFLAGS) -c $< -o $@

BLOB_DIR := blobs
LIBBTDM  := $(BLOB_DIR)/libbtdm_app.a
LIBPHY   := $(BLOB_DIR)/libphy.a

all: ble_vhci_smoke.exe

# The ROM-symbols fragment is a plain extra linker-script input (not
# passed via -T, which would replace the BSP's default script) - GNU ld
# parses any non-object text file given on the command line as
# additional script content merged into the link, so this only adds the
# ~172 PROVIDE()'d ROM addresses on top of the BSP's real default script.
ROM_SYMS := $(BT_DRIVER)/rom-linker-patch/btdm-rom-symbols.ld

ble_vhci_smoke.exe: $(OBJS) $(LIBBTDM) $(LIBPHY) $(ROM_SYMS) linkcmds-patched.stamp
	$(CC) $(OBJS) $(LIBBTDM) $(LIBPHY) $(ROM_SYMS) -o $@ $(LDFLAGS)

# Fetching the blobs (not vendored into the repo - large Apache-2.0
# "Object form" binaries, see ../../upstream-bt-driver/vendor/README.md
# for the licensing recon).
#
# Pinned to the EXACT submodule commits real ESP-IDF v5.3.1 uses (read
# from esp-idf.git's own .gitmodules + `git ls-tree` of the v5.3.1 tag),
# not just "whatever HEAD is today" - an earlier build of this example
# used an unpinned `git clone --depth 1` (grabs the default branch tip)
# and that version skew against the v5.3.1-vintage vendored bt.c was the
# real cause of most of a ~14-symbol "genuinely missing ROM/blob symbol"
# list once investigated for real (see ../../upstream-bt-driver/
# vendor/README.md's "Blob version pinning" section) - 11 of those 14
# were simply a newer blob commit not defining/needing what this bt.c
# expects, not anything actually missing from Espressif's real output.
# 2026-08-26: re-pinned from IDF v5.3.1's commits to the commits paired
# with bt.c's own newer vendored version (see sdkconfig-compat.h's
# "Re-vendoring bt.c" note) - found via Zephyr's ESP32-C3 BLE port, which
# turned out to just track real ESP-IDF's own `master` branch. BTLIB_COMMIT
# is exactly the commit real IDF's own bt.c was updated to pair with
# (esp-idf.git commit 8da824cd0ede2d6c7317c5a65504bce78762b67b, "fix(bt):
# Update bt lib for ESP32-C3 and ESP32-S3" - 2 minutes after this blob
# commit's own timestamp). PHYLIB_COMMIT is not re-pinned here (real IDF's
# esp_phy component wasn't part of this bt.c/blob pairing change).
BTLIB_COMMIT := 0a08c4b32f3666003080b662a1a61794da24ff0f
PHYLIB_COMMIT := 06e7625de197bc12797dd701d6762229bca01826

$(LIBBTDM):
	mkdir -p $(BLOB_DIR)
	git clone https://github.com/espressif/esp32c3-bt-lib.git $(BLOB_DIR)/.esp32c3-bt-lib
	cd $(BLOB_DIR)/.esp32c3-bt-lib && git checkout $(BTLIB_COMMIT)
	cp $(BLOB_DIR)/.esp32c3-bt-lib/esp32c3/libbtdm_app.a $@

$(LIBPHY):
	mkdir -p $(BLOB_DIR)
	git clone https://github.com/espressif/esp-phy-lib.git $(BLOB_DIR)/.esp-phy-lib
	cd $(BLOB_DIR)/.esp-phy-lib && git checkout $(PHYLIB_COMMIT)
	cp $(BLOB_DIR)/.esp-phy-lib/esp32c3/libphy.a $@

# Applying ../../upstream-bt-driver/linker-section-patch/ to the
# installed BSP's linkcmds.base - see that directory's README.md for why
# this is a post-build patch, not a source-file diff, and why it's
# idempotent (safe to re-run this target on every build).
linkcmds-patched.stamp:
	python3 $(BT_DRIVER)/linker-section-patch/apply-patch.py \
	  $(RTEMS_ROOT)/riscv-rtems7/esp32c3db/lib/linkcmds.base
	touch $@

clean:
	rm -f $(OBJS) ble_vhci_smoke.exe linkcmds-patched.stamp

.PHONY: all clean
