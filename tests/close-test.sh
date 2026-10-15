#!/bin/bash
# Opening documents into a running Quill and closing windows: the empty
# Untitled window is replaced, a window behind another one closes, and
# closing the last window quits.
cd "$(dirname "$0")"
QUILL=${QUILL:-/boot/home/Quill/test-copy/QuillTest}
QUILL_SIG=${QUILL_SIG:-application/x-vnd.QuilT}
export QUILL QUILL_SIG
export QUILL_SETTINGS=${QUILL_SETTINGS:-Quill test settings}
fail=0
check() {
	if [ "$2" == "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got '$2' expected '$3'"; fail=1; fi
}
H() { timeout 5 hey "$QUILL_SIG" "$@" 2>&1; }
count() { H count Window | sed -n 's/.*"result" (B_INT32_TYPE) : \([0-9]*\).*/\1/p'; }
title() { H get Title of Window "$1" | sed -n 's/.*"result" (B_STRING_TYPE) : "\(.*\)"$/\1/p'; }
running() { ps -a | awk -v app="$QUILL" '$1 == app && NF >= 5 {f=1} END {print f ? "yes" : "no"}'; }
waitcount() { for i in $(seq 1 20); do [ "$(count)" == "$1" ] && break; sleep 0.25; done; }

printf '{\\rtf1\\ansi First document.\\par}' > /tmp/close-a.rtf
printf '{\\rtf1\\ansi Second document.\\par}' > /tmp/close-b.rtf

# 1. The empty Untitled window makes room for an opened document.
bash launch.sh >/dev/null
check "starts with Untitled1" "$(title 0)" "Untitled1"
"$QUILL" /tmp/close-a.rtf &
sleep 1.5
waitcount 1
check "untitled window replaced" "$(count)" "1"
check "document window" "$(title 0)" "close-a.rtf"

# 2. Your case: a second document, then close the window behind it.
"$QUILL" /tmp/close-b.rtf &
sleep 1.5
waitcount 2
check "two windows" "$(count)" "2"
behind=0
[ "$(title 0)" == "close-b.rtf" ] && behind=1
H _QRQ of Window "$behind" >/dev/null
waitcount 1
check "window behind closed" "$(count)" "1"
check "other window remains" "$(title 0)" "close-b.rtf"
check "remaining window responds" "$(bash q.sh info | grep -o 'paras=[0-9]*')" "paras=1"

# 3. Close (File menu) on the last window quits.
H Qcmd of Window 0 with cmd='"post Fcls"' >/dev/null
for i in $(seq 1 20); do [ "$(running)" == "no" ] && break; sleep 0.25; done
check "closing the last window quits" "$(running)" "no"

# 4. A modified Untitled window is kept when a document is opened.
bash launch.sh >/dev/null
bash q.sh "type keep me" >/dev/null
"$QUILL" /tmp/close-a.rtf &
sleep 1.5
waitcount 2
check "modified untitled window kept" "$(count)" "2"
exit $fail
