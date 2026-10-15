#!/bin/bash
# Text to Table and Table to Text through the Insert menu's Table command.
cd "$(dirname "$0")"
QUILL=${QUILL:-/boot/home/Quill/test-copy/QuillTest}
QUILL_SIG=${QUILL_SIG:-application/x-vnd.QuilT}
export QUILL QUILL_SIG
export QUILL_SETTINGS=${QUILL_SETTINGS:-Quill test settings}
Q() { bash q.sh "$1"; }
H() { timeout 3 hey "$QUILL_SIG" "$@" 2>&1; }
fail=0
check() {
	if [ "$2" == "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got '$2' expected '$3'"; fail=1; fi
}
label() {
	# Command key presses make the window update its menus
	Q "key 52 2" >/dev/null
	sleep 0.3
	H get Label of MenuItem 3 of Menu Insert of MenuBar of Window 0 \
		| sed -n 's/.*"result" (B_STRING_TYPE) : "\(.*\)"$/\1/p'
}
windowTitles() {
	n=$(H count Window | sed -n 's/.*"result" (B_INT32_TYPE) : \([0-9]*\).*/\1/p')
	for ((i = 0; i < ${n:-0}; i++)); do
		H get Title of Window $i | sed -n 's/.*"result" (B_STRING_TYPE) : "\(.*\)"$/\1/p'
	done
}
bash launch.sh >/dev/null
check "menu without selection" "$(label)" "Table…"
Q 'type Name,Size,Kind\nApple,"1,200",Fruit\nLeek,3,Vegetable' >/dev/null
Q "selectall" >/dev/null
check "menu with selected text" "$(label)" "Text to Table…"
Q "post Itbl" >/dev/null
sleep 1
check "dialog opens" "$(windowTitles | grep -c 'Text to Table')" "1"
[ -n "$SHOTS" ] && bash shot.sh /tmp/text-to-table.png 0.3 >/dev/null
H 'tOK ' of Window "Text to Table" >/dev/null
sleep 0.5
check "converted to a table" "$(Q info | grep -o 'paras=[0-9]*')" "paras=13"
Q "select 5 0 5 5" >/dev/null
check "quoted comma stays in its cell" "$(Q selection)" "1,200"
check "menu in a table" "$(label)" "Table to Text…"
[ -n "$SHOTS" ] && bash shot.sh /tmp/table-converted.png 0.3 >/dev/null
Q "post Itbl" >/dev/null
sleep 1
check "table to text dialog" "$(windowTitles | grep -c 'Table to Text')" "1"
H 'tOK ' of Window "Table to Text" >/dev/null
sleep 0.5
check "back to tab delimited text" "$(Q text)" 'Name\tSize\tKind\nApple\t1,200\tFruit\nLeek\t3\tVegetable\n'
Q "key 122 2" >/dev/null
sleep 0.3
check "undo restores the table" "$(Q info | grep -o 'paras=[0-9]*')" "paras=13"
exit $fail
