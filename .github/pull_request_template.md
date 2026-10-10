## Problem and resulting behavior

Describe the problem and what changes for users or contributors.

## Scope

- Issue, ADR or journal entry this change implements: <!-- link -->
- Areas touched: <!-- layer/ kernel/ common/ include/ tools/ tests/ vm/ docs/ site/ .github/ -->

## Validation

List the commands actually run, where the logs are, and the environment.
Explicitly mark tests **not** run. Compiling on macOS tests nothing on Linux.

| Command or workflow | Result | Evidence (log path or run link) |
| --- | --- | --- |
| `make test` | | |
| `make vm-test` | | `build/vm/console.log` |
| KASAN workflow (`layer/` or `vm/` changes) | | run URL |

## Decisions, compatibility and limitations

Explain compatibility impacts (image format, attribute encoding, UAPI, kernel
version, lower filesystems) and what remains untested.

## Checklist

- [ ] Scope: the change matches an accepted ADR or a discussed issue (linked above).
- [ ] Tests: `make test` passed on the host; new or extended tests cover every behavior change, including invalid inputs for parsers and ioctls.
- [ ] VM suite: `make vm-test` passed for changes in `layer/`, `kernel/`, `common/`, `include/`, `tools/`, `tests/` or `vm/`, with the log path above.
- [ ] KASAN: for `layer/` or `vm/` changes, the KASAN/lockdep workflow on this PR ended with `MOOBFS_KASAN_PASS` and no `BUG`, `WARNING`, lockdep or UBSAN report (link above).
- [ ] Journal: a new entry in `docs/history/` (or a dated addendum) lists objective, decisions, tests actually executed, limitations and next step; `docs/history/README.md` updated.
- [ ] `CHANGELOG.md` updated under "Unreleased" when users or contributors would notice the change.
- [ ] DCO: every commit carries a `Signed-off-by:` line (`git commit -s`), and I have the right to contribute this under GPL-2.0-only.
- [ ] No unproven claims: no performance numbers without an evidence file, no "tested on Linux" beyond the logs above, no security or production-readiness statements.
- [ ] SPDX `GPL-2.0-only` header on every new source file; no credentials, real agent memories or private vulnerability details.
- [ ] Material AI assistance and the human review performed are summarized below, if applicable.

### UAPI or format change (tick one)

- [ ] This PR does **not** change `include/moobfs_ioctl.h`, the attribute encoding or the M1 image format.
- [ ] It does, and: the specification is updated, the version is bumped where the UAPI requires it, invalid-input tests are added, and the compatibility decision is recorded in an ADR linked above.

## AI assistance and human review

<!-- What tools helped, what a human verified, or "none". -->
