# Reproduce M1

## Portable tools

Requirements: C11 compiler, Make and Python 3. On Linux or macOS, run `make test`.
The nine test methods include exact-byte sharing, distinct data, hash boundary
sizes, an independent CRC vector, deterministic images, overwrite refusal, limits,
128 objects and 15 malformed variants. Boundary lengths range from 0 to 65536 bytes.

`make test` also runs `tests/attr_test.c`, the host unit tests of the typed attribute
encoding shared by the kernel layer and the tools. `make fuzz FUZZ_TIME=30` runs the
libFuzzer target in `tests/fuzz_attr.c` (LLVM clang with libFuzzer, as in CI; Apple
clang does not ship it). `make attr-tools` builds the Linux-only `moobfs-attr` and
`moobfs-check`.

Run tools with sanitizers in a separate output directory:

```sh
make test BUILD=build/asan \
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined'
```

## Native module in a disposable VM

With Docker running, `make vm-test` builds the container described in
`vm/Dockerfile`, compiles the module against **6.12.111+deb13-amd64** headers and
boots that matching kernel in QEMU TCG. Docker is only the build/launch environment.
The module is loaded inside QEMU, not into the host or Docker kernel. No privileged
container, KVM device or network inside the guest is required.

Expect a longer first run for downloads and x86 emulation on ARM hosts. The QEMU
phase has a 180-second timeout; outer build time depends on downloads. If the pinned
Debian package becomes unavailable, update the image/kernel together through a
reviewed change and repeat the suite; do not silently substitute host headers.
The Debian base digest and kernel package are fixed, but auxiliary apt packages
are not fully locked. This is not a claim of bit-for-bit toolchain reproducibility.

Generated evidence in `build/vm/`:

| File | Purpose |
| --- | --- |
| `console.log` | Mount/read checks and final PASS/FAIL marker |
| `image-report.json` | Inodes, shared object, offsets and hashes |
| `environment.txt` | Kernel, compiler, QEMU, package versions and SHA-256s |
| `kernel.config` | Exact tested kernel configuration |
| `shared.img`, `moobfs.ko` | Image and module used for the experiment |
| `initramfs.cpio.gz` | Test guest filesystem |

Success requires QEMU to exit successfully, a `MOOBFS_VM_PASS:` marker and no
`MOOBFS_VM_FAIL:` marker. Expected rejection messages for malformed images and
write attempts are part of the test. Unsigned/out-of-tree module taint is expected.

The current distribution kernel does not enable KASAN or PROVE_LOCKING. Tool
sanitizers do not replace kernel sanitizers. No performance or durability results
are implied by these correctness checks.

## Phases 2a–2b: memory layer stacked over ext4, btrfs and XFS

The same `make vm-test` run continues after the M1 checks (see
[ADR-0007](decisions/0007-stacked-memory-layer.md) and [spec-m2.md](spec-m2.md)):

1. `make layer` builds `layer/moobfs_layer.ko` against the pinned headers.
2. The container creates sparse ext4 (64 MiB), btrfs (256 MiB) and XFS (300 MiB)
   images, attached to QEMU as virtio disks, and resolves the module chains for
   `virtio_blk`, `ext4`, `btrfs` and `xfs` from `modules.dep` (plus `crc32c_generic`,
   requested through the crypto API).
3. For each lower filesystem the guest mounts it on `/lower`, then `mount -t
   moobfs_layer -o lower=/lower none /mnt/layer`, runs the checks below, remounts
   with `nolinks` and repeats them. Mounting without `lower=`, with a missing
   directory, or on top of another layer mount must fail.
4. `tests/layer.c` checks pass-through semantics (create, write, pread, append,
   truncate, shared and private mmap, mkdir/rmdir, readdir, chmod, unlink, rename,
   xattr, hard links and symlinks) and that every change is visible in `/lower` with
   the same inode numbers and bytes. `FICLONE` and `FIDEDUPERANGE` must share
   extents (visible through `FIEMAP`) on btrfs and XFS and fail cleanly on ext4.
5. `moobfs-attr` writes typed attributes through the layer and `moobfs-check` validates
   the lower tree; an invalid encoding planted directly on the lower tree must be
   reported with its reason (exit code 2).
6. A six-inode tree with attributes of every indexable type is queried through the
   `MOOB_IOC_QUERY` ioctl (`tests/query.c`) and through the user-space oracle
   `moobfs-index` on the lower tree; both outputs must be identical for ten queries,
   also after renames, unlink, removexattr and a remount. `noindex` and
   `index_budget=` are exercised, and malformed requests must return `EINVAL`.
7. `fsx` from xfstests (vendored under `tests/third_party/fsx/`, see its README)
   runs three seeds over the layer, and the resulting file is compared with the
   lower copy.
8. Acceptance with real clients on the layer over ext4: a git repository (three commits,
   `fsck --strict`), SQLite in WAL mode with a writer and a reader in concurrent processes
   (six variants, all must keep 200 of 200 rows), and the memory-client convention. Then a
   power-loss replay: a fresh ext4 under `dm-log-writes`, memories written with marks,
   `replay-log` to each mark on a zeroed disk, `e2fsck -fn`, and the check that memories
   synced before the mark are whole and later ones absent. Finally `dm-flakey` with
   `error_writes`: errors must propagate, nothing may crash, and the tree must come back
   clean. Success prints `MOOBFS_ACCEPTANCE_PASS:`.
9. Unmount, unload, and a `dmesg` scan for `BUG`, `Oops`, `WARNING` or `KASAN`
   close the run. Success prints one `LAYER_LOWER_PASS:` line per lower filesystem
   and a final `MOOBFS_LAYER_PASS:` marker.

The distribution kernel has no KASAN or lockdep. Those run in a separate workflow,
`.github/workflows/kasan.yml` (on demand with `gh workflow run kasan.yml`, and
weekly): it downloads the pinned upstream `linux-6.12.112` tarball, verifies its
SHA-256, lets virtme-ng generate a minimal configuration plus `vm/kasan.config`
(KASAN, UBSAN, lockdep, the three lower filesystems, device mapper), builds the
kernel, builds `moobfs_layer.ko` and the test binaries against it, and boots the
kernel with the host checkout as root and five virtio disks. `vm/kasan-guest.sh`
runs the same `vm/suite.sh` the initramfs harness uses and fails on any `KASAN`,
`WARNING`, `BUG`, lockdep or UBSAN report in `dmesg`. Success prints
`MOOBFS_KASAN_PASS:`; the console, `dmesg` and `.config` are kept as artifacts.

## Manual mount inside your own disposable Linux VM

Use the tested kernel and matching headers; compile the module in that VM.
The following commands assume the image from the README has already been created:

```sh
make module KDIR=/lib/modules/$(uname -r)/build
sudo insmod kernel/moobfs.ko
sudo mkdir -p /mnt/moob
sudo mount -t moobfs -o loop,ro build/demo.img /mnt/moob
ls -li /mnt/moob
cmp /mnt/moob/alpha.txt /mnt/moob/beta.txt
cat /mnt/moob/alpha.txt
sudo umount /mnt/moob
sudo rmmod moobfs
```

Do not alter the backing image while mounted. The signed-module policies of a
particular VM may reject unsigned modules; the provided QEMU harness supplies a
known test environment. Other kernel versions are unverified.

## CI and documentation

GitHub Actions runs portable tests, tool sanitizers and the QEMU test on pushes
and pull requests. VM logs and metadata are retained as temporary Actions artifacts;
permanent conclusions belong in the journal, with links to the relevant run.
The Pages workflow publishes only `site/`, not the entire checkout or test artifacts.
Preview locally with `python3 -m http.server 8080 --directory site`.
