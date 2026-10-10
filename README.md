# MoobFS

**An experimental native Linux filesystem for LLM memory and context.**

[Português](README.pt-BR.md) · [Project site](https://studiomoob.github.io/llmoobfs/) · [Contributing](CONTRIBUTING.md) · [Roadmap](ROADMAP.md)

[![CI](https://github.com/studiomoob/llmoobfs/actions/workflows/ci.yml/badge.svg)](https://github.com/studiomoob/llmoobfs/actions/workflows/ci.yml)
[![License: GPL-2.0-only](https://img.shields.io/badge/license-GPL--2.0--only-blue)](LICENSE)

## Project goal

MoobFS aims to provide a filesystem-level storage foundation for persistent memory
and context consumed by LLM-based agents. The intended use is to retain memories
such as observations, decisions and task records across sessions, share repeated
content and eventually preserve revisions that agents can retrieve as context.

The filesystem stores and exposes memory as files. The agent application is
responsible for selecting relevant material and assembling the LLM's context.
Semantic retrieval and context assembly still need to be designed and integrated;
MoobFS does not currently provide them or extend a model's context window.

## Implemented today: M1 (read-only sharing) and phase 2a of the memory layer

Since [ADR-0007](docs/decisions/0007-stacked-memory-layer.md) MoobFS is being built as a
**native stacked layer over a mature filesystem**. Phases 2a and 2b deliver a complete pass-through module
(`layer/moobfs_layer.ko`, `mount -t moobfs_layer -o lower=<dir>[,nolinks]`) validated in the test VM
over ext4, btrfs and XFS with `fsx`, including delegated reflink and dedupe; it adds no memory
features yet. The M1 experiment below remains as the
validated proof of native shared reads and runs as a regression in the same VM.

### The read-only M1 milestone

The first milestone validates a storage primitive for that goal: separate files
can reference the same immutable bytes. An offline tool creates the image, and a
native Linux kernel module mounts it and serves ordinary file reads through the
VFS, without a FUSE process. The fixture below demonstrates physical sharing,
not a complete agent memory workflow.

```text
alpha.txt  →  inode 2  ─┐
                       ├─→  object 1  →  one physical payload
beta.txt   →  inode 3  ─┘
```

**Status: M1 works in an isolated Linux VM. Experimental, read-only, no stable
release.** This demonstrates physical content sharing. It does not yet implement
agent memory retrieval, semantic search, writes, snapshots or transactions, and
there is no performance claim against FUSE or established filesystems.

## Try the image tools

Requires a C11 compiler, Make and Python 3 on Linux or macOS. Run from the repository root:

```sh
git clone https://github.com/studiomoob/llmoobfs.git
cd llmoobfs
make test
printf 'A memory shared by two files.\n' > build/memory.txt
build/host/mkfs.moobfs build/demo.img alpha.txt=build/memory.txt beta.txt=build/memory.txt
build/host/fsck.moobfs build/demo.img
```

The JSON report shows two inode numbers referencing the same object and physical
offset. The creator refuses to overwrite an existing image. `fsck.moobfs` checks
images; it does not repair them.

## Run the native Linux demonstration

With Docker running, use:

```sh
make vm-test
```

This builds Linux tools and the module, then boots a dedicated QEMU x86-64 VM using
Debian kernel **6.12.111+deb13-amd64**. No privileged container or host module loading
is required. On Apple Silicon it uses CPU emulation; the first build downloads
kernel packages and takes longer. See [reproduction instructions](docs/testing.md).

The successful test ends with:

```text
SHARED: inode=2/3 physical=12288 length=16384
MOUNTED_READ_TESTS_PASS
WRITABLE_MOUNT_REJECTED
MOOBFS_VM_PASS: shared extents, independent inodes, reads, readonly, remount, 15 malformed images
LAYER_LOWER_PASS: ext4 reflink=no
LAYER_LOWER_PASS: btrfs reflink=yes
LAYER_LOWER_PASS: xfs reflink=yes
MOOBFS_LAYER_PASS: pass-through over ext4, btrfs and xfs; layer tests; fsx 9000 ops each; reflink on btrfs/xfs; nolinks; unload
```

The fixture contains 12,562 logical bytes referenced by each file, stored once.
FIEMAP reports the same 16,384-byte allocation and the shared flag for both inodes.
Metadata and alignment overhead still count toward total image size.

## What is implemented

| Component | M1 behavior |
| --- | --- |
| Native module | VFS mounting, flat directory, buffered reads via iomap, FIEMAP |
| Image creator | Deterministic offline creation, exact-byte deduplication, no overwrites |
| Verifier | Structural checks, bounds, CRC32C metadata, SHA-256 payload checks |
| Limits | 128 files/objects, 64 KiB per object, printable ASCII names up to 63 bytes |
| Validation | 9 tool tests, sanitizers for tools, actual QEMU mounts and 15 malformed images |
| Memory layer (phases 2a–2b) | Stacked pass-through over ext4, btrfs and XFS: files, directories, rename, links, xattr, mmap, fsync, delegated reflink/dedupe; no typed attributes or index yet |

Images are immutable while mounted. Files have independent inodes and link count
1; this is not hard linking. The module validates all content at mount, which
adds a full scan. Disk sharing does not guarantee page-cache sharing.

Kernel APIs are version-specific: the supported **test target** is pinned, not a
promise of compatibility with every Linux version. Use a disposable VM for module
development. KASAN/lockdep kernel validation and performance benchmarks are pending.

## Documentation and project memory

- [Implemented disk format](docs/format-m1.md) and [testing guide](docs/testing.md).
- [M2 specification](docs/spec-m2.md) (proposal, in Portuguese): a native stacked memory layer over a
  mature filesystem ([ADR-0007](docs/decisions/0007-stacked-memory-layer.md)), reusing
  [kernel infrastructure](docs/kernel-infrastructure.md) instead of rebuilding it.
- [Original full MVP proposal](docs/native-mvp.md), in Portuguese; its write interface is superseded by ADR-0007.
- [Architecture decisions](docs/decisions/), [technical journal](docs/history/) and
  [free-software ecosystem survey](docs/ecosystem.md).
- [AI-assisted development rules](docs/ai-workflow.md), shared through `AGENTS.md`, `CLAUDE.md` and Cursor rules.
- [Changelog](CHANGELOG.md), [governance](GOVERNANCE.md), [conduct](CODE_OF_CONDUCT.md) and [security](SECURITY.md).

Git records exact changes; journals record concise rationale, experiments and
results. Future work must distinguish proposals from implemented and tested behavior.

## Contribute

Bug reports, format review, kernel review and reproducibility improvements are
welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md). AI-assisted contributions are welcome
with human review and accurate test evidence. Do not put private vulnerability
details in public issues; use the [security contact](SECURITY.md).

## License

The entire project is licensed under **GPL-2.0-only**. See [LICENSE](LICENSE).
