#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# bench/sqlbase: the SQLite side of baselines A and B (docs/benchmark-harness.md §4).
# Keeps an attribute table equivalent to the layer's index next to the files, with the
# composite indexes of §4.1, and answers the W3 shapes in SQL. Uses the sqlite3 CLI so it
# runs inside the VM initramfs; timing of the SQLite path is not done here.
#
#   sqlbase.sh init  <db>                         create schema and indexes
#   sqlbase.sh load  <db> <manifest> <target-dir> insert one row per memory (ino from stat)
#   sqlbase.sh sql   <query-line>                 print the SQL for one W3 query line
#   sqlbase.sh plan  <db> <query-line>            EXPLAIN QUERY PLAN; exit 3 on a SCAN
#   sqlbase.sh query <db> <query-line>            print matching inos, sorted numerically
# A query line is "<name> <op> <type> <value> [...]" as in bench/queries-w3.txt.
set -eu
SQLITE=${SQLITE:-sqlite3}

schema() {
    cat <<'EOF'
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
CREATE TABLE IF NOT EXISTS memories (
  ino INTEGER PRIMARY KEY, path TEXT UNIQUE NOT NULL,
  namespace TEXT, agent TEXT, memory_id TEXT UNIQUE, kind TEXT, score INTEGER, created INTEGER,
  pinned INTEGER, weight REAL, tag1 TEXT, tag2 TEXT, tag3 TEXT, tag4 TEXT, tag5 TEXT, tag6 TEXT, tag7 TEXT);
CREATE INDEX IF NOT EXISTS ix_kind_score ON memories(kind, score);
CREATE INDEX IF NOT EXISTS ix_ns_agent ON memories(namespace, agent);
CREATE INDEX IF NOT EXISTS ix_ns_kind_created ON memories(namespace, kind, created);
CREATE INDEX IF NOT EXISTS ix_pinned_score ON memories(pinned, score);
CREATE INDEX IF NOT EXISTS ix_tag1_kind_pinned_score ON memories(tag1, kind, pinned, score);
CREATE INDEX IF NOT EXISTS ix_score ON memories(score);
CREATE INDEX IF NOT EXISTS ix_agent ON memories(agent);
CREATE INDEX IF NOT EXISTS ix_created ON memories(created);
CREATE INDEX IF NOT EXISTS ix_weight ON memories(weight);
EOF
}

# One INSERT per manifest line; the ino comes from the file created by bench/load W1. One
# find for all inode numbers and one awk pass, so the load stays linear at 60k memories.
load_sql() {
    manifest=$1; target=$2
    # busybox find (VM initramfs) has no -printf; stat through -exec works with GNU and busybox
    (cd "$target" && find . -type f -exec stat -c '%i %n' {} +) | awk '{ print substr($2, 3) "\t" $1 }' | awk -F'\t' '
        NR == FNR { ino[$1] = $2; next }
        /^#/ { next }
        {
            if (!($1 in ino)) { print "sqlbase: no file for " $1 > "/dev/stderr"; exit 1 }
            cols = "ino,path"; vals = ino[$1] ",\047" $1 "\047"
            n = split($5, a, " ")
            for (i = 1; i <= n; i++) {
                eq = index(a[i], "="); name = substr(a[i], 1, eq - 1)
                type = substr(a[i], eq + 1, 1); v = substr(a[i], eq + 3)
                if (type == "r") continue      # raw is never indexed and has no column
                cols = cols "," name
                vals = vals "," (type == "s" ? "\047" v "\047" : v)
            }
            print "INSERT INTO memories(" cols ") VALUES(" vals ");"
        }' - "$manifest"
}

# SQL predicate for one term. Prefix becomes a binary half-open interval; exists becomes IS NOT NULL.
term_sql() {
    name=$1; op=$2; type=$3; value=$4
    case $type in
        s|string) lit="'$value'" ;;
        b|bool) lit=$([ "$value" = 1 ] || [ "$value" = true ] && echo 1 || echo 0) ;;
        *) lit=$value ;;
    esac
    case $op in
        eq) echo "$name = $lit" ;;
        lt) echo "$name < $lit" ;;
        le) echo "$name <= $lit" ;;
        gt) echo "$name > $lit" ;;
        ge) echo "$name >= $lit" ;;
        prefix)
            # upper bound: last byte incremented; values here are ASCII without 0xFF
            last=$(printf '%s' "$value" | tail -c 1); head=$(printf '%s' "$value" | sed 's/.$//')
            next=$(printf '%s%s' "$head" "$(printf "\\$(printf '%03o' $(( $(printf '%d' "'$last") + 1 )))")")
            echo "$name >= '$value' AND $name < '$next'" ;;
        exists) echo "$name IS NOT NULL" ;;
        *) echo "sqlbase: unknown op $op" >&2; return 1 ;;
    esac
}

query_sql() {
    where=""
    while [ $# -ge 4 ]; do
        t=$(term_sql "$1" "$2" "$3" "$4")
        where="${where:+$where AND }$t"
        shift 4
    done
    [ $# -eq 0 ] && [ -n "$where" ] || { echo "sqlbase: bad query line" >&2; return 1; }
    # ino and path: the same data a QUERY result carries
    echo "SELECT ino, path FROM memories WHERE $where ORDER BY ino;"
}

cmd=${1:-}; shift || true
case $cmd in
    init) schema | "$SQLITE" "$1" ;;
    load) db=$1; { echo "BEGIN;"; load_sql "$2" "$3"; echo "COMMIT; ANALYZE;"; } | "$SQLITE" "$db" ;;
    sql) query_sql "$@" ;;
    plan)
        # exit 0: SEARCH with an index; exit 3: SCAN (an exception the run must justify, §4.1);
        # exit 2: neither (unexpected plan)
        db=$1; shift
        plan=$("$SQLITE" "$db" "EXPLAIN QUERY PLAN $(query_sql "$@")")
        echo "$plan"
        echo "$plan" | grep -q 'SCAN' && exit 3
        echo "$plan" | grep -q 'SEARCH' || exit 2 ;;
    query) db=$1; shift; "$SQLITE" "$db" "$(query_sql "$@")" ;;
    *) echo "usage: sqlbase.sh init|load|sql|plan|query ..." >&2; exit 64 ;;
esac
