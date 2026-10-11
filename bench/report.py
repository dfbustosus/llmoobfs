# SPDX-License-Identifier: GPL-2.0-only
"""bench/report: turns raw latency records into percentiles, CSV and JSON.

Usage: report.py --out DIR REP_DIR [REP_DIR ...]

Each REP_DIR is one repetition and contains <system>/<prefix>.<proc>.rec files
written by bench/load (32-byte records, see bench/common.h). The system name is
the sub-directory name (for example A, B, C-ext4). Per repetition, system,
workload and operation the report computes count, errors, p50/p95/p99 of the
duration in microseconds and throughput in operations per second over the span
of the repetition. Across repetitions it reports the median and the p5-p95
range of each metric; a relative spread above 20% marks the row inconclusive
(docs/benchmark-plan.md, "Método"). Standard library only.
"""
import argparse
import csv
import json
import os
import struct
import sys

REC = struct.Struct("=IIQQIi")
OPS = {1: "create", 2: "read", 3: "query", 4: "attr_set", 5: "attr_rm", 6: "mount", 7: "rename", 8: "recreate"}


def percentile(sorted_values, p):
    if not sorted_values:
        return None
    k = (len(sorted_values) - 1) * p / 100.0
    lo, hi = int(k), min(int(k) + 1, len(sorted_values) - 1)
    return sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * (k - lo)


def read_records(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) % REC.size:
        raise SystemExit(f"{path}: size is not a multiple of {REC.size}")
    return [REC.unpack_from(data, i) for i in range(0, len(data), REC.size)]


def summarize_rep(rep_dir):
    """Returns {(system, workload, op): metrics} for one repetition."""
    out = {}
    for system in sorted(os.listdir(rep_dir)):
        sdir = os.path.join(rep_dir, system)
        if not os.path.isdir(sdir):
            continue
        groups = {}
        for name in sorted(os.listdir(sdir)):
            if not name.endswith(".rec"):
                continue
            for wl, op, start, dur, _proc, result in read_records(os.path.join(sdir, name)):
                g = groups.setdefault((wl, op), {"ok": [], "errors": 0, "min": None, "max": None})
                if result < 0:
                    g["errors"] += 1
                    continue
                g["ok"].append(dur / 1000.0)
                end = start + dur
                g["min"] = start if g["min"] is None else min(g["min"], start)
                g["max"] = end if g["max"] is None else max(g["max"], end)
        for (wl, op), g in groups.items():
            vals = sorted(g["ok"])
            span = (g["max"] - g["min"]) / 1e9 if vals and g["max"] > g["min"] else None
            out[(system, f"W{wl}", OPS.get(op, str(op)))] = {
                "count": len(vals),
                "errors": g["errors"],
                "p50_us": percentile(vals, 50),
                "p95_us": percentile(vals, 95),
                "p99_us": percentile(vals, 99),
                "ops_per_s": (len(vals) / span) if span else None,
            }
    return out


def aggregate(reps):
    """Median and p5-p95 spread across repetitions for every metric."""
    keys = sorted(set(k for r in reps for k in r))
    rows = []
    for key in keys:
        row = {"system": key[0], "workload": key[1], "op": key[2], "repetitions": 0}
        inconclusive = False
        for metric in ("count", "errors", "p50_us", "p95_us", "p99_us", "ops_per_s"):
            vals = sorted(r[key][metric] for r in reps if key in r and r[key][metric] is not None)
            row["repetitions"] = max(row["repetitions"], len(vals))
            if not vals:
                row[metric] = None
                continue
            med = percentile(vals, 50)
            lo, hi = percentile(vals, 5), percentile(vals, 95)
            if metric in ("count", "errors"):
                med, lo, hi = int(med), int(lo), int(hi)
            row[metric] = med
            row[metric + "_p5"] = lo
            row[metric + "_p95"] = hi
            if metric in ("p50_us", "p95_us", "p99_us", "ops_per_s") and med and len(vals) > 1:
                if (hi - lo) / med > 0.20:
                    inconclusive = True
        row["inconclusive"] = inconclusive
        rows.append(row)
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("reps", nargs="+")
    args = ap.parse_args()
    reps = [summarize_rep(d) for d in args.reps]
    rows = aggregate(reps)
    if not rows:
        sys.exit("report: no records found")
    os.makedirs(args.out, exist_ok=True)
    fields = ["system", "workload", "op", "repetitions", "count", "errors", "p50_us", "p50_us_p5", "p50_us_p95",
              "p95_us", "p95_us_p5", "p95_us_p95", "p99_us", "p99_us_p5", "p99_us_p95",
              "ops_per_s", "ops_per_s_p5", "ops_per_s_p95", "inconclusive"]
    with open(os.path.join(args.out, "results.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow({k: (f"{v:.3f}" if isinstance(v, float) else v) for k, v in r.items()})
    with open(os.path.join(args.out, "results.json"), "w") as f:
        json.dump({"repetitions": args.reps, "rows": rows}, f, indent=1)
    for r in rows:
        flag = " INCONCLUSIVE" if r["inconclusive"] else ""
        p50 = f"{r['p50_us']:.1f}" if r["p50_us"] is not None else "-"
        p99 = f"{r['p99_us']:.1f}" if r["p99_us"] is not None else "-"
        print(f"{r['system']} {r['workload']} {r['op']}: n={r['count']} err={r['errors']} p50={p50}us p99={p99}us{flag}")


if __name__ == "__main__":
    main()
