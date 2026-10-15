#!/bin/bash
# Starts printing a two-page document and shows what the print system does.
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
bash launch.sh >/dev/null
Q "type Printing test\nThis document is printed through the Haiku print system.\f\nSecond page." >/dev/null
Q "post Fprt" >/dev/null
sleep 2
bash shot.sh /tmp/print1.png 0.3 >/dev/null
roster | grep -i -E "print|preview" 
