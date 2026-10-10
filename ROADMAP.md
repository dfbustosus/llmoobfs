# Roadmap

This is a sequence of research milestones, not a delivery schedule.

| Milestone | Status | Exit criteria |
| --- | --- | --- |
| M0 — Project memory | Implemented | Versioned journal, ADRs, shared AI instructions and Git history |
| M1 — Native shared reads | Validated in local VM | Create image; mount; distinct inodes share physical bytes; malformed-image and read-only tests |
| M1 hardening | Frozen as a completed experiment ([ADR-0007](docs/decisions/0007-stacked-memory-layer.md)); kept as regression | KASAN/lockdep VM, fuzzing, parser review, lifecycle/concurrency tests and kernel compatibility policy |
| M2 — Stacked memory layer | All phases 2a–2e validated in local VM; M2 exit criteria met except full xfstests and KASAN/lockdep | [Spec](docs/spec-m2.md): native stacked filesystem over btrfs/XFS/ext4 ([ADR-0007](docs/decisions/0007-stacked-memory-layer.md)); typed xattr attributes, in-memory index and `QUERY`, reflink delegated to the lower filesystem; xfstests, fsx, dm-log-writes and a real client (git + SQLite + ai-memory) as exit criteria |
| M3 — Revisions and snapshots | Proposed | Stable identities, retained revisions, garbage-collection rules and isolation tests |
| M4 — Agent experiment | Proposed | Integrate a memory workflow and measure utility, space, latency and operational cost against explicit baselines |

The [original architecture](docs/native-mvp.md) explores the full transactional MVP.
The [M1 format](docs/format-m1.md) intentionally does not implement that design yet.
Any extension starts with an ADR, format compatibility decision and measurable tests.
