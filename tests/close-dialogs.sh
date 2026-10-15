#!/bin/bash
# Closes every Quill window except the document windows (titles that do
# not belong to dialogs are kept).
count=$(hey "${QUILL_SIG:-application/x-vnd.Quill}" count Window 2>/dev/null \
	| sed -n 's/.*"result" (B_INT32_TYPE) : \([0-9]*\).*/\1/p')
for ((i = ${count:-0} - 1; i >= 0; i--)); do
	title=$(hey "${QUILL_SIG:-application/x-vnd.Quill}" get Title of Window $i 2>/dev/null \
		| sed -n 's/.*"result" (B_STRING_TYPE) : "\(.*\)"$/\1/p')
	case "$title" in
		Untitled*|*.rtf|*.txt|"Print Preview"*|"") ;;
		*) hey "${QUILL_SIG:-application/x-vnd.Quill}" _QRQ of Window $i >/dev/null 2>&1 ;;
	esac
done
