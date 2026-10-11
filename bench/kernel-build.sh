#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# bench/kernel-build: builds and installs the benchmark kernel on an x86_64 Debian or Ubuntu
# host (docs/bench-accelerated-guest.md). Same upstream tarball and SHA-256 as the KASAN
# workflow (6.12.112), configured from the running distribution kernel plus vm/bench.config:
# no sanitizers, no lockdep. Produces Debian packages and installs them; reboot afterwards.
#
#   sudo sh bench/kernel-build.sh [BUILD_DIR]        (defaults to build/kernel)
# Needs: build-essential flex bison libelf-dev libssl-dev bc dpkg-dev debhelper rsync kmod
set -eu
KERNEL_VERSION=${KERNEL_VERSION:-6.12.112}
KERNEL_SHA256=${KERNEL_SHA256:-164dc9d1f6c93c61a15e1f071c48379b467f2b17c469cce7223471968208ed03}
here=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-$here/build/kernel}
mkdir -p "$build"
cd "$build"
if [ ! -f "linux-$KERNEL_VERSION.tar.xz" ]; then
    curl -fsSL -o "linux-$KERNEL_VERSION.tar.xz" "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KERNEL_VERSION.tar.xz"
fi
echo "$KERNEL_SHA256  linux-$KERNEL_VERSION.tar.xz" | sha256sum -c -
if [ ! -d src ]; then
    mkdir src && tar -xJf "linux-$KERNEL_VERSION.tar.xz" -C src --strip-components=1
fi
cd src
# start from the running kernel's configuration so every driver this host needs stays enabled
cp "/boot/config-$(uname -r)" .config
sh scripts/kconfig/merge_config.sh -m .config "$here/vm/bench.config" > /dev/null
make olddefconfig > /dev/null
for opt in CONFIG_EXT4_FS CONFIG_BTRFS_FS CONFIG_XFS_FS CONFIG_FS_POSIX_ACL CONFIG_DM_LOG_WRITES CONFIG_DM_FLAKEY CONFIG_USER_NS; do
    grep -q "^$opt=[ym]" .config || { echo "kernel-build: $opt is not enabled after merge" >&2; exit 1; }
done
grep -q '^CONFIG_KASAN=y' .config && { echo "kernel-build: KASAN must be off for a benchmark kernel" >&2; exit 1; }
echo "configured linux-$KERNEL_VERSION from /boot/config-$(uname -r) + vm/bench.config"
make -j"$(nproc)" bindeb-pkg LOCALVERSION=-moobfs-bench KDEB_PKGVERSION="$KERNEL_VERSION-1" > "$build/build.log" 2>&1 || { tail -40 "$build/build.log"; exit 1; }
cd "$build"
# the -dbg package is not needed and fails to install without debug info; install image and headers only
pkgs=""
for f in linux-image-*moobfs-bench*.deb linux-headers-*moobfs-bench*.deb; do
    case $f in *-dbg_*) ;; *) [ -e "$f" ] && pkgs="$pkgs $f" ;; esac
done
echo "packages:$pkgs"
if [ "$(id -u)" = 0 ]; then
    # shellcheck disable=SC2086
    dpkg -i $pkgs
    echo "installed; the newest kernel stays the GRUB default: boot this one once with"
    echo "  GRUB_DEFAULT=saved in /etc/default/grub, update-grub, then grub-reboot '<entry for ${KERNEL_VERSION}-moobfs-bench>'"
    echo "and check: uname -r  (expected ${KERNEL_VERSION}-moobfs-bench)"
else
    echo "not root: install with  cd $build && sudo dpkg -i$pkgs"
fi
