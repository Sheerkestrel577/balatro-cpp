#!/bin/bash
# backup.sh -- mirror the irreplaceable files to a second device.
#
#   crontab -e  ->  0 * * * * /home/sheerk/naneinf/w4/python/backup.sh
#
# Copies hof.npz + models/ + history/state to $DEST. If $DEST is a
# removable drive, it's mounted only for the copy and unmounted after,
# so a flaky device is exposed for seconds per hour, not continuously.
# A backup target that is missing is a no-op, never an error that could
# wedge the loop.
set -u
SRC=/home/sheerk/naneinf/w4/python
DEST=${1:-/mnt/backup}          # mountpoint or plain directory
LOG=$SRC/logs/backup.log
MOUNTED=0

ts() { date '+%Y-%m-%d %H:%M:%S'; }

# if DEST is an fstab mountpoint that isn't mounted, try to mount it
if grep -qs " $DEST " /etc/fstab && ! mountpoint -q "$DEST"; then
    timeout 20 mount "$DEST" 2>>"$LOG" && MOUNTED=1
fi
if [ ! -d "$DEST" ] || ! touch "$DEST/.naneinf_backup_ok" 2>/dev/null; then
    echo "$(ts) backup target $DEST unavailable, skipping" >>"$LOG"
    exit 0
fi

mkdir -p "$DEST/naneinf"
timeout 300 rsync -a --delete \
    "$SRC/data/hof.npz" "$SRC/models" "$SRC/history.csv" \
    "$SRC/loop_state.json" "$SRC/status.json" \
    "$DEST/naneinf/" 2>>"$LOG" \
  && echo "$(ts) backup ok -> $DEST" >>"$LOG" \
  || echo "$(ts) backup FAILED -> $DEST" >>"$LOG"
sync

if [ "$MOUNTED" = 1 ]; then
    timeout 20 umount "$DEST" 2>>"$LOG"
fi
