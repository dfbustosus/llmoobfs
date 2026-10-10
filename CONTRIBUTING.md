# Contributing to MoobFS

MoobFS is an early research project. Issues and pull requests in English or
Portuguese are welcome. Start with the README, relevant architecture decisions
and latest entry in `docs/history/`.

## Before changing code

Open an issue for substantial format or architecture changes so alternatives and
compatibility can be discussed. Small fixes can go directly to a pull request.
Report reproducible bugs with the commit, environment, commands and actual output.
Never upload real agent memories, credentials or private data as fixtures.
For vulnerabilities, follow [SECURITY.md](SECURITY.md).

## Development and validation

- `make test`: portable creator/parser tests, with a C11 compiler, Make and Python 3.
- Run tool changes with AddressSanitizer/UndefinedBehaviorSanitizer as described in
  [testing.md](docs/testing.md).
- `make vm-test`: required for kernel, disk-format and shared-parser changes.
  Develop and load the module only in a disposable VM.
- Documentation-only changes need link/content checks, not fabricated kernel runs.

Use tabs in C indentation, explicit bounds and endian handling, and SPDX
`GPL-2.0-only` identifiers in source files. Keep changes focused. Add meaningful
regression coverage for behavior changes, especially malformed images and lifetime
management. Never disable checks to get a passing build.

## Pull requests and history

Explain the problem, resulting behavior, validation and remaining limitations.
Update `CHANGELOG.md` for relevant changes and the technical journal for project
work. Architectural choices need a sequential ADR that links superseded decisions
without deleting them. Do not rewrite shared history or force-push to main.

A maintainer reviews contributions before merging. Passing CI is necessary for
code changes but does not prove safety or production readiness. The project does
not currently require a CLA or DCO sign-off. Only submit material you have the
right to contribute under GPL-2.0-only; third-party material needs compatible terms
and preserved attribution.

## AI-assisted contributions

AI tools are welcome. Contributors remain responsible for understanding the patch,
license compatibility, correctness and test evidence. Follow [AGENTS.md](AGENTS.md);
Claude and Cursor load the same policy through their project entry points.
Summarize material AI assistance and what a human reviewed in the PR. Do not
invent test results, references or approvals. Record concise technical rationale,
not hidden reasoning, private conversations or secrets.
