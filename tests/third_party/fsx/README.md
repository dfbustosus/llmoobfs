# fsx (vendored from xfstests)

`fsx.c` and `statx.h` are copied unmodified from the xfstests repository
(`ltp/fsx.c`, `src/global.h` replaced by the minimal `global.h` here so the tool
builds standalone with a static glibc). License: GPL-2.0, see the header of
`fsx.c`. The upstream commit and date are recorded in `UPSTREAM`.

`global.h` is ours: it only supplies the includes and the `roundup_64` /
`rounddown_64` helpers that the xfstests build system normally provides.
