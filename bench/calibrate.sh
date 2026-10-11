#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# bench/calibrate: device calibration before a campaign (docs/benchmark-plan.md, "Ferramentas").
# For each mounted directory: 30 s of 4 KiB random writes each followed by fsync, then the
# operations per second and the latency of the fsync itself (p50, p99), which is what bounds
# W1. Raw fio JSON is kept next to the summary.
#
#   calibrate.sh --out DIR NAME=MOUNTED_DIR [NAME=MOUNTED_DIR ...]
set -eu
out=""
[ "${1:-}" = --out ] && { out=$2; shift 2; }
[ -n "$out" ] && [ $# -gt 0 ] || { echo "usage: calibrate.sh --out DIR NAME=DIR ..." >&2; exit 64; }
command -v fio > /dev/null || { echo "calibrate: fio not installed" >&2; exit 1; }
mkdir -p "$out"
: > "$out/calibration.txt"
for spec in "$@"; do
    name=${spec%%=*}; dir=${spec#*=}
    fio --name="cal-$name" --filename="$dir/fio.tmp" --size=256M --rw=randwrite --bs=4k --fsync=1 \
        --runtime=30 --time_based --output-format=json --output="$out/fio-$name.json" > /dev/null
    rm -f "$dir/fio.tmp"
    python3 - "$out/fio-$name.json" "$name" >> "$out/calibration.txt" <<'EOF'
import json, sys
job = json.load(open(sys.argv[1]))["jobs"][0]
w, s = job["write"], job.get("sync", {})
lat = s.get("lat_ns", {}).get("percentile", {})
def us(key):
    return "%.0fus" % (lat[key] / 1000) if key in lat else "n/a"
print("%s: write+fsync/s=%.0f fsync_p50=%s fsync_p99=%s" % (sys.argv[2], w["iops"], us("50.000000"), us("99.000000")))
EOF
done
cat "$out/calibration.txt"
