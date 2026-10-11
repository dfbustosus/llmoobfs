#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# timing-v2.sh: timing-channel test of docs/spec-query-v2.md (§7, test 4), the domain-scoped
# counterpart of journal 0030 §3. Uid A owns 10 memories "kind = tc" in domain dA (0700);
# uid B owns N memories with the same attribute in domain dB (0700). Uid A runs QUERY2 on dA
# --repeat times for each N; the latency must not grow with N. Run as root on a layer mounted
# with domain_depth=1 (the default). The domain directories are created, then the layer is
# remounted so that their chown/chmod does not leave them in fallback mode.
#   timing-v2.sh --layer MOUNT --lower DIR --out DIR [--uid-a 1001] [--uid-b 1002] [--repeat 400] [--sizes "0 1000 5000 20000"]
set -eu
BIN=${BIN:-/opt/moob-test}
layer=""; lower=""; out=""; ua=1001; ub=1002; repeat=400; sizes="0 1000 5000 20000"
while [ $# -gt 0 ]; do
    case $1 in
        --layer) layer=$2; shift 2 ;;
        --lower) lower=$2; shift 2 ;;
        --out) out=$2; shift 2 ;;
        --uid-a) ua=$2; shift 2 ;;
        --uid-b) ub=$2; shift 2 ;;
        --repeat) repeat=$2; shift 2 ;;
        --sizes) sizes=$2; shift 2 ;;
        *) echo "timing-v2.sh: unknown option $1" >&2; exit 64 ;;
    esac
done
[ -n "$layer" ] && [ -n "$lower" ] && [ -n "$out" ] || { echo "usage: timing-v2.sh --layer MOUNT --lower DIR --out DIR [...]" >&2; exit 64; }
[ "$(id -u)" = 0 ] || { echo "timing-v2.sh: run as root" >&2; exit 1; }
"$BIN/query" "$layer" info | grep -q 'domain_depth=1 ' || { echo "timing-v2.sh: layer needs domain_depth=1" >&2; exit 1; }
[ ! -e "$layer/dA" ] && [ ! -e "$layer/dB" ] || { echo "timing-v2.sh: $layer/dA or dB exists; refusing to reuse" >&2; exit 1; }
[ ! -e "$out" ] || { echo "timing-v2.sh: $out exists" >&2; exit 1; }
mkdir -p "$out/rep1"
tag=$(printf '\001tc')
mkdir "$layer/dA" "$layer/dB"
chown "$ua:$ua" "$layer/dA"; chown "$ub:$ub" "$layer/dB"
chmod 0700 "$layer/dA" "$layer/dB"
umount "$layer"
mount -t moobfs_layer -o "lower=$lower" none "$layer"
"$BIN/query" "$layer" info | grep -q 'fallback_domains=0' || { echo "timing-v2.sh: domains in fallback after remount" >&2; exit 1; }
for i in $(seq 1 10); do
    f=$layer/dA/m$i.md; echo "a $i" > "$f"; setfattr -n user.moob.kind -v "$tag" "$f"
    chown "$ua:$ua" "$f"; chmod 0600 "$f"
done
echo "kind eq string tc" > "$out/queries.txt"
have=0
for n in $sizes; do
    while [ $have -lt "$n" ]; do
        have=$((have + 1)); f=$layer/dB/m$have.md
        echo "b $have" > "$f"; setfattr -n user.moob.kind -v "$tag" "$f"
        chown "$ub:$ub" "$f"; chmod 0600 "$f"
    done
    mkdir -p "$out/rep1/N$n"
    "$BIN/bench-load" --workload W3 --target "$layer/dA" --root "$layer/dA" --query2 1 --uid "$ua" \
        --queries "$out/queries.txt" --repeat "$repeat" --out "$out/rep1/N$n/w3"
    "$BIN/query" "$layer" info > "$out/rep1/N$n/get_info.txt"
    echo "timing-v2: N=$n done"
done
python3 "$(dirname "$0")/report.py" --out "$out/report" "$out/rep1" || python3 "$BIN/bench-report.py" --out "$out/report" "$out/rep1"
echo "timing-v2: report in $out/report; remove $layer/dA and $layer/dB by hand when done"
