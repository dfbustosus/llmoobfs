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
