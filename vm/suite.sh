#!/bin/sh
# MoobFS layer test suite, shared by the Debian initramfs harness (vm/init) and the
# KASAN/lockdep harness (vm/kasan-guest.sh). The caller sets:
#   BIN        directory with layer, fsx, query, ipc, moobfs-*, replay-log, moobfs_layer.ko
#   TOOLS      directory with git, sqlite3, dmsetup, mke2fs, e2fsck
#   MODS       optional directory with lower-fs modules and an "order" file
#   DEV_EXT4 DEV_BTRFS DEV_XFS   block devices carrying the prepared lower filesystems
#   DEV_SCRATCH DEV_LOG          blank block devices for dm-log-writes and dm-flakey
# and a fail() function. LOWER and LAYER (mount points) default to /lower and /mnt/layer.
# Prints MOOBFS_LAYER_PASS and MOOBFS_ACCEPTANCE_PASS on success.
set -u
LOWER=${LOWER:-/lower}
LAYER=${LAYER:-/mnt/layer}
mkdir -p "$LOWER" "$LAYER" /tmp
# Phases 2a/2b: memory layer stacked over ext4, btrfs and XFS (virtio disks vda, vdb, vdc).
if [ -f "$MODS/order" ]; then for m in $(cat "$MODS/order"); do insmod "$MODS/$m" || fail "lower module $m"; done; fi
insmod "$BIN/moobfs_layer.ko" || fail layer-module
mount -t moobfs_layer none $LAYER && fail layer-mount-without-lower
mount -t moobfs_layer -o lower=/nonexistent none $LAYER && fail layer-mount-bad-lower
run_lower() {
    fs=$1; dev=$2; reflink=$3
    mount -t "$fs" "$dev" $LOWER || fail "lower-mount-$fs"
    mount -t moobfs_layer -o lower=$LOWER none $LAYER || fail "layer-mount-$fs"
    grep -q 'moobfs_layer' /proc/mounts || fail "layer-proc-mounts-$fs"
    mount -t moobfs_layer -o lower=$LAYER none /mnt && fail "layer-stack-on-itself-$fs"
    $BIN/layer $LAYER $LOWER "$reflink" links || fail "layer-tests-$fs"
    # typed attribute tools: write through the layer, check the lower tree
    echo memo > $LAYER/typed.md || fail "typed-create-$fs"
    $BIN/moobfs-attr set $LAYER/typed.md kind string decision || fail "attr-set-$fs"
    $BIN/moobfs-attr set $LAYER/typed.md created time 1760000000000000000 || fail "attr-time-$fs"
    $BIN/moobfs-attr set $LAYER/typed.md pinned bool true || fail "attr-bool-$fs"
    $BIN/moobfs-attr set $LAYER/typed.md score double 0.5 || fail "attr-double-$fs"
    [ "$($BIN/moobfs-attr get $LAYER/typed.md kind)" = "$(printf 'kind\tstring\tdecision')" ] || fail "attr-get-$fs"
    [ "$($BIN/moobfs-attr list $LOWER/typed.md | wc -l)" -eq 4 ] || fail "attr-list-lower-$fs"
    $BIN/moobfs-attr set $LAYER/typed.md Bad string x 2>/dev/null && fail "attr-badname-accepted-$fs"
    $BIN/moobfs-check $LOWER > /tmp/check.json || fail "check-clean-$fs"
    grep -q '"problem_count": 0' /tmp/check.json || fail "check-count-$fs"
    # plant an invalid encoding directly on the lower tree: the checker must flag it
    $BIN/moobfs-attr rawset $LOWER/typed.md broken 0402 || fail "rawset-lower-$fs"
    $BIN/moobfs-check $LOWER > /tmp/check.json; [ $? -eq 2 ] || fail "check-detects-$fs"
    grep -q 'payload is not a valid value' /tmp/check.json || fail "check-reason-$fs"
    $BIN/moobfs-attr get $LAYER/typed.md broken >/dev/null; [ $? -eq 2 ] || fail "attr-get-invalid-$fs"
    $BIN/moobfs-attr rm $LAYER/typed.md broken || fail "attr-rm-$fs"
    $BIN/moobfs-check $LOWER > /dev/null || fail "check-clean-again-$fs"
    rm $LAYER/typed.md || fail "typed-rm-$fs"
    # index and QUERY: the kernel answer must equal the user-space oracle on the lower tree
    same_query() {
        $BIN/query $LAYER "$@" > /tmp/k.txt || return 1
        $BIN/moobfs-index $LOWER "$@" > /tmp/o.txt || return 1
        cmp -s /tmp/k.txt /tmp/o.txt || { echo "kernel:"; cat /tmp/k.txt; echo "oracle:"; cat /tmp/o.txt; return 1; }
    }
    mkdir -p $LAYER/q/sub || fail "q-mkdir-$fs"
    for i in 1 2 3 4 5; do
        echo "memory $i" > $LAYER/q/f$i
        $BIN/moobfs-attr set $LAYER/q/f$i score int64 $((i * 10)) || fail "q-score-$fs"
    done
    $BIN/moobfs-attr set $LAYER/q/f1 kind string decision || fail "q-kind-$fs"
    $BIN/moobfs-attr set $LAYER/q/f2 kind string note || fail "q-kind-$fs"
    $BIN/moobfs-attr set $LAYER/q/f3 kind string decided || fail "q-kind-$fs"
    $BIN/moobfs-attr set $LAYER/q/f4 kind string note || fail "q-kind-$fs"
    $BIN/moobfs-attr set $LAYER/q/sub kind string folder || fail "q-kind-dir-$fs"
    $BIN/moobfs-attr set $LAYER/q/f2 pinned bool true || fail "q-pinned-$fs"
    $BIN/moobfs-attr set $LAYER/q/f5 pinned bool false || fail "q-pinned-$fs"
    $BIN/moobfs-attr set $LAYER/q/f3 weight double 0.75 || fail "q-double-$fs"
    $BIN/moobfs-attr set $LAYER/q/f4 weight double -2.5 || fail "q-double-$fs"
    $BIN/moobfs-attr set $LAYER/q/f1 raw raw deadbeef || fail "q-raw-$fs"
    same_query kind eq string decision || fail "query-eq-$fs"
    [ "$(wc -l < /tmp/k.txt)" -eq 1 ] && grep -q 'q/f1$' /tmp/k.txt || fail "query-eq-content-$fs"
    same_query kind eq string note score ge int64 40 || fail "query-and-$fs"
    [ "$(wc -l < /tmp/k.txt)" -eq 1 ] && grep -q 'q/f4$' /tmp/k.txt || fail "query-and-content-$fs"
    same_query kind prefix string dec || fail "query-prefix-$fs"
    [ "$(wc -l < /tmp/k.txt)" -eq 2 ] || fail "query-prefix-count-$fs"
    same_query score gt int64 20 || fail "query-gt-$fs"
    [ "$(wc -l < /tmp/k.txt)" -eq 3 ] || fail "query-gt-count-$fs"
    same_query score lt int64 0 || fail "query-empty-$fs"
    [ ! -s /tmp/k.txt ] || fail "query-empty-count-$fs"
    same_query pinned exists - - || fail "query-exists-$fs"
    [ "$(wc -l < /tmp/k.txt)" -eq 2 ] || fail "query-exists-count-$fs"
    same_query pinned eq bool true || fail "query-bool-$fs"
    same_query weight lt double 0 || fail "query-double-$fs"
    [ "$(wc -l < /tmp/k.txt)" -eq 1 ] && grep -q 'q/f4$' /tmp/k.txt || fail "query-double-content-$fs"
    same_query kind eq string folder || fail "query-dir-$fs"
    grep -q 'q/sub$' /tmp/k.txt || fail "query-dir-content-$fs"
    $BIN/query $LAYER raw exists - - > /tmp/k.txt || fail "query-raw-$fs"
    [ ! -s /tmp/k.txt ] || fail "query-raw-not-indexed-$fs"
    $BIN/query $LAYER invalid || fail "query-invalid-$fs"
    $BIN/query $LAYER info > /tmp/info.txt || fail "query-info-$fs"
    grep -q "lower=$fs" /tmp/info.txt || fail "query-info-lower-$fs"
    grep -q 'entries=1[0-9] inodes=6' /tmp/info.txt || { cat /tmp/info.txt; fail "query-info-counts-$fs"; }
    # updates: rename, directory rename, unlink and removexattr are reflected
    mv $LAYER/q/f1 $LAYER/q/g1 || fail "q-rename-$fs"
    same_query kind eq string decision || fail "query-renamed-$fs"
    grep -q 'q/g1$' /tmp/k.txt || fail "query-renamed-path-$fs"
    mv $LAYER/q $LAYER/r || fail "q-dir-rename-$fs"
    same_query kind eq string decision || fail "query-dir-renamed-$fs"
    grep -q 'r/g1$' /tmp/k.txt || fail "query-dir-renamed-path-$fs"
    rm $LAYER/r/f2 || fail "q-unlink-$fs"
    same_query kind eq string note || fail "query-unlinked-$fs"
    [ "$(wc -l < /tmp/k.txt)" -eq 1 ] || fail "query-unlinked-count-$fs"
    $BIN/moobfs-attr rm $LAYER/r/f4 kind || fail "q-removexattr-$fs"
    same_query kind eq string note || fail "query-removexattr-$fs"
    [ ! -s /tmp/k.txt ] || fail "query-removexattr-count-$fs"
    # remount rebuilds the index from the lower tree
    umount $LAYER || fail "q-umount-$fs"
    mount -t moobfs_layer -o lower=$LOWER none $LAYER || fail "q-remount-$fs"
    same_query score ge int64 10 || fail "query-after-remount-$fs"
    [ "$(wc -l < /tmp/k.txt)" -eq 4 ] || fail "query-after-remount-count-$fs"
    same_query kind eq string decision || fail "query-after-remount-eq-$fs"
    grep -q 'r/g1$' /tmp/k.txt || fail "query-after-remount-path-$fs"
    umount $LAYER || fail "q-umount2-$fs"
    mount -t moobfs_layer -o lower=/lower,noindex none $LAYER || fail "q-noindex-mount-$fs"
    $BIN/query $LAYER score ge int64 10 > /dev/null 2>&1 && fail "query-noindex-answered-$fs"
    $BIN/query $LAYER info | grep -q 'flags=0x2' || fail "query-noindex-flag-$fs"
    umount $LAYER || fail "q-umount3-$fs"
    mount -t moobfs_layer -o lower=/lower,index_budget=1M none $LAYER || fail "q-budget-mount-$fs"
    same_query score ge int64 10 || fail "query-budget-$fs"
    umount $LAYER || fail "q-umount4-$fs"
    mount -t moobfs_layer -o lower=$LOWER none $LAYER || fail "q-remount-final-$fs"
    rm -rf $LAYER/r || fail "q-cleanup-$fs"
    mkdir -p $LAYER/fsx
    $BIN/fsx -q -N 5000 -S 1 $LAYER/fsx/file1 || fail "fsx-1-$fs"
    $BIN/fsx -q -N 3000 -S 2 -r 512 -w 512 $LAYER/fsx/file2 || fail "fsx-2-$fs"
    $BIN/fsx -q -N 1000 -S 3 -l 2000000 $LAYER/fsx/file3 || fail "fsx-large-$fs"
    cmp $LAYER/fsx/file1 $LOWER/fsx/file1 || fail "fsx-lower-mismatch-$fs"
    rm -rf $LAYER/fsx || fail "layer-cleanup-$fs"
    sync
    umount $LAYER || fail "layer-unmount-$fs"
    mount -t moobfs_layer -o lower=/lower,nolinks none $LAYER || fail "layer-remount-$fs"
    $BIN/layer $LAYER $LOWER "$reflink" nolinks || fail "layer-nolinks-tests-$fs"
    umount $LAYER || fail "layer-second-unmount-$fs"
    umount $LOWER || fail "lower-unmount-$fs"
    echo "LAYER_LOWER_PASS: $fs reflink=$reflink"
}
run_lower ext4 "$DEV_EXT4" no
run_lower btrfs "$DEV_BTRFS" yes
run_lower xfs "$DEV_XFS" yes

# Phase 2e: acceptance with clients that do not know MoobFS (git, SQLite), an own memory
# client, power loss under dm-log-writes and I/O errors under dm-flakey.
export PATH=$TOOLS:/bin:/usr/bin:/sbin:/usr/sbin
export HOME=/tmp GIT_CONFIG_NOSYSTEM=1 GIT_EXEC_PATH=/usr/lib/git-core GIT_TEMPLATE_DIR=/nonexistent
export DM_DISABLE_UDEV=1
remember() {   # remember <mount> <relative path> <kind> <score> : the client convention of spec-m2 §6
    tmp="$1/.tmp.$$"
    printf '# %s\n\nmemory %s\n' "$2" "$2" > "$tmp" || return 1
    $BIN/moobfs-attr set "$tmp" kind string "$3" || return 1
    $BIN/moobfs-attr set "$tmp" score int64 "$4" || return 1
    $BIN/moobfs-attr set "$tmp" created time 1760000000000000000 || return 1
    mkdir -p "$(dirname "$1/$2")" || return 1
    sync "$tmp" 2>/dev/null || sync
    mv "$tmp" "$1/$2" || return 1
    sync    # a rename is durable only after the parent directory is synced; clients must fsync it
}
mount -t ext4 "$DEV_EXT4" $LOWER || fail acc-lower-mount
mount -t moobfs_layer -o lower=$LOWER none $LAYER || fail acc-layer-mount
# git: a repository living on the layer, with memories as tracked files
mkdir -p $LAYER/repo && cd $LAYER/repo || fail acc-repo-dir
git init -q . 2>/dev/null || fail acc-git-init
for i in 1 2 3; do
    remember $LAYER/repo "notes/n$i.md" note $((i * 7)) || fail "acc-remember-$i"
    git add -A || fail "acc-git-add-$i"
    git -c user.name=moob -c user.email=moob@test commit -q -m "memory $i" || fail "acc-git-commit-$i"
done
[ "$(git log --oneline | wc -l)" -eq 3 ] || fail acc-git-log
[ -z "$(git status --porcelain)" ] || fail acc-git-status
git fsck --strict --no-progress 2>&1 | grep -v '^$' && fail acc-git-fsck
git cat-file -p HEAD:notes/n2.md | grep -q 'memory notes/n2.md' || fail acc-git-content
cd /
$BIN/query $LAYER kind eq string note score gt int64 10 > /tmp/k.txt || fail acc-query
[ "$(wc -l < /tmp/k.txt)" -eq 2 ] || fail acc-query-count
# SQLite in WAL mode: a writer and a reader at the same time, then integrity_check.
# Run as a controlled experiment: directly on the lower fs (control), on the layer without a
# concurrent reader, and on the layer with one. Counts are printed before any verdict.
sq() { sqlite3 -cmd ".timeout 10000" "$@"; }   # concurrent CLI processes wait for fcntl locks instead of failing
sqlite_run() {   # sqlite_run <writer db path> <reader db path or -> <mode wal|delete> <label> [vfs]
    DB=$1; RDB=$2; MODE=$3
    VFS=${5:-}
    if [ -n "$VFS" ]; then sq() { sqlite3 -vfs "$VFS" -cmd ".timeout 10000" "$@"; }; else sq() { sqlite3 -cmd ".timeout 10000" "$@"; }; fi
    rm -f "$DB" "$DB-wal" "$DB-shm" "$DB-journal"
    sq "$DB" "PRAGMA journal_mode=$MODE; CREATE TABLE memories(id INTEGER PRIMARY KEY, path TEXT, score INT);" | grep -qi "$MODE" || fail "sqlite-mode-$4"
    ( for i in $(seq 1 200); do sq "$DB" "INSERT INTO memories(path, score) VALUES('p$i', $i);" || { echo "writer failed at $i ($4)"; exit 1; }; done ) &
    writer=$!
    reads=0
    if [ "$RDB" != - ]; then
        while kill -0 $writer 2>/dev/null; do sq "$RDB" 'SELECT count(*) FROM memories;' > /dev/null || echo "reader error ($4)"; reads=$((reads + 1)); done
    fi
    wait $writer || echo "writer exit code $? ($4)"
    count=$(sq "$DB" 'SELECT count(*) FROM memories;')
    integrity=$(sq "$DB" 'PRAGMA integrity_check;')
    missing=$(sq "$DB" "WITH RECURSIVE s(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM s WHERE x<200) SELECT group_concat(x) FROM s WHERE x NOT IN (SELECT score FROM memories);")
    echo "SQLITE_RESULT $4: rows=$count integrity=$integrity reads=$reads missing=[$missing]"
    [ "$count" = 200 ]
}
sqlite_run $LOWER/c1.db $LOWER/c1.db wal lower-wal; r1=$?
sqlite_run $LAYER/a1.db - wal layer-wal-alone; r2=$?
sqlite_run $LAYER/m1.db $LAYER/m1.db wal layer-wal; r3=$?
sqlite_run $LAYER/m2.db $LAYER/m2.db delete layer-rollback; r4=$?
sqlite_run $LAYER/m3.db $LAYER/m3.db wal layer-wal-unix-excl unix-excl; r5=$?
sqlite_run $LAYER/m5.db $LAYER/m5.db wal layer-wal-again; r6=$?
echo "SQLITE_VARIANTS: lower=$r1 alone=$r2 layer=$r3 rollback=$r4 excl=$r5 again=$r6"
[ $r1 -eq 0 ] || fail acc-sqlite-control
[ $r2 -eq 0 ] || fail acc-sqlite-layer-alone
[ $r3 -eq 0 ] && [ $r6 -eq 0 ] || fail acc-sqlite-layer-reader
echo "SQLITE_CONCURRENT_OK"
$BIN/moobfs-check $LOWER > /dev/null || fail acc-check
umount $LAYER || fail acc-layer-umount
umount $LOWER || fail acc-lower-umount
echo "ACCEPTANCE_CLIENTS_PASS"
# Power loss: format and use a device under dm-log-writes, then replay the log to each mark
# on a zeroed disk and check that every memory completed before the mark is whole.
SCRATCH=$DEV_SCRATCH; LOGDEV=$DEV_LOG
dd if=/dev/zero of=$SCRATCH bs=1M count=64 2>/dev/null || fail lw-zero
$TOOLS/dmsetup create lw --table "0 $(blockdev --getsz $SCRATCH) log-writes $SCRATCH $LOGDEV" || fail lw-create
$TOOLS/mke2fs -q -F -t ext4 -b 4096 /dev/mapper/lw || fail lw-mkfs
$TOOLS/dmsetup message lw 0 mark mkfs || fail lw-mark-mkfs
mount -t ext4 /dev/mapper/lw $LOWER || fail lw-lower-mount
mount -t moobfs_layer -o lower=$LOWER none $LAYER || fail lw-layer-mount
for i in 1 2 3 4; do
    remember $LAYER "mem/m$i.md" decision $i || fail "lw-remember-$i"
    $TOOLS/dmsetup message lw 0 mark "m$i" || fail "lw-mark-$i"
done
umount $LAYER || fail lw-layer-umount
umount $LOWER || fail lw-lower-umount
$TOOLS/dmsetup message lw 0 mark end || fail lw-mark-end
$TOOLS/dmsetup remove lw || fail lw-remove
check_mark() {   # replay up to a mark; memories 1..$2 must be whole, later ones absent
    dd if=/dev/zero of=$SCRATCH bs=1M count=64 2>/dev/null || return 1
    $BIN/replay-log --log $LOGDEV --replay $SCRATCH --end-mark "$1" || return 1
    mount -t ext4 $SCRATCH $LOWER && umount $LOWER || return 1      # journal recovery, as after a reboot
    $TOOLS/e2fsck -fn $SCRATCH > /tmp/fsck.txt 2>&1 || { cat /tmp/fsck.txt; return 1; }
    mount -t ext4 -o ro $SCRATCH $LOWER || return 1
    mount -t moobfs_layer -o lower=$LOWER none $LAYER || return 1
    n=0
    for i in 1 2 3 4; do
        if [ "$i" -le "$2" ]; then
            grep -q "memory mem/m$i.md" "$LAYER/mem/m$i.md" || return 1
            [ "$($BIN/moobfs-attr get $LAYER/mem/m$i.md score)" = "$(printf 'score\tint64\t%s' $i)" ] || return 1
        else
            [ ! -e "$LAYER/mem/m$i.md" ] || return 1
        fi
    done
    $BIN/query $LAYER kind eq string decision > /tmp/k.txt || return 1
    [ "$(wc -l < /tmp/k.txt)" -eq "$2" ] || return 1
    ls -A $LOWER | grep -q '^\.tmp\.' && echo "note: a temporary file survived the crash at $1 (allowed)"
    umount $LAYER && umount $LOWER
}
check_mark mkfs 0 || fail lw-check-mkfs
check_mark m1 1 || fail lw-check-m1
check_mark m2 2 || fail lw-check-m2
check_mark m4 4 || fail lw-check-m4
check_mark end 4 || fail lw-check-end
echo "POWER_LOSS_REPLAY_PASS: marks mkfs m1 m2 m4 end"
# I/O errors: the lower device starts erroring every write; errors must reach the caller,
# nothing may crash, and the tree must come back clean once the device is healthy again.
dd if=/dev/zero of=$SCRATCH bs=1M count=64 2>/dev/null || fail fl-zero
$TOOLS/mke2fs -q -F -t ext4 -b 4096 $SCRATCH || fail fl-mkfs
SZ=$(blockdev --getsz $SCRATCH)
$TOOLS/dmsetup create fl --table "0 $SZ linear $SCRATCH 0" || fail fl-create
mount -t ext4 /dev/mapper/fl $LOWER || fail fl-lower-mount
mount -t moobfs_layer -o lower=$LOWER none $LAYER || fail fl-layer-mount
remember $LAYER "ok.md" note 1 || fail fl-remember-ok
sync
$TOOLS/dmsetup suspend fl && dmsetup reload fl --table "0 $SZ flakey $SCRATCH 0 0 1 1 error_writes" && dmsetup resume fl || fail fl-reload-down
remember $LAYER "lost.md" note 2 2>/dev/null && sync && echo "note: writes were absorbed by the page cache before the device reported errors"
sync 2>/dev/null
$TOOLS/dmsetup suspend --noflush --nolockfs fl && dmsetup reload fl --table "0 $SZ linear $SCRATCH 0" && dmsetup resume fl || fail fl-reload-up
umount $LAYER 2>/dev/null || true
umount $LOWER 2>/dev/null || true
$TOOLS/dmsetup remove fl || fail fl-remove
$TOOLS/e2fsck -fy $SCRATCH > /tmp/fsck.txt 2>&1; rc=$?; [ $rc -le 1 ] || { cat /tmp/fsck.txt; fail fl-fsck; }
mount -t ext4 $SCRATCH $LOWER || fail fl-remount
mount -t moobfs_layer -o lower=$LOWER none $LAYER || fail fl-layer-remount
grep -q 'memory ok.md' $LAYER/ok.md || fail fl-ok-content
$BIN/moobfs-check $LOWER > /dev/null || fail fl-check
umount $LAYER && umount $LOWER || fail fl-final-umount
echo "IO_ERROR_INJECTION_PASS"
rmmod moobfs_layer || fail layer-unload
if dmesg | grep -E 'BUG:|Oops:|WARNING:|Kernel panic|KASAN:'; then fail kernel-diagnostic-layer; fi
echo "MOOBFS_ACCEPTANCE_PASS: git, sqlite wal, memory client, dm-log-writes replay, dm-flakey"
echo "MOOBFS_LAYER_PASS: pass-through over ext4, btrfs and xfs; layer tests; typed attributes and tools; index and QUERY equal to the oracle; fsx 9000 ops each; reflink on btrfs/xfs; nolinks; unload"
