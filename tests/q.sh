#!/bin/bash
# tests/q.sh "<command>" -- send a scripting command to Quill's front window
# and print the result. Commands: text, type <text>, select a b c d,
# selectall, selection, info, format, para, save <path>, undo, redo,
# post <code> [name=value ...], status, page, dump.
hey "${QUILL_SIG:-application/x-vnd.Quill}" Qcmd of Window 0 with cmd="\"$1\"" 2>/dev/null \
	| sed -n 's/.*"result" (B_STRING_TYPE) : "\(.*\)"$/\1/p'
