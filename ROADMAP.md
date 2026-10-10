# Roadmap

This is a sequence of research milestones, not a delivery schedule.

| Milestone | Status | Exit criteria |
| --- | --- | --- |
| M0 — Project memory | Implemented | Versioned journal, ADRs, shared AI instructions and Git history |
| M1 — Native shared reads | Validated in local VM | Create image; mount; distinct inodes share physical bytes; malformed-image and read-only tests |
| M1 hardening | Planned | KASAN/lockdep VM, fuzzing, parser review, lifecycle/concurrency tests and kernel compatibility policy |
| M2 — Durable publication | Proposed | Specify writes, failure model and recovery; prove behavior with crash/power-loss tests |
| M3 — Revisions and snapshots | Proposed | Stable identities, retained revisions, garbage-collection rules and isolation tests |
| M4 — Agent experiment | Proposed | Integrate a memory workflow and measure utility, space, latency and operational cost against explicit baselines |

The [original architecture](docs/native-mvp.md) explores the full transactional MVP.
The [M1 format](docs/format-m1.md) intentionally does not implement that design yet.
Any extension starts with an ADR, format compatibility decision and measurable tests.
