# Roadmap

This is a sequence of research milestones, not a delivery schedule.

| Milestone | Status | Exit criteria |
| --- | --- | --- |
| M0 — Project memory | Implemented | Versioned journal, ADRs, shared AI instructions and Git history |
| M1 — Native shared reads | Validated in local VM | Create image; mount; distinct inodes share physical bytes; malformed-image and read-only tests |
| M1 hardening | Frozen as a completed experiment ([ADR-0007](docs/decisions/0007-stacked-memory-layer.md)); kept as regression | KASAN/lockdep VM, fuzzing, parser review, lifecycle/concurrency tests and kernel compatibility policy |
| M2 — Stacked memory layer | All phases 2a–2e validated in local VM; M2 exit criteria met except full xfstests and KASAN/lockdep | [Spec](docs/spec-m2.md): native stacked filesystem over btrfs/XFS/ext4 ([ADR-0007](docs/decisions/0007-stacked-memory-layer.md)); typed xattr attributes, in-memory index and `QUERY`, reflink delegated to the lower filesystem; xfstests, fsx, dm-log-writes and a real client (git + SQLite + ai-memory) as exit criteria |
| Security and robustness (cross-cutting) | Proposed | [Security roadmap](docs/security-roadmap.md): threat and credential model (ADR), fstests, syzkaller over the ioctls and `setxattr`, lower directory treated as hostile input, static analysis in CI, disclosure policy, runtime hardening, resource limits, module signing |
| M3 — Identity, isolation and revisions | Proposed | Stable memory identity and provenance attributes; agent and namespace isolation expressed through typed attributes and POSIX permissions (no policy logic in the kernel); temporal queries; batch reads and a "changes since revision" cursor on the v1 UAPI; complements git and the lower filesystem's snapshots instead of replacing them |
| M4 — Benchmark | Planned | [Benchmark plan](docs/benchmark-plan.md): ext4/btrfs + SQLite baselines versus the layer's `QUERY`; real clients (ai-memory, mem0, Letta) as workloads; p50/p95/p99, write amplification, mount and index-rebuild time; scales tied to the index budget |

The [original architecture](docs/native-mvp.md) explores the full transactional MVP.
The [M1 format](docs/format-m1.md) intentionally does not implement that design yet.
Any extension starts with an ADR, format compatibility decision and measurable tests.
