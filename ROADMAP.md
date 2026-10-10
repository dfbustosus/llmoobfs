# Roadmap

A sequence of research milestones, not a delivery schedule. Items are grouped by
their state of evidence. "Done, with evidence" means a journal entry records the
tests that actually ran, on which kernel and lower filesystem; it never means
production readiness. Nothing here is a compatibility commitment.

## Done, with evidence

| Item | What was tested | What was not | Evidence |
| --- | --- | --- | --- |
| M0 — Project memory | Versioned journal, ADRs, shared AI instructions, Git history, GitHub replica | — | [0001](docs/history/0001-origin.md), [0002](docs/history/0002-native-read-milestone.md), [0003](docs/history/0003-open-source-github.md) |
| M1 — Native shared reads (read-only image) | Image creation, mount in QEMU, distinct inodes sharing physical bytes (FIEMAP), reads, read-only enforcement, 15 malformed images, remount; kept as a regression in every `make vm-test` | Writes, other kernels, KASAN on the Debian kernel | [0002](docs/history/0002-native-read-milestone.md), [format](docs/format-m1.md), [ADR-0003](docs/decisions/0003-readonly-milestone.md) |
| M1 hardening | Frozen as a completed experiment by [ADR-0007](docs/decisions/0007-stacked-memory-layer.md) | Its original exit criteria (fuzzing of the image parser, lifecycle tests) were not completed before freezing | [0010](docs/history/0010-adr-0007-stacked-layer.md) |
| M2 phase 2a — Minimal stacked pass-through | `moobfs_layer.ko` over ext4 in the VM; pass-through acceptance test; 23,000 `fsx` operations | btrfs, XFS | [0011](docs/history/0011-phase-2a-stacked-passthrough.md), [spec](docs/spec-m2.md) |
| M2 phase 2b — Full pass-through | Rename, links, symlinks, `nolinks`, opaque xattr, delegated reflink/dedupe/`copy_file_range`, FIEMAP over ext4, btrfs and XFS | xfstests | [0012](docs/history/0012-phase-2b-full-passthrough.md) |
| M2 phase 2c — Typed attributes | `user.moob.*` validated in the kernel (shared `common/attr.c`), `moobfs-attr`, `moobfs-check`, host unit tests, libFuzzer target | Fuzzing beyond 30 s per CI run | [0013](docs/history/0013-phase-2c-typed-attributes.md) |
| M2 phase 2d — In-memory index and `QUERY` | Index built at mount and kept current; `QUERY`/`GET_INFO` (UAPI v1); kernel answers identical to the `moobfs-index` oracle over the three lower filesystems, after rename, unlink, removexattr and remount; `noindex`, `index_budget` | Budget exhaustion on large trees; persistence | [0014](docs/history/0014-phase-2d-index-query.md) |
| M2 phase 2e — Acceptance with real clients | git, SQLite WAL with concurrent reader (six variants), own memory client, power-loss replay with dm-log-writes, I/O errors with dm-flakey; two layer bugs found and fixed (stale lower dentry losing WAL frames; `set_nlink` race) | Acceptance over btrfs and XFS; full xfstests | [0015](docs/history/0015-phase-2e-investigation.md) |
| KASAN/UBSAN/lockdep harness | Upstream 6.12.112 built with `vm/kasan.config` under virtme-ng, same `vm/suite.sh`; weekly and on demand; a third defect found (uninitialized `rwsem`/xarray on early mount failure) | KMSAN, KCSAN, KFENCE | [0016](docs/history/0016-kasan-lockdep-harness.md) |
| M3 phase 3b — POSIX authorization in `QUERY` | Per-candidate authorization with the caller's credentials; two users, two namespaces, counts and chunking exact; attribute changes never grant; chmod and POSIX ACLs honoured immediately and across remount; idmapped mounts refused (`EINVAL`), user-namespace mounts refused (`EPERM`); 5-second race of chmod/rename/unlink against `QUERY`; passed in the VM and under KASAN on 6.12.112 | Timing channels; M3 phase 3a identity convention (only the `moobfs-check` duplicate detection exists); phase 3c | [0020](docs/history/0020-phase-3b-query-authorization.md), [spec-m3](docs/spec-m3.md) §3 |

## Planned

No dates. Each item starts with an issue or ADR and ends with a journal entry.

| Item | Scope | Reference |
| --- | --- | --- |
| Remaining M3 | Phase 3a: identity and provenance convention documented end to end; phase 3c: revisit batch operations and the change cursor with M4 numbers | [spec-m3](docs/spec-m3.md) §2, §4, §5, §6 |
| M4 — Benchmark harness | `bench/` per the harness specification: deterministic dataset generator, SQLite baselines, equivalence check against the oracle, latency collection, report. Nothing exists yet; no number is published before a VM run with evidence | [benchmark-plan](docs/benchmark-plan.md), [benchmark-harness](docs/benchmark-harness.md) |
| Community actions 15–18 | Review requests to independent reviewers (after S1), threat-model ADR (S1), isolated execution of a single suite step (`MOOB_ONLY`), KASAN as a required check for `layer/` PRs | [community-plan](docs/community-plan.md) §8, §11 |
| Security roadmap, near term | S1 threat and credential model (ADR), S5 lint and static analysis in CI, S6 disclosure process (`SECURITY.md` revised in this round), S2 fstests, S4 hostile lower tree | [security-roadmap](docs/security-roadmap.md) |

## Depends on validation

These are deliberately **not** scheduled: each is conditioned on M4 results or
on the security roadmap, and a negative result may cancel it.

| Item | Condition | Reference |
| --- | --- | --- |
| Index persistence (surviving remount) | M4 must show that rebuild time or memory at the target scale justifies an on-disk structure, and a format and compatibility ADR must exist | [spec-m3](docs/spec-m3.md) §1, [value-assessment](docs/value-assessment.md) §9 |
| `QUERY_BATCH` and `READ_BATCH` | M4 must measure the per-call cost they would remove; contract in spec-m3 §4 is a proposal | [spec-m3](docs/spec-m3.md) §4 |
| Change cursor ("changes since revision") | Decision deferred; needs a client with a measured benefit and answers to the open questions in spec-m3 §5 | [spec-m3](docs/spec-m3.md) §5 |
| Own filesystem as a lower (the pre-ADR-0007 path) | Returns only as an alternative implementation of the same interface, and only with evidence that the mature lower filesystems are the bottleneck | [ADR-0007](docs/decisions/0007-stacked-memory-layer.md) |
| Public release (DKMS, `.deb`, kernel matrix) | Stage 2 (technical preview) needs S1, S4, S5, S6, S2 results and S10; stage 3 (public) needs M4 published and S1–S6, S8–S10 done | [distribution-strategy](docs/distribution-strategy.md) §8 |
| Security roadmap, remaining items | S3 syzkaller, S7 continuous fuzzing, S8 runtime hardening, S9 resource limits and denial of service, S10 module signing and distribution, S11 M3/M4 surfaces | [security-roadmap](docs/security-roadmap.md) |

The [original architecture](docs/native-mvp.md) explores the full transactional
MVP; the [M1 format](docs/format-m1.md) intentionally does not implement it, and
ADR-0007 moved the project to a stacked layer. Any extension starts with an ADR,
a format or UAPI compatibility decision and measurable tests.
