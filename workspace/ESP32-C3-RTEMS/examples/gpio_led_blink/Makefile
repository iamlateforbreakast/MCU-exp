# Builds gpio_led_blink.exe by compiling init.c and linking it against the
# esp32c3db BSP, using the same riscv-rtems7-gcc + pkg-config pattern as
# ../hello_world/Makefile (see that file's comments for why the .pc file
# is discovered rather than hardcoded).
#
# NOTE: this links against RTEMS's generic GPIO API (bsp/gpio.h), which
# requires the draft driver in ../../upstream-gpio-driver/ to have been
# integrated into the esp32c3db BSP build first - see that driver's
# README. Until then this will fail to build with an undefined-reference
# or missing-header error, not a working image.
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
export PKG_CONFIG_PATH := $(BSP_PC_DIR)
PKG_CONFIG := PKG_CONFIG_PATH=$(BSP_PC_DIR) pkg-config

CFLAGS  := -Wall -Wextra -O2 -g -fdata-sections -ffunction-sections \
           $(shell $(PKG_CONFIG) --cflags $(BSP_PC))
LDFLAGS := $(shell $(PKG_CONFIG) --variable=ABI_FLAGS $(BSP_PC)) \
           $(shell $(PKG_CONFIG) --libs $(BSP_PC))

all: gpio_led_blink.exe

gpio_led_blink.exe: init.o
	$(CC) init.o -o $@ $(LDFLAGS)

init.o: init.c
	$(CC) $(CFLAGS) -c init.c -o $@

clean:
	rm -f init.o gpio_led_blink.exe

.PHONY: all clean
