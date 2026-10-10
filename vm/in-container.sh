#!/bin/sh
set -eu
cd /work
KERNEL=6.12.111+deb13-amd64
test -d "/lib/modules/$KERNEL/build"
make tools BUILD=build/linux
MOOB_BUILD_DIR=build/linux python3 tests/test_images.py
make module KDIR="/lib/modules/$KERNEL/build"
mkdir -p build/vm
# Temporary work is confined to build; generated image creators remain exclusive.
STAGE=$(mktemp -d /work/build/vm/stage.XXXXXX)
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/bin" "$STAGE/dev" "$STAGE/proc" "$STAGE/sys" "$STAGE/data"
cp /bin/busybox "$STAGE/bin/busybox"
ln -s busybox "$STAGE/bin/sh"
cp vm/init "$STAGE/init"
chmod +x "$STAGE/init"
cp kernel/moobfs.ko "$STAGE/moobfs.ko"
python3 - "$KERNEL" "$STAGE/loop.ko" <<'PY'
import lzma
from pathlib import Path
import sys
source = Path('/lib/modules') / sys.argv[1] / 'kernel/drivers/block/loop.ko.xz'
Path(sys.argv[2]).write_bytes(lzma.decompress(source.read_bytes()))
PY
gcc -static -O2 -Wall -Wextra -Werror tests/mounted.c -o "$STAGE/mounted"
MOOB_BUILD_DIR=build/linux python3 tests/test_images.py --fixtures "$STAGE/data"
build/linux/fsck.moobfs "$STAGE/data/shared.img" > build/vm/image-report.json
cp "$STAGE/data/shared.img" build/vm/shared.img
cp kernel/moobfs.ko build/vm/moobfs.ko
cp "/boot/config-$KERNEL" build/vm/kernel.config
{
    printf 'Kernel: %s\n' "$KERNEL"
    gcc --version | head -1
    qemu-system-x86_64 --version | head -1
    dpkg-query -W 'linux-image-*' 'linux-headers-*' gcc qemu-system-x86
    sha256sum "/boot/vmlinuz-$KERNEL" kernel/moobfs.ko build/vm/shared.img
} > build/vm/environment.txt
(cd "$STAGE" && find . -print0 | sort -z | cpio --null -o --format=newc 2>/dev/null | gzip -n) > build/vm/initramfs.cpio.gz
set +e
timeout 180 qemu-system-x86_64 \
    -machine q35 -accel tcg -cpu max -m 384M -smp 2 \
    -kernel "/boot/vmlinuz-$KERNEL" -initrd build/vm/initramfs.cpio.gz \
    -append 'console=ttyS0 rdinit=/init panic=-1 loglevel=4' \
    -nographic -no-reboot -nic none > build/vm/console.log 2>&1
RESULT=$?
set -e
cat build/vm/console.log
test "$RESULT" -eq 0
grep -q '^MOOBFS_VM_PASS:' build/vm/console.log
! grep -q '^MOOBFS_VM_FAIL:' build/vm/console.log
