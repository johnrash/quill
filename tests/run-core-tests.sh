#!/bin/bash
# Builds and runs the core unit tests with AddressSanitizer and UBSan.
# Usage: tests/run-core-tests.sh [path to an ispell word list]
set -e
cd "$(dirname "$0")/.."
OUT=${TMPDIR:-/tmp}/quill-core-test
g++ -std=c++17 -g -O1 -Wall -Wextra -fsanitize=address,undefined \
	-fno-omit-frame-pointer -o "$OUT" tests/core_test.cpp src/core/*.cpp
"$OUT" "$@"
