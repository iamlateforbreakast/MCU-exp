# Builds hello_world.exe by compiling init.c and linking it against the
# esp32c3db BSP, using the same riscv-rtems7-gcc + pkg-config pattern
# documented in rtems_ubuntu_build.md for the SPARC/RISC-V examples.
#
# BSP_PC's name is inferred from RTEMS's standard waf install naming
# convention (<arch>-rtems<ver>-<bsp>.pc) - it hasn't been confirmed against
# a real build of this .pc file; adjust it if pkg-config can't find it (see
# `ls $(RTEMS_ROOT)/lib/pkgconfig` to check the actual name).
RTEMS_ROOT ?= $(HOME)/rtems/7
BSP_PC     := riscv-rtems7-esp32c3db
CC         := riscv-rtems7-gcc

export PKG_CONFIG_PATH := $(RTEMS_ROOT)/lib/pkgconfig

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
