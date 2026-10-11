#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Every tracked C source and header must start with an SPDX line (CONTRIBUTING.md).
set -eu
missing=0
# Tracked paths contain no spaces, so word splitting of the list is intended.
# shellcheck disable=SC2046
for f in $(git ls-files 'layer/*.[ch]' 'common/*.[ch]' 'tools/*.[ch]' 'tests/*.[ch]' 'include/*.[ch]' \
    ':!:tests/third_party/*'); do
	if ! head -n 1 "$f" | grep -q 'SPDX-License-Identifier:'; then
		echo "missing SPDX header: $f"
		missing=1
	fi
done
[ "$missing" -eq 0 ]
