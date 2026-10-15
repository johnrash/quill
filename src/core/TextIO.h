/*
 * Quill - plain text import and export.
 */
#ifndef QUILL_TEXTIO_H
#define QUILL_TEXTIO_H

#include "Document.h"

#include <string>

namespace quill {

class DocLayout;

// Converts file contents to UTF-8: UTF-8 (with or without BOM), UTF-16
// with BOM, Mac Roman (classic Mac files, CR line ends) or Windows-1252.
std::string TextToUtf8(const std::string& data, std::string* encoding);

// Splits UTF-8 text at CR, LF or CRLF into paragraphs of the given style.
Story TextToStory(const std::string& utf8, const Document& document,
	int32_t style);

void ReadPlainText(const std::string& data, Document& document);

// "Text Only": one line per paragraph.
std::string WritePlainText(const Document& document);
// "Text Only with Line Breaks": one line per displayed line.
std::string WritePlainTextWithLineBreaks(const Document& document,
	const DocLayout& layout);

// Plain text of a story range for the clipboard; fields are resolved to
// their placeholder text and pictures are dropped.
std::string StoryToText(const Story& story);

bool LooksLikeRTF(const std::string& data);

}	// namespace quill

#endif	// QUILL_TEXTIO_H
