# replay-log (vendored from xfstests)

`log-writes.c`, `log-writes.h` and `replay-log.c` are copied unmodified from
`src/log-writes/` in the xfstests repository (GPL-2.0, see file headers; upstream
commit and date in `UPSTREAM`). `replay-log` replays the write log recorded by the
kernel `dm-log-writes` target up to a named mark, which lets the VM harness check
what a filesystem looks like after a power loss at that point.
