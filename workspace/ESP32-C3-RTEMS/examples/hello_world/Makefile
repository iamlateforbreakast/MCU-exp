# Builds hello_world.exe by compiling init.c and linking it against the
# esp32c3db BSP, using the same riscv-rtems7-gcc + pkg-config pattern
# documented in rtems_ubuntu_build.md for the SPARC/RISC-V examples.
#
# RTEMS's own pkgconfig support is documented upstream as experimental and
# can be inconsistent across BSPs, and esp32c3db is unreleased, so rather
# than hardcode a guessed .pc filename/location, find it: search RTEMS_ROOT
# for whatever *.pc file the esp32c3db BSP install actually produced and
# derive both PKG_CONFIG_PATH and the package name from it.
RTEMS_ROOT  ?= $(HOME)/rtems/7
CC          := riscv-rtems7-gcc
BSP_PC_FILE := $(firstword $(shell find $(RTEMS_ROOT) -name '*esp32c3db*.pc' 2>/dev/null))
BSP_PC      := $(basename $(notdir $(BSP_PC_FILE)))

ifeq ($(BSP_PC_FILE),)
$(error No *esp32c3db*.pc file found under $(RTEMS_ROOT) - run \
  "find $(RTEMS_ROOT) -name '*.pc'" to see what pkgconfig files the BSP \
  install actually produced, and adjust this Makefile's BSP_PC_FILE search)
endif

export PKG_CONFIG_PATH := $(dir $(BSP_PC_FILE))

CFLAGS  := -Wall -Wextra -O2 -g -fdata-sections -ffunction-sections \
           $(shell pkg-config --cflags $(BSP_PC))
LDFLAGS := $(shell pkg-config --variable=ABI_FLAGS $(BSP_PC)) \
           $(shell pkg-config --libs $(BSP_PC))

all: hello_world.exe

hello_world.exe: init.o
	$(CC) init.o -o $@ $(LDFLAGS)

init.o: init.c
	$(CC) $(CFLAGS) -c init.c -o $@

clean:
	rm -f init.o hello_world.exe

.PHONY: all clean
