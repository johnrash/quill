/*
 * Quill - UTF-8 helpers and the special characters used in paragraphs.
 */
#ifndef QUILL_TEXT_H
#define QUILL_TEXT_H

#include <cstdint>
#include <string>

namespace quill {

// Characters with a special meaning inside paragraph text.
enum : uint32_t {
	kTabChar			= 0x09,
	kLineBreakChar		= 0x0B,		// Shift-Return: new line, same paragraph
	kPageBreakChar		= 0x0C,		// manual page break
	kCellMarkChar		= 0x07,		// (only in RTF/clipboard conversions)
	kNbspChar			= 0x00A0,	// nonbreaking space
	kSoftHyphenChar		= 0x00AD,	// optional hyphen
	kNbHyphenChar		= 0x2011,	// nonbreaking hyphen
	kFieldPageChar		= 0xE000,	// page number
	kFieldDateChar		= 0xE001,	// current date
	kFieldTimeChar		= 0xE002,	// current time
	kFieldPagesChar		= 0xE003,	// number of pages
	kFootnoteChar		= 0xE004,	// footnote reference (CharFormat::footnote)
	kSectionBreakChar	= 0xE00C,	// section break (acts as a new page)
	kPictureChar		= 0xFFFC	// picture (CharFormat::picture)
};

int32_t Utf8CharLength(unsigned char lead);
int32_t NextOffset(const std::string& text, int32_t offset);
int32_t PrevOffset(const std::string& text, int32_t offset);
uint32_t DecodeUtf8(const char* text, int32_t length, int32_t* consumed);
uint32_t CharAt(const std::string& text, int32_t offset);
void AppendUtf8(std::string& text, uint32_t c);
std::string Utf8(uint32_t c);
bool IsValidUtf8(const char* text, size_t length);
int32_t CountChars(const std::string& text);

bool IsSpace(uint32_t c);			// breaking white space (not tab)
bool IsWordChar(uint32_t c);		// letters, digits, apostrophes inside words
bool IsLetter(uint32_t c);
bool IsDigit(uint32_t c);
bool IsUpper(uint32_t c);
bool IsLower(uint32_t c);
uint32_t ToUpper(uint32_t c);
uint32_t ToLower(uint32_t c);
bool IsSpecialChar(uint32_t c);		// field, footnote, picture, breaks
bool IsSentenceEnd(uint32_t c);

std::string ToUpperString(const std::string& text);
std::string ToLowerString(const std::string& text);

// Legacy 8-bit encodings for reading old files and RTF \'hh escapes.
uint32_t MacRomanToUnicode(unsigned char c);
uint32_t Cp1252ToUnicode(unsigned char c);
uint32_t Cp437ToUnicode(unsigned char c);
std::string MacRomanToUtf8(const std::string& text);
std::string Cp1252ToUtf8(const std::string& text);

}	// namespace quill

#endif	// QUILL_TEXT_H
