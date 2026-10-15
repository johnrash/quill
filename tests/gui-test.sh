#!/bin/bash
# End-to-end test of Quill on Haiku, driven with hey through the window's
# scripting command ('Qcmd', see tests/q.sh) and synthetic key presses.
# Usage: tests/gui-test.sh   (run on Haiku after "make")
cd "$(dirname "$0")"
# By default a copy of the build with its own signature is tested, so a
# Quill that someone is working with is never touched. To test another
# binary set QUILL (path) and QUILL_SIG (its signature).
if [ -z "$QUILL" ]; then
	bash make-test-copy.sh >/dev/null
	export QUILL=/boot/home/Quill/test-copy/QuillTest
	export QUILL_SIG=application/x-vnd.QuilT
fi
Q() { bash q.sh "$1"; }
passed=0
failed=0
check() {
	if [ "$2" == "$3" ]; then
		passed=$((passed + 1))
		echo "ok   $1"
	else
		failed=$((failed + 1))
		echo "FAIL $1"
		echo "     got:      $2"
		echo "     expected: $3"
	fi
}
field() { Q "$1" | tr ' ' '\n' | grep "^$2=" | cut -d= -f2-; }

echo "== keyboard"
bash keys.sh | sed 's/^/  /'
[ "${PIPESTATUS[0]}" == 0 ] && passed=$((passed + 1)) || failed=$((failed + 1))

echo "== opening and closing windows"
bash close-test.sh | sed 's/^/  /'
[ "${PIPESTATUS[0]}" == 0 ] && passed=$((passed + 1)) || failed=$((failed + 1))

echo "== text to table"
bash table-text.sh | sed 's/^/  /'
[ "${PIPESTATUS[0]}" == 0 ] && passed=$((passed + 1)) || failed=$((failed + 1))

echo "== mouse"
bash mouse.sh | sed 's/^/  /'
[ "${PIPESTATUS[0]}" == 0 ] && passed=$((passed + 1)) || failed=$((failed + 1))

echo "== replace all"
bash launch.sh >/dev/null
Q "type one fish two fish\nred fish blue fish" >/dev/null
Q "post Drpa what=fish with=cat" >/dev/null
sleep 0.3
check "replace all" "$(Q text)" 'one cat two cat\nred cat blue cat'
check "replace status" "$(Q status)" "4 replacements made"
Q "post Drpa what=cat^p with=;^t" >/dev/null
sleep 0.3
check "replace paragraph mark" "$(Q text)" 'one cat two ;\tred cat blue cat'
Q "key 122 2" >/dev/null
sleep 0.3
check "undo replace" "$(Q text)" 'one cat two cat\nred cat blue cat'

echo "== styles, RTF and reopening"
Q "selectall" >/dev/null
Q "type Heading\nBody text with bold." >/dev/null
Q "select 0 0 0 0" >/dev/null
Q "post Asty index=1" >/dev/null
Q "select 1 15 1 19" >/dev/null
Q "post Otgl field=4" >/dev/null
Q "select 1 0 1 0" >/dev/null
Q "post Oaln align=3" >/dev/null
sleep 0.3
check "heading style" "$(field para style)" "Normal"
Q "select 0 0 0 0" >/dev/null
check "heading 1 applied" "$(Q para | grep -o 'style=heading')" "style=heading"
rm -f /tmp/gui-test.rtf
check "save" "$(Q 'save /tmp/gui-test.rtf')" "ok"
check "saved type" "$(catattr -d BEOS:TYPE /tmp/gui-test.rtf 2>/dev/null)" "text/rtf"
bash launch.sh /tmp/gui-test.rtf >/dev/null
check "reopened text" "$(Q text)" 'Heading\nBody text with bold.'
Q "select 1 15 1 19" >/dev/null
check "reopened bold" "$(field format bold)" "1"
Q "select 1 0 1 0" >/dev/null
check "reopened justify" "$(field para align)" "3"
check "not modified" "$(field info modified)" "0"

echo "== outline and table of contents"
bash launch.sh >/dev/null
Q "type Alpha\nalpha text\nBeta\nbeta text" >/dev/null
for p in 0 2; do Q "select $p 0 $p 0" >/dev/null; Q "post Asty index=1" >/dev/null; done
Q "post Voln" >/dev/null
check "outline mode" "$(field info mode)" "1"
Q "select 2 0 2 0" >/dev/null
Q "post Votl op=demote" >/dev/null
sleep 0.3
check "demote" "$(Q para | grep -o 'style=heading [0-9]')" "style=heading 2"
Q "post Votl op=promote" >/dev/null
Q "post Votl op=up" >/dev/null
sleep 0.3
check "move up" "$(Q text)" 'Alpha\nBeta\nalpha text\nbeta text'
Q "key 122 2" >/dev/null
Q "post Vnrm" >/dev/null
Q "select 0 0 0 0" >/dev/null
Q "post Itoc" >/dev/null
sleep 0.3
check "table of contents" "$(Q text | cut -c1-24)" 'Alpha\t1\nBeta\t1\nAlpha'
Q "select 0 0 0 0" >/dev/null
check "toc style" "$(Q para | grep -o 'style=toc 1')" "style=toc 1"

echo "== pages and views"
bash launch.sh >/dev/null
Q "type Page one\fPage two\fPage three" >/dev/null
sleep 0.3
check "page breaks" "$(field info pages)" "3"
Q "post Ocol columns=2" >/dev/null
check "columns" "$(field info columns)" "2"
Q "post Gzom percent=150" >/dev/null
check "zoom" "$(field info zoom)" "150"
Q "post Vpag" >/dev/null
check "page layout" "$(field info mode)" "2"
Q "post Fprv" >/dev/null
check "print preview" "$(field info mode)" "3"
Q "post Fprv" >/dev/null
check "back from preview" "$(field info mode)" "2"

echo
echo "$passed passed, $failed failed"
[ "$failed" == 0 ]
