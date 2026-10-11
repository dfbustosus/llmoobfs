# Changelog

## Unreleased

- `QUERY`: results are copied to the caller one at a time instead of through a limit-sized
  kernel array, and the scan of the first term starts at the first candidate and stops after
  the last one (journal 0027). UAPI unchanged. New suite block with one attribute holding
  several types.

## v0.1.0-experimental — 2026-10-10

First pre-release tag: a source snapshot of M1, M2 (phases 2a–2e), M3 phase 3b and the M4
harness, with the community and SEO files. No binaries, no packaging, no support, no
performance numbers. Everything below was previously listed as unreleased.

- `bench/`: first slice of the M4 harness: deterministic manifest generator, loader for W1-W4
  with per-call latency records, report with percentiles and dispersion, `make bench-smoke` in
  CI. Correctness only; no performance numbers. `bench/sqlbase.sh` (SQLite baseline schema,
  composite indexes, plan gate) and `bench/equiv.sh` (kernel, SQLite and oracle must agree)
  run in the VM suite over the layer. W5, W6/W7, W8, `bench/run.sh` (campaign orchestrator)
  and `bench/sqlclient.c` (W3 through libsqlite3 for the baselines) complete the harness.
- SEO/GEO files (plan `docs/seo-geo-plan.md`, journal 0024): `README.md` reorganized into the
  planned section order (banner and "At a glance" unchanged; new "How it works", "What is
  implemented, with evidence", "Limitations", comparison, "Documentation map", "Prior work and
  positioning"), `README.pt-BR.md` mirrored; new English pages `docs/architecture-overview.md`
  (layer internals with source citations), `docs/when-not-to-use.md` (comparison with plain
  files, SQLite, FUSE systems and memory frameworks; what M4 must show) and `docs/faq.md`
  (twenty sourced answers); site updated to the current state (M1, M2 phases 2a–2e and M3
  phase 3b done with evidence, M4 pending, nothing released) with the new title and
  description, the experimental banner, `rel=canonical` and Open Graph tags on every page,
  `site/faq.html`, `site/robots.txt` and `site/sitemap.xml`; stale statements fixed (`plan.html`
  said M2 was not implemented and that `QUERY` returned file handles; `guide.html` said the QEMU
  timeout was 180 s). Documentation only; no code, test or CI changes; no release or tag.
- Community readiness (plan `docs/community-plan.md`, actions 1–14, journal 0024): Contributor
  Covenant 2.1 as `CODE_OF_CONDUCT.md`; `SECURITY.md` with scope, what to include, response
  targets (acknowledge in 7 days, assessment in 30, updates every 30), coordinated disclosure,
  CVE via GitHub, credit, no bounty; `CONTRIBUTING.md` rewritten with the experimental stage,
  language policy, DCO (`Signed-off-by` required), commit style, history and scope rules, testing
  requirements, review rule (one maintainer plus one recorded external review for `layer/`),
  no performance claims without evidence; PR template checklist; issue templates revised
  (`bug_report`, `proposal`, `config`) and added (`test_failure`, `review_request`);
  `docs/labels.md`; `docs/development.md` (new) and `docs/testing.md` restructured with a marker
  table; experimental-status banner and "At a glance" in both READMEs; `ROADMAP.md` in three
  evidence states; CI `Lint` job (shellcheck, SPDX headers, DCO check on pull requests); KASAN
  workflow now also runs on pull requests touching `layer/` or `vm/`. Documentation and
  configuration only; no code or test changes.
- `docs/project-review-2026-10-10.md`: implementation and roadmap review, static QUERY
  correctness findings, configurations requiring Linux reproduction, and a proposed
  focused benchmark and agent-memory demonstration. Analysis only; no code fixes.
- `docs/research-originality.md`: prior-art and originality audit (SFS, BFS, YoloFS, SMFS and
  others; patents described, no legal opinion); claims classified as proven, probable or
  unverified, with a list of statements to avoid in public texts.
- `docs/distribution-strategy.md`: proposed deployment model (Core, Workspace, Integration),
  DKMS packaging and kernel matrix, container bind-mount limits, incremental release plan;
  nothing implemented.
- `docs/value-assessment.md`: critical assessment of real usefulness, value proposition and
  maturity as an adoption criterion; today's recommendation for production stays SQLite or
  plain files; code findings on index term selection and budget accounting recorded.
- `docs/spec-m3.md`: M3 specification proposal (identity, isolation, provenance, change cursor,
  batch reads).
- `docs/benchmark-plan.md`: M4 benchmark methodology, baselines and the real clients used as
  workloads; M3 scope gains agent/namespace isolation, batch reads and a change cursor.
- `docs/security-roadmap.md`: proposed list of security and robustness techniques for the layer
  (threat model, fstests, syzkaller, hostile lower, static analysis, disclosure policy,
  hardening, resource limits, module signing), with current state and sources; linked from
  the roadmap. Nothing implemented yet.
- Phase 2e: acceptance with git, SQLite (WAL, concurrent processes) and an own memory client;
  power-loss replay with dm-log-writes and I/O error injection with dm-flakey. Two layer bugs
  found and fixed: stale lower dentry after unlink+recreate (lost SQLite WAL frames) and a
  concurrent `set_nlink` race on unlocked attribute copies (kernel warning on CI).
- Phase 2d: in-memory attribute index built at mount and kept current on writes; `QUERY` and
  `GET_INFO` ioctls on the mount root (UAPI v1); `moobfs-index` user-space oracle; kernel answers
  verified identical to the oracle over ext4, btrfs and XFS.
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
