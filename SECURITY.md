# Security policy

MoobFS is experimental kernel code. There is no supported stable release and no
production security guarantee. Current development targets the `main` branch.
Use a disposable VM, synthetic fixtures and an immutable backing image.

## Report a vulnerability privately

Email **claudio@studiomoob.com** with a description, affected commit/kernel,
reproduction steps, potential impact and a minimal non-sensitive fixture if needed.
Do not post exploit details, credentials or real memory contents in a public issue.
For large or sensitive material, first arrange a suitable transfer method by email.

We will investigate and coordinate a fix and disclosure when possible. This small
project does not promise a response deadline or operate a bounty program. Private
GitHub vulnerability reporting may also be used when the repository displays that
option; email remains the documented contact.

## Current boundaries

The parser checks image structure and hashes, but checksums are not authentication.
The module validates at mount, not continuously against external backing-device
writes. Neither encryption nor authorization beyond ordinary VFS access is provided.
The existing VM suite is not a complete security audit; KASAN/lockdep, fuzzing and
broader kernel review are still needed.
