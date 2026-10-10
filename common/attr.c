/* SPDX-License-Identifier: GPL-2.0-only */
/* Typed attribute encoding: one implementation for the module and the tools. */
#include "moobfs_attr.h"

int moob_attr_name_ok(const char *name, size_t len)
{
	size_t i;

	if (!len || len > MOOB_ATTR_NAME_MAX)
		return 0;
	if (name[0] == '.' || name[len - 1] == '.')
		return 0;
	for (i = 0; i < len; i++) {
		char c = name[i];

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.'))
			return 0;
		if (c == '.' && name[i - 1] == '.')
			return 0;
	}
	return 1;
}

int moob_attr_name_reserved(const char *name, size_t len)
{
	return len == sizeof(MOOB_ATTR_FORMAT_NAME) - 1 &&
	       !memcmp(name, MOOB_ATTR_FORMAT_NAME, len);
}

int moob_utf8_valid(const ma_u8 *p, size_t n)
{
	size_t i = 0;

	while (i < n) {
		ma_u8 c = p[i];
		size_t need;
		ma_u8 min2 = 0x80, max2 = 0xbf;

		if (c == 0)
			return 0;
		if (c < 0x80) {
			i++;
			continue;
		}
		if (c >= 0xc2 && c <= 0xdf) {
			need = 1;
		} else if (c >= 0xe0 && c <= 0xef) {
			need = 2;
			if (c == 0xe0)
				min2 = 0xa0;	/* no overlong */
			if (c == 0xed)
				max2 = 0x9f;	/* no surrogates */
		} else if (c >= 0xf0 && c <= 0xf4) {
			need = 3;
			if (c == 0xf0)
				min2 = 0x90;
			if (c == 0xf4)
				max2 = 0x8f;	/* no code points above U+10FFFF */
		} else {
			return 0;
		}
		if (n - i <= need)
			return 0;
		if (p[i + 1] < min2 || p[i + 1] > max2)
			return 0;
		if (need >= 2 && (p[i + 2] < 0x80 || p[i + 2] > 0xbf))
			return 0;
		if (need == 3 && (p[i + 3] < 0x80 || p[i + 3] > 0xbf))
			return 0;
		i += need + 1;
	}
	return 1;
}

int moob_attr_validate(const ma_u8 *value, size_t size)
{
	size_t payload;

	if (!size)
		return MOOB_ATTR_EEMPTY;
	payload = size - 1;
	switch (value[0]) {
	case MOOB_T_STRING:
		if (payload > MOOB_ATTR_STRING_MAX)
			return MOOB_ATTR_ESIZE;
		if (!moob_utf8_valid(value + 1, payload))
			return MOOB_ATTR_EUTF8;
		return MOOB_ATTR_OK;
	case MOOB_T_INT64:
	case MOOB_T_TIME:
		return payload == 8 ? MOOB_ATTR_OK : MOOB_ATTR_ESIZE;
	case MOOB_T_DOUBLE: {
		ma_u64 bits;

		if (payload != 8)
			return MOOB_ATTR_ESIZE;
		bits = moob_attr_get64(value + 1);
		/* exponent all ones with a non-zero mantissa is NaN; infinities are allowed */
		if (((bits >> 52) & 0x7ff) == 0x7ff && (bits & 0xfffffffffffffULL))
			return MOOB_ATTR_EVALUE;
		return MOOB_ATTR_OK;
	}
	case MOOB_T_BOOL:
		if (payload != 1)
			return MOOB_ATTR_ESIZE;
		return value[1] <= 1 ? MOOB_ATTR_OK : MOOB_ATTR_EVALUE;
	case MOOB_T_RAW:
		return payload <= MOOB_ATTR_RAW_MAX ? MOOB_ATTR_OK : MOOB_ATTR_ESIZE;
	default:
		return MOOB_ATTR_ETYPE;
	}
}

const char *moob_attr_strerror(int err)
{
	switch (err) {
	case MOOB_ATTR_OK: return "ok";
	case MOOB_ATTR_EBADNAME: return "invalid attribute name";
	case MOOB_ATTR_ERESERVED: return "reserved attribute name";
	case MOOB_ATTR_EEMPTY: return "empty value";
	case MOOB_ATTR_ETYPE: return "unknown type byte";
	case MOOB_ATTR_ESIZE: return "payload size does not match type";
	case MOOB_ATTR_EUTF8: return "string is not valid UTF-8";
	case MOOB_ATTR_EVALUE: return "payload is not a valid value";
	default: return "unknown error";
	}
}

const char *moob_attr_type_name(int type)
{
	switch (type) {
	case MOOB_T_STRING: return "string";
	case MOOB_T_INT64: return "int64";
	case MOOB_T_DOUBLE: return "double";
	case MOOB_T_BOOL: return "bool";
	case MOOB_T_TIME: return "time";
	case MOOB_T_RAW: return "raw";
	default: return "unknown";
	}
}

unsigned int moob_attr_parse_format(const ma_u8 *value, size_t size)
{
	unsigned int v = 0;
	size_t i;

	if (!size || size > 9)
		return 0;
	for (i = 0; i < size; i++) {
		if (value[i] < '0' || value[i] > '9')
			return 0;
		v = v * 10 + (unsigned int)(value[i] - '0');
	}
	return v;
}
