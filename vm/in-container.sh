#!/bin/sh
set -eu
cd /work
KERNEL=6.12.111+deb13-amd64
test -d "/lib/modules/$KERNEL/build"
make tools BUILD=build/linux
MOOB_BUILD_DIR=build/linux python3 tests/test_images.py
make module KDIR="/lib/modules/$KERNEL/build"
make layer KDIR="/lib/modules/$KERNEL/build"
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
cp layer/moobfs_layer.ko "$STAGE/moobfs_layer.ko"
# Lower filesystems for the layer: ext4, btrfs and XFS as in-tree modules with their
# dependency chains, plus virtio_blk for the disks that carry the lower images.
mkdir -p "$STAGE/mods"
python3 - "$KERNEL" "$STAGE/mods" <<'PY'
import lzma, sys
from pathlib import Path
base = Path('/lib/modules') / sys.argv[1]
dep = {}
for line in (base / 'modules.dep').read_text().splitlines():
    k, _, v = line.partition(':')
    dep[k.strip()] = v.split()
order = []
def visit(m):
    for d in dep.get(m, []):
        visit(d)
    if m not in order:
        order.append(m)
# Filesystems request the crc32c shash through the crypto API; modules.dep does not record that.
for m in ('kernel/crypto/crc32c_generic.ko.xz', 'kernel/drivers/block/virtio_blk.ko.xz',
          'kernel/fs/ext4/ext4.ko.xz', 'kernel/fs/btrfs/btrfs.ko.xz', 'kernel/fs/xfs/xfs.ko.xz'):
    visit(m)
out = Path(sys.argv[2])
with open(out / 'order', 'w') as f:
    for m in order:
        name = Path(m).name.replace('.ko.xz', '.ko')
        (out / name).write_bytes(lzma.decompress((base / m).read_bytes()))
        f.write(name + '\n')
print('lower module chain:', ' '.join(Path(m).name for m in order))
PY
# Lower images are sparse disks attached through virtio, not initramfs files.
rm -f build/vm/lower-ext4.img build/vm/lower-btrfs.img build/vm/lower-xfs.img
truncate -s 64M build/vm/lower-ext4.img
mke2fs -q -F -t ext4 -L moob-lower build/vm/lower-ext4.img
truncate -s 256M build/vm/lower-btrfs.img
mkfs.btrfs -q -f -L moob-lower build/vm/lower-btrfs.img
truncate -s 300M build/vm/lower-xfs.img
mkfs.xfs -q -f -L moob-lower build/vm/lower-xfs.img
python3 - "$KERNEL" "$STAGE/loop.ko" <<'PY'
import lzma
from pathlib import Path
import sys
source = Path('/lib/modules') / sys.argv[1] / 'kernel/drivers/block/loop.ko.xz'
Path(sys.argv[2]).write_bytes(lzma.decompress(source.read_bytes()))
PY
gcc -static -O2 -Wall -Wextra -Werror tests/mounted.c -o "$STAGE/mounted"
gcc -static -O2 -Wall -Wextra -Werror tests/layer.c -o "$STAGE/layer"
gcc -static -O2 -Itests/third_party/fsx tests/third_party/fsx/fsx.c -o "$STAGE/fsx"
MOOB_BUILD_DIR=build/linux python3 tests/test_images.py --fixtures "$STAGE/data"
build/linux/fsck.moobfs "$STAGE/data/shared.img" > build/vm/image-report.json
cp "$STAGE/data/shared.img" build/vm/shared.img
cp kernel/moobfs.ko build/vm/moobfs.ko
cp layer/moobfs_layer.ko build/vm/moobfs_layer.ko
cp "/boot/config-$KERNEL" build/vm/kernel.config
{
    printf 'Kernel: %s\n' "$KERNEL"
    gcc --version | head -1
    qemu-system-x86_64 --version | head -1
    dpkg-query -W 'linux-image-*' 'linux-headers-*' gcc qemu-system-x86
    sha256sum "/boot/vmlinuz-$KERNEL" kernel/moobfs.ko layer/moobfs_layer.ko build/vm/shared.img
} > build/vm/environment.txt
(cd "$STAGE" && find . -print0 | sort -z | cpio --null -o --format=newc 2>/dev/null | gzip -n) > build/vm/initramfs.cpio.gz
set +e
timeout 600 qemu-system-x86_64 \
    -machine q35 -accel tcg -cpu max -m 512M -smp 2 \
    -drive file=build/vm/lower-ext4.img,format=raw,if=virtio \
    -drive file=build/vm/lower-btrfs.img,format=raw,if=virtio \
    -drive file=build/vm/lower-xfs.img,format=raw,if=virtio \
    -kernel "/boot/vmlinuz-$KERNEL" -initrd build/vm/initramfs.cpio.gz \
    -append 'console=ttyS0 rdinit=/init panic=-1 loglevel=4' \
    -nographic -no-reboot -nic none > build/vm/console.log 2>&1
RESULT=$?
set -e
cat build/vm/console.log
test "$RESULT" -eq 0
grep -q '^MOOBFS_VM_PASS:' build/vm/console.log
grep -q '^MOOBFS_LAYER_PASS:' build/vm/console.log
! grep -q '^MOOBFS_VM_FAIL:' build/vm/console.log
