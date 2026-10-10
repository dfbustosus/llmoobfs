/* SPDX-License-Identifier: GPL-2.0-only */
/* Host unit tests for the typed attribute encoding (no filesystem involved). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "moobfs_attr.h"

static int failures;
#define EXPECT(cond) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int check(const char *bytes, size_t n)
{
	return moob_attr_validate((const ma_u8 *)bytes, n);
}

int main(void)
{
	ma_u8 buf[MOOB_ATTR_VALUE_MAX + 8];
	size_t i;

	/* names */
	EXPECT(moob_attr_name_ok("kind", 4));
	EXPECT(moob_attr_name_ok("a.b_c9", 6));
	EXPECT(!moob_attr_name_ok("", 0));
	EXPECT(!moob_attr_name_ok("Kind", 4));
	EXPECT(!moob_attr_name_ok("a b", 3));
	EXPECT(!moob_attr_name_ok(".a", 2));
	EXPECT(!moob_attr_name_ok("a.", 2));
	EXPECT(!moob_attr_name_ok("a..b", 4));
	EXPECT(!moob_attr_name_ok("a-b", 3));
	memset(buf, 'a', sizeof buf);
	EXPECT(moob_attr_name_ok((char *)buf, MOOB_ATTR_NAME_MAX));
	EXPECT(!moob_attr_name_ok((char *)buf, MOOB_ATTR_NAME_MAX + 1));
	EXPECT(moob_attr_name_reserved("format", 6));
	EXPECT(!moob_attr_name_reserved("formats", 7));
	EXPECT(!moob_attr_name_reserved("forma", 5));

	/* strings */
	EXPECT(check("\x01", 1) == MOOB_ATTR_OK);
	EXPECT(check("\x01note", 5) == MOOB_ATTR_OK);
	EXPECT(check("\x01" "caf\xc3\xa9", 6) == MOOB_ATTR_OK);
	EXPECT(check("\x01" "\xe2\x82\xac", 4) == MOOB_ATTR_OK);
	EXPECT(check("\x01" "\xf0\x9f\x98\x80", 5) == MOOB_ATTR_OK);
	EXPECT(check("\x01" "a\0b", 4) == MOOB_ATTR_EUTF8);
	EXPECT(check("\x01" "\xc3", 2) == MOOB_ATTR_EUTF8);
	EXPECT(check("\x01" "\xc0\xaf", 3) == MOOB_ATTR_EUTF8);		/* overlong */
	EXPECT(check("\x01" "\xe0\x80\x80", 4) == MOOB_ATTR_EUTF8);	/* overlong */
	EXPECT(check("\x01" "\xed\xa0\x80", 4) == MOOB_ATTR_EUTF8);	/* surrogate */
	EXPECT(check("\x01" "\xf4\x90\x80\x80", 5) == MOOB_ATTR_EUTF8);	/* > U+10FFFF */
	EXPECT(check("\x01" "\xff", 2) == MOOB_ATTR_EUTF8);
	EXPECT(check("\x01" "\x80", 2) == MOOB_ATTR_EUTF8);
	buf[0] = MOOB_T_STRING;
	for (i = 1; i <= MOOB_ATTR_STRING_MAX + 1; i++)
		buf[i] = 'x';
	EXPECT(moob_attr_validate(buf, 1 + MOOB_ATTR_STRING_MAX) == MOOB_ATTR_OK);
	EXPECT(moob_attr_validate(buf, 2 + MOOB_ATTR_STRING_MAX) == MOOB_ATTR_ESIZE);

	/* fixed-size scalars */
	EXPECT(check("\x02\x01\x00\x00\x00\x00\x00\x00\x00", 9) == MOOB_ATTR_OK);
	EXPECT(check("\x02\x01\x00\x00\x00\x00\x00\x00", 8) == MOOB_ATTR_ESIZE);
	EXPECT(check("\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00", 10) == MOOB_ATTR_ESIZE);
	EXPECT(check("\x05\x00\x00\x00\x00\x00\x00\x00\x80", 9) == MOOB_ATTR_OK);	/* negative time */
	EXPECT(check("\x03\x00\x00\x00\x00\x00\x00\xf0\x3f", 9) == MOOB_ATTR_OK);	/* 1.0 */
	EXPECT(check("\x03\x00\x00\x00\x00\x00\x00\xf0\x7f", 9) == MOOB_ATTR_OK);	/* +inf */
	EXPECT(check("\x03\x01\x00\x00\x00\x00\x00\xf8\x7f", 9) == MOOB_ATTR_EVALUE);	/* NaN */
	EXPECT(check("\x03\x00\x00\x00\x00\x00\x00\xf8\x7f", 9) == MOOB_ATTR_EVALUE);	/* quiet NaN */
	EXPECT(check("\x03\x00\x00\x00\x00\x00\x00\xf0", 8) == MOOB_ATTR_ESIZE);
	EXPECT(check("\x04\x00", 2) == MOOB_ATTR_OK);
	EXPECT(check("\x04\x01", 2) == MOOB_ATTR_OK);
	EXPECT(check("\x04\x02", 2) == MOOB_ATTR_EVALUE);
	EXPECT(check("\x04", 1) == MOOB_ATTR_ESIZE);
	EXPECT(check("\x04\x01\x00", 3) == MOOB_ATTR_ESIZE);

	/* raw */
	EXPECT(check("\x06", 1) == MOOB_ATTR_OK);
	EXPECT(check("\x06\x00\xff", 3) == MOOB_ATTR_OK);
	buf[0] = MOOB_T_RAW;
	EXPECT(moob_attr_validate(buf, 1 + MOOB_ATTR_RAW_MAX) == MOOB_ATTR_OK);
	EXPECT(moob_attr_validate(buf, 2 + MOOB_ATTR_RAW_MAX) == MOOB_ATTR_ESIZE);

	/* framing */
	EXPECT(check("", 0) == MOOB_ATTR_EEMPTY);
	EXPECT(check("\x00", 1) == MOOB_ATTR_ETYPE);
	EXPECT(check("\x07", 1) == MOOB_ATTR_ETYPE);
	EXPECT(check("\xff\x01", 2) == MOOB_ATTR_ETYPE);

	/* format version */
	EXPECT(moob_attr_parse_format((const ma_u8 *)"1", 1) == 1);
	EXPECT(moob_attr_parse_format((const ma_u8 *)"12", 2) == 12);
	EXPECT(moob_attr_parse_format((const ma_u8 *)"", 0) == 0);
	EXPECT(moob_attr_parse_format((const ma_u8 *)"x", 1) == 0);
	EXPECT(moob_attr_parse_format((const ma_u8 *)"1\n", 2) == 0);
	EXPECT(moob_attr_parse_format((const ma_u8 *)"1234567890", 10) == 0);

	/* 64-bit helpers round-trip */
	moob_attr_put64(buf, 0x0102030405060708ULL);
	EXPECT(buf[0] == 8 && buf[7] == 1 && moob_attr_get64(buf) == 0x0102030405060708ULL);

	if (failures) {
		fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	puts("attr encoding tests: 60 checks passed");
	return 0;
}
