#!/bin/bash
# Makes a copy of the built Quill with the signature application/x-vnd.QuilT
# so that tests never touch a Quill someone is using.
cd "$(dirname "$0")/.."
mkdir -p test-copy
perl -pe 's/application\/x-vnd\.Quill/application\/x-vnd.QuilT/g' \
	objects.*/Quill > test-copy/QuillTest
chmod +x test-copy/QuillTest
mimeset -f test-copy/QuillTest
echo test-copy/QuillTest
