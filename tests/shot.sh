#!/bin/bash
# tests/shot.sh <out.png> [delay seconds] -- screenshot of the whole screen
# after the screen saver is stopped.
OUT=${1:-/tmp/quill.png}
DELAY=${2:-1}
kill $(roster 2>/dev/null | grep -i screen_blanker | awk '{print $1}') 2>/dev/null
sleep "$DELAY"
screenshot -s -f png "$OUT" >/dev/null 2>&1
echo "$OUT"
