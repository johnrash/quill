#!/bin/bash
# tests/launch.sh [files...] -- (re)start the Quill under test and wait until
# its first window accepts messages. Only instances of the binary under test
# are killed, never a Quill that someone is working with.
APP=${QUILL:-/boot/home/Quill/objects.x86_64-cc13-release/Quill}
# keep the tests' zoom, window frames and recent files out of the
# preferences of the Quill people use
export QUILL_SETTINGS=${QUILL_SETTINGS:-Quill test settings}
SIG=${QUILL_SIG:-application/x-vnd.Quill}
for team in $(ps -a 2>/dev/null | awk -v app="$APP" '$1 == app && NF >= 5 {print $(NF-3)}'); do
	kill -9 "$team" 2>/dev/null
done
for i in $(seq 1 40); do
	ps -a 2>/dev/null | awk -v app="$APP" '$1 == app && NF >= 5 {f=1} END {exit f ? 1 : 0}' && break
	sleep 0.1
done
"$APP" "$@" >/tmp/quill-stdout.log 2>&1 &
waitfor -m "$SIG"
for i in 1 2 3 4 5 6 7 8 9 10; do
	if hey "$SIG" count Window 2>/dev/null | grep -q '"result"'; then
		break
	fi
	sleep 0.3
done
sleep 0.5
echo launched
