#!/bin/bash
# Mouse selection through synthetic mouse events.
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
fail=0
check() {
	if [ "$2" == "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got '$2' expected '$3'"; fail=1; fi
}
bash launch.sh >/dev/null
Q "type The quick brown fox jumps. It sleeps.\nSecond paragraph here." >/dev/null
sleep 0.3
read x y <<< "$(Q 'where 0 6')"
Q "click $x $y" >/dev/null
sleep 0.2
check "click places the caret" "$(Q info | grep -o 'caret=[0-9,]*')" "caret=0,6"
Q "click $x $y 2" >/dev/null
sleep 0.2
check "double-click selects a word" "$(Q selection)" "quick "
Q "click $x $y 3" >/dev/null
sleep 0.2
check "triple-click selects the paragraph" "$(Q selection)" 'The quick brown fox jumps. It sleeps.\n'
Q "click 5 $y" >/dev/null
sleep 0.2
check "selection bar selects the line" "$(Q selection)" 'The quick brown fox jumps. It sleeps.\n'
read x2 y2 <<< "$(Q 'where 1 6')"
Q "click 5 $y2 1 2" >/dev/null
sleep 0.2
check "Alt-click in the selection bar selects all" "$(Q selection)" 'The quick brown fox jumps. It sleeps.\nSecond paragraph here.'
Q "click $x $y" >/dev/null
Q "click $x2 $y2 1 1" >/dev/null
sleep 0.2
check "shift-click extends" "$(Q selection)" 'ick brown fox jumps. It sleeps.\nSecond'
read sx sy <<< "$(Q 'where 0 30')"
Q "click $sx $sy 1 2" >/dev/null
sleep 0.2
check "Alt-click selects a sentence" "$(Q selection)" "It sleeps."
read ax ay <<< "$(Q 'where 0 0')"
read bx by <<< "$(Q 'where 0 9')"
Q "drag $ax $ay $bx $by" >/dev/null
sleep 0.3
check "drag selects" "$(Q selection)" "The quick"
Q "post Voln" >/dev/null
Q "select 0 0 0 0" >/dev/null
Q "post Asty index=1" >/dev/null
sleep 0.3
read hx hy <<< "$(Q 'where 0 0')"
Q "click $((hx - 12)) $hy" >/dev/null
sleep 0.2
check "outline symbol selects heading with its text" "$(Q selection)" 'The quick brown fox jumps. It sleeps.\nSecond paragraph here.'
Q "click $((hx - 12)) $hy 2" >/dev/null
sleep 0.3
check "double-click on the symbol collapses" "$(Q 'visible 1')" "0"
Q "click $((hx - 12)) $hy 2" >/dev/null
sleep 0.3
check "and expands again" "$(Q 'visible 1')" "1"
exit $fail
