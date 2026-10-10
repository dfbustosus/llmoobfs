# Changelog

## Unreleased — 2026-10-10

- Phase 2c: typed `user.moob.*` attributes validated in the kernel (shared encoding in
  `common/attr.c`), `moobfs-attr` and `moobfs-check` tools, host unit tests and a libFuzzer target.
- Phase 2b: rename, links, symlinks (`nolinks` option), opaque xattr pass-through, delegated
  reflink/dedupe/copy_file_range and FIEMAP; validated over ext4, btrfs and XFS in the VM.
- Phase 2a of the memory layer: `layer/moobfs_layer.ko`, a native stacked pass-through
  filesystem over a lower directory, validated in the QEMU VM over ext4 with the pass-through
  acceptance test and 23,000 `fsx` operations (vendored from xfstests).
- ADR-0007 supersedes ADR-0006: MoobFS becomes a native stacked memory layer over a mature
  filesystem; `docs/spec-m2.md` rewritten accordingly, previous version preserved. Proposal only.
- Journal 0009: differentiation analysis and the "build on mature software, innovate only in
  LLM-memory features" principle; open question whether M2 becomes a stacked filesystem.
- ADR-0006 and `docs/spec-m2.md`: M2 specified as a writable POSIX volume on jbd2 + iomap with
  shared extents via `remap_file_range`; `docs/kernel-infrastructure.md` lists the reused kernel
  infrastructure verified against v6.12. Proposal only; no code.
- `docs/research-ai-memory.md`: code-level reading of akitaonrails/ai-memory and its
  relation to MoobFS; M2 now needs POSIX writes to host a real client.
- Site: interactive plan page (`site/plan.html`) showing inputs, processing and outputs per milestone.
- `docs/ecosystem.md`: survey of free software relevant to testing, crash consistency,
  kernel data structures, BFS tooling and agent memory layers.
- ADR-0005: agent memory model (typed attributes, kernel-side attribute indexes,
  ioctl queries) with the Be File System as design reference; no code changes.
- Clarified LLM memory/context as the project goal in both READMEs and the site,
  separating future agent workflows from the implemented read-only M1.
- Native Linux read-only filesystem module, offline image creator and verifier.
- Independent file inodes can share one immutable payload; FIEMAP exposes sharing.
- Versioned M1 disk format with bounded parsing, CRC32C metadata and SHA-256 content.
- Tool tests and isolated QEMU mount tests, including malformed images and read-only enforcement.
- Project history, architecture decisions and shared Codex, Claude and Cursor rules.
- GPL-2.0-only license for the entire project.
- English/Portuguese README, contribution, governance, security and conduct policies.
- Issue/PR templates, pinned CI actions and a static GitHub Pages site with a reproduction guide.

This is an experimental milestone, not a stable release or the complete transactional MVP.
