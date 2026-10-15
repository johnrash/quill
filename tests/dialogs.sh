#!/bin/bash
# Opens each dialog in turn and takes a screenshot of it.
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
bash launch.sh >/dev/null
Q "type Some text for the dialogs.\nSecond paragraph." >/dev/null
Q "select 0 0 0 4" >/dev/null
for code in Ochr Opar Otab Obrd Odoc Osec Fpgs Osty Ocas Efnd Erpl Egto Itbl Isym Twrd Tren Fsum Eglo Tprf Vzom Tspl; do
	Q "post $code" >/dev/null
	sleep 1
	bash shot.sh /tmp/dialog-$code.png 0.3 >/dev/null
	bash close-dialogs.sh
	sleep 0.3
done
echo done
