#!/bin/bash
# Types a small document and takes screenshots of the views.
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
bash launch.sh >/dev/null
Q "type Quill Test Document\n" >/dev/null
Q "select 0 0 0 19" >/dev/null
Q "post Asty name=heading 1" >/dev/null
sleep 0.3
Q "select 1 0 1 0" >/dev/null
Q "type The quick brown fox jumps over the lazy dog. This paragraph is long enough to wrap onto a second line so that we can see how word wrapping and justification look in the document window.\n" >/dev/null
Q "type Bold, italic and underlined words, \"smart quotes\" and a tab\there.\n" >/dev/null
Q "select 2 0 2 4" >/dev/null
Q "post Otgl field=4" >/dev/null
Q "select 2 6 2 12" >/dev/null
Q "post Otgl field=8" >/dev/null
Q "select 2 17 2 27" >/dev/null
Q "post Otgl field=16" >/dev/null
Q "select 1 0 1 0" >/dev/null
Q "post Oaln align=3" >/dev/null
Q "select 3 0 3 0" >/dev/null
Q "type Centered line\n" >/dev/null
Q "select 3 0 3 0" >/dev/null
Q "post Oaln align=1" >/dev/null
sleep 0.5
Q "info"
Q "text"
bash shot.sh /tmp/smoke-normal.png 0.5 >/dev/null
Q "post Vpag" >/dev/null
sleep 0.5
bash shot.sh /tmp/smoke-page.png 0.5 >/dev/null
Q "post Vshw" >/dev/null
sleep 0.3
bash shot.sh /tmp/smoke-showall.png 0.5 >/dev/null
Q "post Vshw" >/dev/null
Q "post Vnrm" >/dev/null
echo done
