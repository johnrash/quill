#!/bin/bash
# Builds a document using most features, screenshots it, saves it as RTF
# and opens it again.
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
bash launch.sh >/dev/null
Q "type Quarterly Report\n" >/dev/null
Q "select 0 0 0 0" >/dev/null
Q "post Asty index=1" >/dev/null
Q "select 1 0 1 0" >/dev/null
LOREM="Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat."
Q "type $LOREM Duis aute irure dolor in reprehenderit.\n" >/dev/null
# a table of 3 x 3 after the first paragraph
Q "post Gtbl rows=3 columns=3" >/dev/null
sleep 0.3
Q "type Region" >/dev/null
Q "info"
# continue below the table (the last paragraph)
Q "select 14 0 14 0" >/dev/null
for i in $(seq 1 12); do
	Q "type $LOREM\n" >/dev/null
done
# footnote in the first body paragraph
Q "select 1 11 1 11" >/dev/null
Q "post Ifnt" >/dev/null
sleep 0.3
Q "type This is the text of the first footnote." >/dev/null
Q "post Vcpn" >/dev/null
# header with a page number
Q "post Vhdr" >/dev/null
sleep 0.3
Q "type Quill Test Header\t\tPage " >/dev/null
Q "post Ipgn" >/dev/null
Q "post Vcpn" >/dev/null
Q "post Ocol columns=2" >/dev/null
Q "post Vpag" >/dev/null
Q "post Gzom percent=60" >/dev/null
sleep 0.8
Q "info"
bash shot.sh /tmp/feat-page.png 0.5 >/dev/null
Q "post Ocol columns=1" >/dev/null
Q "post Fprv" >/dev/null
sleep 0.8
bash shot.sh /tmp/feat-preview.png 0.5 >/dev/null
Q "post Fprv" >/dev/null
Q "post Gzom percent=100" >/dev/null
Q "post Vnrm" >/dev/null
sleep 0.3
bash shot.sh /tmp/feat-normal.png 0.5 >/dev/null
rm -f /tmp/quill-test.rtf
Q "save /tmp/quill-test.rtf"
Q "info"
Q "dump" > /tmp/dump1.rtf
bash launch.sh /tmp/quill-test.rtf >/dev/null
sleep 0.5
Q "info"
Q "dump" > /tmp/dump2.rtf
cmp /tmp/dump1.rtf /tmp/dump2.rtf && echo "reopened document is identical"
bash shot.sh /tmp/feat-reopen.png 0.5 >/dev/null
echo done
