/*
 * Quill - Rich Text Format reader and writer.
 *
 * RTF is Quill's native file format and its rich clipboard format, as it
 * was Word's interchange format.
 */
#ifndef QUILL_RTF_H
#define QUILL_RTF_H

#include "Document.h"

#include <string>

namespace quill {

// A whole document, including style sheet, page setup, headers, footers,
// footnotes, pictures and document information.
std::string WriteRTF(const Document& document);

// A fragment (clipboard contents): the last paragraph is not terminated
// by \par so that pasting a partial paragraph merges it into the target.
std::string WriteRTFFragment(const Document& document, const Story& fragment);

// Reads a document. On failure returns false and leaves a message.
bool ReadRTF(const std::string& data, Document& document,
	std::string* error = nullptr);

// Reads a fragment into scratch (a document that receives the fragment's
// styles, pictures and footnotes) and returns the paragraphs.
bool ReadRTFFragment(const std::string& data, Document& scratch,
	Story& fragment);

// Copies a story from one document into another: styles are matched by
// name (missing ones are added), pictures and footnotes get new ids.
Story ImportStory(Document& target, const Document& source,
	const Story& story);

// Maps font names that are common in old documents to installed families.
std::string SubstituteFontName(const std::string& name);

}	// namespace quill

#endif	// QUILL_RTF_H
