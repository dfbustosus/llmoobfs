CC ?= cc
CFLAGS ?= -O2 -g
WARN = -std=c11 -Wall -Wextra -Werror -Wconversion -Wshadow
COMMON = common/hash.c common/format.c
HEADERS = include/moobfs_format.h
KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD ?= build/host

.PHONY: all tools attr-tools test fuzz module layer vm-test
all: tools
tools: $(BUILD)/mkfs.moobfs $(BUILD)/fsck.moobfs $(BUILD)/attr_test
$(BUILD):
	mkdir -p $(BUILD)
$(BUILD)/mkfs.moobfs: tools/mkfs.c $(COMMON) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude tools/mkfs.c $(COMMON) $(LDFLAGS) -o $@
$(BUILD)/fsck.moobfs: tools/fsck.c $(COMMON) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude tools/fsck.c $(COMMON) $(LDFLAGS) -o $@
$(BUILD)/attr_test: tests/attr_test.c common/attr.c include/moobfs_attr.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude tests/attr_test.c common/attr.c $(LDFLAGS) -o $@
# Linux-only tools over the xattr API (built statically inside the VM container too).
attr-tools: $(BUILD)/moobfs-attr $(BUILD)/moobfs-check $(BUILD)/moobfs-index
$(BUILD)/moobfs-attr: tools/moobfs-attr.c common/attr.c include/moobfs_attr.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude tools/moobfs-attr.c common/attr.c $(LDFLAGS) -o $@
$(BUILD)/moobfs-check: tools/moobfs-check.c common/attr.c include/moobfs_attr.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude tools/moobfs-check.c common/attr.c $(LDFLAGS) -o $@
$(BUILD)/moobfs-index: tools/moobfs-index.c tools/query-common.h common/attr.c include/moobfs_attr.h include/moobfs_ioctl.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude tools/moobfs-index.c common/attr.c $(LDFLAGS) -o $@
test: tools
	$(BUILD)/attr_test
	MOOB_BUILD_DIR=$(BUILD) python3 tests/test_images.py
# libFuzzer run over the attribute encoding (needs LLVM clang with libFuzzer, e.g. on Linux).
FUZZ_TIME ?= 20
fuzz: | $(BUILD)
	clang -O1 -g -fsanitize=fuzzer,address,undefined -Iinclude tests/fuzz_attr.c common/attr.c -o $(BUILD)/fuzz_attr
	mkdir -p $(BUILD)/fuzz-corpus
	$(BUILD)/fuzz_attr -max_total_time=$(FUZZ_TIME) -max_len=4200 tests/fuzz-corpus/attr $(BUILD)/fuzz-corpus
module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel modules
layer:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/layer modules
vm-test:
	sh vm/run.sh
