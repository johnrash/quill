#!/bin/bash
# Opens a large text file and measures typing and repagination.
cd "$(dirname "$0")"
Q() { bash q.sh "$1"; }
start=$(date +%s%N)
bash launch.sh /tmp/big.txt >/dev/null
Q "info" >/dev/null
end=$(date +%s%N)
echo "open and lay out: $(( (end - start) / 1000000 )) ms (includes launch)"
Q "info"
Q "select 2500 5 2500 5" >/dev/null
start=$(date +%s%N)
for i in $(seq 1 20); do Q "type x" >/dev/null; done
end=$(date +%s%N)
echo "typing in the middle: $(( (end - start) / 20000000 )) ms per character (with hey)"
start=$(date +%s%N)
for i in $(seq 1 20); do Q "info" >/dev/null; done
end=$(date +%s%N)
echo "hey round trip alone: $(( (end - start) / 20000000 )) ms"
start=$(date +%s%N)
Q "post Trpg" >/dev/null; Q "info" >/dev/null
end=$(date +%s%N)
echo "repaginate (full relayout): $(( (end - start) / 1000000 )) ms"
Q "post Vpag" >/dev/null
start=$(date +%s%N)
for i in $(seq 1 20); do Q "type y" >/dev/null; done
end=$(date +%s%N)
echo "typing in page layout: $(( (end - start) / 20000000 )) ms per character"
