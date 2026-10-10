CC ?= cc
CFLAGS ?= -O2 -g
WARN = -std=c11 -Wall -Wextra -Werror -Wconversion -Wshadow
COMMON = common/hash.c common/format.c
HEADERS = include/moobfs_format.h
KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD ?= build/host

.PHONY: all tools test module vm-test
all: tools
tools: $(BUILD)/mkfs.moobfs $(BUILD)/fsck.moobfs
$(BUILD):
	mkdir -p $(BUILD)
$(BUILD)/mkfs.moobfs: tools/mkfs.c $(COMMON) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude tools/mkfs.c $(COMMON) $(LDFLAGS) -o $@
$(BUILD)/fsck.moobfs: tools/fsck.c $(COMMON) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude tools/fsck.c $(COMMON) $(LDFLAGS) -o $@
test: tools
	MOOB_BUILD_DIR=$(BUILD) python3 tests/test_images.py
module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel modules
vm-test:
	sh vm/run.sh
