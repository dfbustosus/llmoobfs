/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * libFuzzer target for the typed attribute encoding. Build with
 *   clang -fsanitize=fuzzer,address,undefined -Iinclude tests/fuzz_attr.c common/attr.c
 * The first byte of each input selects the function; the rest is the payload.
 */
#include <stdint.h>
#include <stddef.h>
#include "moobfs_attr.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (!size)
		return 0;
	switch (data[0] % 3) {
	case 0:
		(void)moob_attr_validate(data + 1, size - 1);
		break;
	case 1:
		(void)moob_attr_name_ok((const char *)data + 1, size - 1);
		(void)moob_attr_name_reserved((const char *)data + 1, size - 1);
		break;
	default:
		(void)moob_attr_parse_format(data + 1, size - 1);
		break;
	}
	return 0;
}
