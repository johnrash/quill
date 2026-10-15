#!/bin/bash
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
bash launch.sh >/dev/null
Q "type Some text in the body." >/dev/null
Q "post Vhdr" >/dev/null
sleep 0.3
Q "keys Report\t\tPage " >/dev/null
Q "post Ipgn" >/dev/null
sleep 0.5
bash shot.sh /tmp/pane-header.png 0.3 >/dev/null
Q "post Vcpn" >/dev/null
Q "post Fsva" >/dev/null
sleep 1.5
bash shot.sh /tmp/save-panel.png 0.3 >/dev/null
