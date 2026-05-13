# Compiler settings
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=gnu99 -D_FILE_OFFSET_BITS=64
TARGET  := u_sysfs
SRCS    := u_sysfs.c

# Fetch FUSE 3 compilation and linking options dynamically
FUSE_CFLAGS := $(shell pkg-config fuse3 --cflags)
FUSE_LIBS   := $(shell pkg-config fuse3 --libs)

# Combine build flags
CFLAGS  += $(FUSE_CFLAGS)
LDFLAGS += $(FUSE_LIBS)

# Default target rule
.PHONY: all
all: $(TARGET)

# Compile target binary
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

# System cleanup rule
.PHONY: clean
clean:
	rm -f $(TARGET)

# Direct execution rule for rapid debugging
.PHONY: run
run: all
	@mkdir -p /tmp/sys_camera
	@echo "Mounting FUSE filesystem at /tmp/sys_camera..."
	./$(TARGET) -f /tmp/sys_camera
