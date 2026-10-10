# Reproduce M1

## Portable tools

Requirements: C11 compiler, Make and Python 3. On Linux or macOS, run `make test`.
The nine test methods include exact-byte sharing, distinct data, hash boundary
sizes, an independent CRC vector, deterministic images, overwrite refusal, limits,
128 objects and 15 malformed variants. Boundary lengths range from 0 to 65536 bytes.

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

## Phase 2a: memory layer stacked over ext4

The same `make vm-test` run continues after the M1 checks (see
[ADR-0007](decisions/0007-stacked-memory-layer.md) and [spec-m2.md](spec-m2.md)):

1. `make layer` builds `layer/moobfs_layer.ko` against the pinned headers.
2. The container creates a 64 MiB ext4 image with `mke2fs` and resolves the
   `ext4.ko` module chain from `modules.dep`, decompressing each module into the
   guest.
3. The guest mounts ext4 on `/lower`, then `mount -t moobfs_layer -o lower=/lower
   none /mnt/layer`. Mounting without `lower=`, with a missing directory, or on top
   of another layer mount must fail.
4. `tests/layer.c` checks pass-through semantics (create, write, pread, append,
   truncate, shared and private mmap, mkdir/rmdir, readdir, chmod, unlink) and that
   every change is visible in `/lower` with the same inode numbers and bytes.
   Links and renames must return `EPERM` in this phase.
5. `fsx` from xfstests (vendored under `tests/third_party/fsx/`, see its README)
   runs three seeds over the layer, and the resulting file is compared with the
   lower copy.
6. Unmount, remount, unload, and a `dmesg` scan for `BUG`, `Oops`, `WARNING` or
   `KASAN` close the run. Success adds a `MOOBFS_LAYER_PASS:` marker.

The distribution kernel still has no KASAN or lockdep; those runs belong to a
separate virtme-ng kernel and are not implied by this harness.

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
