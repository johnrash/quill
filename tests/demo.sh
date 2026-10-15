#!/bin/bash
# Types a demonstration document and takes screenshots for the README.
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
bash launch.sh >/dev/null
hey "${QUILL_SIG:-application/x-vnd.Quill}" set Frame of Window 0 to "BRect(40,30,900,770)" >/dev/null
Q "keys Quill\n" >/dev/null
Q "keys A word processor in the spirit of 1992. Quill brings the menus, " >/dev/null
Q "keys Toolbar, Ribbon and Ruler of a classic Macintosh word processor " >/dev/null
Q "keys to Haiku, with \"smart quotes\", styles, tables, footnotes " >/dev/null
Q "keys and Page Layout view.\n" >/dev/null
Q "keys Features\n" >/dev/null
Q "keys Character formatting: bold, italic, underline, small caps and more\n" >/dev/null
Q "keys Paragraph formatting with the Ruler\nStyles, tabs, borders and tables\n" >/dev/null
Q "keys Results\n" >/dev/null
Q "keys The table below was made with Insert Table.\n" >/dev/null
sleep 1
Q "select 0 0 0 0" >/dev/null
Q "post Asty index=1" >/dev/null
for p in 2 6; do Q "select $p 0 $p 0" >/dev/null; Q "post Asty index=2" >/dev/null; done
# bold, italic, underline words in paragraph 1
Q "select 1 46 1 52" >/dev/null; Q "post Otgl field=4" >/dev/null
Q "select 1 53 1 56" >/dev/null; Q "post Otgl field=8" >/dev/null
Q "select 1 57 1 62" >/dev/null; Q "post Otgl field=16" >/dev/null
Q "select 1 0 1 0" >/dev/null; Q "post Oaln align=3" >/dev/null
Q "select 3 0 5 10" >/dev/null; Q "post Tbul" >/dev/null
Q "select 3 26 3 30" >/dev/null; Q "post Otgl field=4" >/dev/null
Q "select 3 32 3 38" >/dev/null; Q "post Otgl field=8" >/dev/null
Q "select 3 40 3 49" >/dev/null; Q "post Otgl field=16" >/dev/null
Q "select 3 51 3 61" >/dev/null; Q "post Otgl field=256" >/dev/null
# footnote after "1992"
Q "select 1 39 1 39" >/dev/null
Q "post Ifnt" >/dev/null
sleep 0.3
Q "keys Word 5.1 for the Macintosh was released in 1992." >/dev/null
Q "post Vcpn" >/dev/null
# table at the end
Q "select 8 0 8 0" >/dev/null
Q "post Gtbl rows=3 columns=3" >/dev/null
sleep 0.3
Q "keys View\tLooks like\tUse it for\tNormal\tOne column\tWriting\t" >/dev/null
Q "keys Page Layout\tThe printed page\tChecking pages" >/dev/null
# header
Q "post Vhdr" >/dev/null
sleep 0.3
Q "keys Quill demonstration\t\tPage " >/dev/null
Q "post Ipgn" >/dev/null
Q "post Vcpn" >/dev/null
Q "select 1 0 1 0" >/dev/null
sleep 0.8
bash shot.sh /tmp/demo-normal.png 0.5 >/dev/null
Q "post Vpag" >/dev/null
Q "post Gzom percent=75" >/dev/null
sleep 0.8
bash shot.sh /tmp/demo-page.png 0.5 >/dev/null
Q "post Gzom percent=100" >/dev/null
Q "post Vnrm" >/dev/null
rm -f /tmp/Quill-demo.rtf
Q "save /tmp/Quill-demo.rtf"
echo done
