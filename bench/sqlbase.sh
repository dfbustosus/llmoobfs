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

# One INSERT per manifest line; the ino comes from the file created by bench/load W1.
load_sql() {
    manifest=$1; target=$2
    grep -v '^#' "$manifest" | while IFS="$(printf '\t')" read -r path _size _seed _dup attrs; do
        ino=$(stat -c %i "$target/$path")
        # raw attributes are never indexed and have no column (docs/benchmark-harness.md §4)
        printf 'INSERT INTO memories(ino,path' ; printf '%s\n' "$attrs" | tr ' ' '\n' | awk -F'[=:]' '$2 != "r" { printf ",%s", $1 }'
        printf ') VALUES(%s,%s' "$ino" "'$path'"
        printf '%s\n' "$attrs" | tr ' ' '\n' | awk -F'[=:]' '$2 != "r" {
            if ($2 == "s") printf ",%s", "\x27" $3 "\x27";
            else printf ",%s", $3 }'
        printf ');\n'
    done
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
    echo "SELECT ino FROM memories WHERE $where ORDER BY ino;"
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
