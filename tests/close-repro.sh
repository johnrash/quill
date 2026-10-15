#!/bin/bash
# Reproduces the hang and prints the backtraces of the test instance.
cd "$(dirname "$0")"
export QUILL=/boot/home/Quill/test-copy/QuillTest QUILL_SIG=application/x-vnd.QuilT
printf '{\\rtf1\\ansi Close test document.\\par}' > /tmp/close-test.rtf
bash launch.sh >/dev/null
"$QUILL" /tmp/close-test.rtf &
sleep 3
team=$(ps -a | awk -v app="$QUILL" '$1 == app && NF >= 5 {print $(NF-3)}')
echo "team $team"
ps -a | awk -v t="$team" '$2 == t {p=1} p && /^---/ {exit} p' | tail -6
threads=$(ps -a | awk -v t="$team" '$2 == t {p=1; next} p && /^---/ {exit} p && $2 ~ /^[0-9]+$/ {print $2}')
cmds="threads\n"
for t in $threads; do cmds="${cmds}bt $t\n"; done
printf "${cmds}quit\n" | timeout 60 Debugger --cli --team "$team" 2>&1 | grep -v "^$" | head -120
