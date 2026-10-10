#!/bin/sh
# Runs the layer suite inside a virtme-ng guest booted from a kernel built with
# vm/kasan.config (KASAN, UBSAN, lockdep). The host checkout is the guest's root,
# so this runs from the repository directory with build/ writable.
# Usage (inside the guest, as root): sh vm/kasan-guest.sh
set -u
REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1
OUT=$REPO/build/vng
mkdir -p "$OUT/tools"
for t in git sqlite3 dmsetup mke2fs e2fsck setfacl getfacl; do
    p=$(command -v "$t") || { echo "MOOBFS_VM_FAIL: missing $t on the host"; exit 1; }
    ln -sf "$p" "$OUT/tools/$t"
done
fail() {
    echo "MOOBFS_VM_FAIL: $*"
    dmesg | tail -80
    dmesg > "$OUT/dmesg.txt"
    exit 1
}
echo "kernel: $(uname -r); KASAN init lines in dmesg: $(dmesg | grep -c 'KernelAddressSanitizer'); lockdep: $(grep -c . /proc/lockdep_stats 2>/dev/null || echo absent)"
BIN=$OUT TOOLS=$OUT/tools MODS=$OUT/nomods LOWER=/tmp/moob-lower LAYER=/tmp/moob-layer   # the 9p root is read-only
DEV_EXT4=/dev/vda DEV_BTRFS=/dev/vdb DEV_XFS=/dev/vdc DEV_SCRATCH=/dev/vdd DEV_LOG=/dev/vde
export BIN TOOLS MODS LOWER LAYER DEV_EXT4 DEV_BTRFS DEV_XFS DEV_SCRATCH DEV_LOG
for d in $DEV_EXT4 $DEV_BTRFS $DEV_XFS $DEV_SCRATCH $DEV_LOG; do [ -b "$d" ] || fail "missing disk $d"; done
. "$REPO/vm/suite.sh"
dmesg > "$OUT/dmesg.txt"
if grep -E 'BUG: KASAN|KASAN: |BUG:|Oops:|WARNING:|possible (circular|recursive) locking|INFO: (task|rcu)|UBSAN:' "$OUT/dmesg.txt"; then
    echo "MOOBFS_VM_FAIL: kernel diagnostics under KASAN/lockdep"
    exit 1
fi
echo "MOOBFS_KASAN_PASS: layer suite under KASAN, UBSAN and lockdep with no kernel diagnostics"
