# Roadmap

This is a sequence of research milestones, not a delivery schedule.

| Milestone | Status | Exit criteria |
| --- | --- | --- |
| M0 — Project memory | Implemented | Versioned journal, ADRs, shared AI instructions and Git history |
| M1 — Native shared reads | Validated in local VM | Create image; mount; distinct inodes share physical bytes; malformed-image and read-only tests |
| M1 hardening | Planned | KASAN/lockdep VM, fuzzing, parser review, lifecycle/concurrency tests and kernel compatibility policy |
| M2 — Writable POSIX volume | Specified (proposal) | [Spec](docs/spec-m2.md): POSIX writes on jbd2 + iomap, shared extents via `remap_file_range`, typed xattr attributes and `QUERY` ([ADR-0005](docs/decisions/0005-memory-model-bfs-reference.md), [ADR-0006](docs/decisions/0006-m2-posix-writes.md)); xfstests, fsx, dm-log-writes and a real client (git + SQLite + ai-memory) as exit criteria |
| M3 — Revisions and snapshots | Proposed | Stable identities, retained revisions, garbage-collection rules and isolation tests |
| M4 — Agent experiment | Proposed | Integrate a memory workflow and measure utility, space, latency and operational cost against explicit baselines |

The [original architecture](docs/native-mvp.md) explores the full transactional MVP.
The [M1 format](docs/format-m1.md) intentionally does not implement that design yet.
Any extension starts with an ADR, format compatibility decision and measurable tests.
