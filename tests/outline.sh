#!/bin/bash
# Builds an outline and shows it in Outline view.
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
bash launch.sh >/dev/null
Q "type Introduction\nWhy word processors matter, and why this one looks the way it does. Some body text that is long enough to wrap onto a second line in the window.\nHistory\nThe early years\nBody text about the early years.\nThe Macintosh years\nBody text about the Macintosh.\nConclusion\nThe end." >/dev/null
for p in 0 2 7; do Q "select $p 0 $p 0" >/dev/null; Q "post Asty index=1" >/dev/null; done
for p in 3 5; do Q "select $p 0 $p 0" >/dev/null; Q "post Asty index=2" >/dev/null; done
Q "post Voln" >/dev/null
sleep 0.5
bash shot.sh /tmp/outline-all.png 0.3 >/dev/null
Q "select 2 0 2 0" >/dev/null
Q "post Votl op=collapse" >/dev/null
sleep 0.3
bash shot.sh /tmp/outline-collapsed.png 0.3 >/dev/null
Q "post Votl op=level level=1" >/dev/null
sleep 0.3
bash shot.sh /tmp/outline-level1.png 0.3 >/dev/null
Q "post Votl op=level level=0" >/dev/null
Q "select 7 0 7 0" >/dev/null
Q "post Votl op=up" >/dev/null
Q "post Votl op=up" >/dev/null
sleep 0.3
Q "text" | head -3
Q "info"
bash shot.sh /tmp/outline-moved.png 0.3 >/dev/null
Q "post Vnrm" >/dev/null
echo done
