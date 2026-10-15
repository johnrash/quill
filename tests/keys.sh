#!/bin/bash
# Keyboard editing through synthetic key events.
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
fail=0
check() {
	if [ "$2" == "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got '$2' expected '$3'"; fail=1; fi
}
bash launch.sh >/dev/null
Q "keys He said \"it's done\" today." >/dev/null
sleep 0.5
check "smart quotes" "$(Q text)" "He said “it’s done” today."
# Left arrow 7 times, then Backspace removes the closing quote
for i in 1 2 3 4 5 6 7; do Q "key 28" >/dev/null; done
Q "key 8" >/dev/null
sleep 0.3
check "arrows and backspace" "$(Q text)" "He said “it’s done today."
# Home, shift-End selects the line; Alt-Shift-B makes it bold
Q "key 1" >/dev/null
Q "key 4 1" >/dev/null
sleep 0.2
check "shift-end selection" "$(Q selection)" "He said “it’s done today."
Q "key 98 3" >/dev/null
sleep 0.3
check "Alt-Shift-B bold" "$(Q format | grep -o 'bold=[01]')" "bold=1"
Q "key 117 2" >/dev/null
sleep 0.3
check "Alt-U underline" "$(Q format | grep -o 'underline=[0-9]')" "underline=1"
# End, Return, type, Shift-Return
Q "key 4" >/dev/null
Q "key 10" >/dev/null
Q "keys Second" >/dev/null
Q "key 10 1" >/dev/null
Q "keys line" >/dev/null
sleep 0.3
check "paragraphs" "$(Q info | grep -o 'paras=[0-9]*')" "paras=2"
check "new line" "$(Q text)" 'He said “it’s done today.\nSecond\vline'
# undo: continuous typing is one step, as in Word
Q "key 122 2" >/dev/null
sleep 0.3
check "Alt-Z undo" "$(Q text)" 'He said “it’s done today.'
Q "key 121 2" >/dev/null
sleep 0.3
check "Alt-Y redo" "$(Q text)" 'He said “it’s done today.\nSecond\vline'
Q "key 122 2" >/dev/null
# Alt-Shift-C centers, Alt-M opens Paragraph
Q "key 99 3" >/dev/null
sleep 0.2
check "Alt-Shift-C center" "$(Q para | grep -o 'align=[0-9]')" "align=1"
# table: Tab moves to the next cell
Q "key 4" >/dev/null
Q "key 10" >/dev/null
Q "post Gtbl rows=2 columns=2" >/dev/null
sleep 0.3
Q "keys A1" >/dev/null
Q "key 9" >/dev/null
Q "keys B1" >/dev/null
Q "key 9" >/dev/null
Q "keys A2" >/dev/null
sleep 0.3
check "table cells" "$(Q info | grep -o 'caret=[0-9]*')" "caret=4"
Q 'select 4 0 4 2' >/dev/null
check "cell text" "$(Q selection)" "A2"
exit $fail
