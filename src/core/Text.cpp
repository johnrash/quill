/*
 * Quill - UTF-8 helpers.
 */
#include "Text.h"

namespace quill {

int32_t
Utf8CharLength(unsigned char lead)
{
	if (lead < 0x80)
		return 1;
	if ((lead & 0xE0) == 0xC0)
		return 2;
	if ((lead & 0xF0) == 0xE0)
		return 3;
	if ((lead & 0xF8) == 0xF0)
		return 4;
	return 1;
}


int32_t
NextOffset(const std::string& text, int32_t offset)
{
	int32_t size = (int32_t)text.size();
	if (offset >= size)
		return size;
	offset++;
	while (offset < size && ((unsigned char)text[offset] & 0xC0) == 0x80)
		offset++;
	return offset;
}


int32_t
PrevOffset(const std::string& text, int32_t offset)
{
	if (offset <= 0)
		return 0;
	if (offset > (int32_t)text.size())
		offset = (int32_t)text.size();
	offset--;
	while (offset > 0 && ((unsigned char)text[offset] & 0xC0) == 0x80)
		offset--;
	return offset;
}


uint32_t
DecodeUtf8(const char* text, int32_t length, int32_t* consumed)
{
	const unsigned char* s = (const unsigned char*)text;
	if (length <= 0) {
		if (consumed)
			*consumed = 0;
		return 0;
	}
	uint32_t c = s[0];
	int32_t count = Utf8CharLength(s[0]);
	if (count > length || count == 1) {
		if (consumed)
			*consumed = 1;
		return c < 0x80 ? c : 0xFFFD;
	}
	if (count == 2)
		c &= 0x1F;
	else if (count == 3)
		c &= 0x0F;
	else
		c &= 0x07;
	for (int32_t i = 1; i < count; i++) {
		if ((s[i] & 0xC0) != 0x80) {
			if (consumed)
				*consumed = i;
			return 0xFFFD;
		}
		c = (c << 6) | (s[i] & 0x3F);
	}
	if (consumed)
		*consumed = count;
	return c;
}


uint32_t
CharAt(const std::string& text, int32_t offset)
{
	if (offset < 0 || offset >= (int32_t)text.size())
		return 0;
	return DecodeUtf8(text.c_str() + offset, (int32_t)text.size() - offset,
		nullptr);
}


void
AppendUtf8(std::string& text, uint32_t c)
{
	if (c < 0x80) {
		text += (char)c;
	} else if (c < 0x800) {
		text += (char)(0xC0 | (c >> 6));
		text += (char)(0x80 | (c & 0x3F));
	} else if (c < 0x10000) {
		text += (char)(0xE0 | (c >> 12));
		text += (char)(0x80 | ((c >> 6) & 0x3F));
		text += (char)(0x80 | (c & 0x3F));
	} else {
		text += (char)(0xF0 | (c >> 18));
		text += (char)(0x80 | ((c >> 12) & 0x3F));
		text += (char)(0x80 | ((c >> 6) & 0x3F));
		text += (char)(0x80 | (c & 0x3F));
	}
}


std::string
Utf8(uint32_t c)
{
	std::string text;
	AppendUtf8(text, c);
	return text;
}


bool
IsValidUtf8(const char* text, size_t length)
{
	const unsigned char* s = (const unsigned char*)text;
	size_t i = 0;
	while (i < length) {
		unsigned char c = s[i];
		if (c < 0x80) {
			i++;
			continue;
		}
		int32_t count;
		if ((c & 0xE0) == 0xC0 && c >= 0xC2)
			count = 2;
		else if ((c & 0xF0) == 0xE0)
			count = 3;
		else if ((c & 0xF8) == 0xF0 && c <= 0xF4)
			count = 4;
		else
			return false;
		if (i + count > length)
			return false;
		for (int32_t k = 1; k < count; k++) {
			if ((s[i + k] & 0xC0) != 0x80)
				return false;
		}
		i += count;
	}
	return true;
}


int32_t
CountChars(const std::string& text)
{
	int32_t count = 0;
	for (unsigned char c : text) {
		if ((c & 0xC0) != 0x80)
			count++;
	}
	return count;
}


bool
IsSpace(uint32_t c)
{
	return c == ' ' || c == 0x2002 || c == 0x2003 || c == 0x2009
		|| c == 0x3000;
}


bool
IsDigit(uint32_t c)
{
	return c >= '0' && c <= '9';
}


bool
IsLetter(uint32_t c)
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return true;
	if (c < 0xC0)
		return c == 0xAA || c == 0xB5 || c == 0xBA;
	if (c == 0xD7 || c == 0xF7)
		return false;
	if (c < 0x2000)
		return true;	// Latin extended, Greek, Cyrillic, ...
	if (c >= 0x2000 && c < 0x3000)
		return false;	// punctuation, symbols, arrows, box drawing
	if (c >= 0xE000 && c < 0xF900)
		return false;	// private use (fields)
	if (c == kPictureChar)
		return false;
	return c >= 0x3040;	// CJK and beyond count as word characters
}


bool
IsWordChar(uint32_t c)
{
	return IsLetter(c) || IsDigit(c) || c == '_';
}


bool
IsUpper(uint32_t c)
{
	return ToLower(c) != c;
}


bool
IsLower(uint32_t c)
{
	return ToUpper(c) != c;
}


uint32_t
ToUpper(uint32_t c)
{
	if (c >= 'a' && c <= 'z')
		return c - 32;
	if (c < 0xB5)
		return c;
	if (c == 0xB5)
		return 0x39C;
	if (c >= 0xE0 && c <= 0xFE && c != 0xF7)
		return c - 32;
	if (c == 0xFF)
		return 0x178;
	if (c >= 0x100 && c <= 0x17F) {
		if (c == 0x131)
			return 'I';
		if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
			return (c & 1) ? c : c - 1;
		if (c == 0x130 || c == 0x138 || c == 0x149 || c == 0x17F)
			return c;
		return (c & 1) ? c - 1 : c;
	}
	if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2)
		return c - 32;
	if (c == 0x3C2)
		return 0x3A3;
	if (c >= 0x430 && c <= 0x44F)
		return c - 32;
	if (c >= 0x450 && c <= 0x45F)
		return c - 80;
	return c;
}


uint32_t
ToLower(uint32_t c)
{
	if (c >= 'A' && c <= 'Z')
		return c + 32;
	if (c < 0xC0)
		return c;
	if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
		return c + 32;
	if (c == 0x178)
		return 0xFF;
	if (c >= 0x100 && c <= 0x17F) {
		if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
			return (c & 1) ? c + 1 : c;
		if (c == 0x130)
			return 'i';
		if (c == 0x131 || c == 0x138 || c == 0x149 || c == 0x17F)
			return c;
		return (c & 1) ? c : c + 1;
	}
	if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2)
		return c + 32;
	if (c >= 0x410 && c <= 0x42F)
		return c + 32;
	if (c >= 0x400 && c <= 0x40F)
		return c + 80;
	return c;
}


bool
IsSpecialChar(uint32_t c)
{
	return c == kLineBreakChar || c == kPageBreakChar || c == kPictureChar
		|| c == kSectionBreakChar || (c >= kFieldPageChar && c <= kFootnoteChar);
}


bool
IsSentenceEnd(uint32_t c)
{
	return c == '.' || c == '!' || c == '?';
}


static std::string
MapString(const std::string& text, uint32_t (*map)(uint32_t))
{
	std::string result;
	result.reserve(text.size());
	int32_t offset = 0;
	int32_t size = (int32_t)text.size();
	while (offset < size) {
		int32_t consumed;
		uint32_t c = DecodeUtf8(text.c_str() + offset, size - offset,
			&consumed);
		AppendUtf8(result, map(c));
		offset += consumed;
	}
	return result;
}


std::string
ToUpperString(const std::string& text)
{
	return MapString(text, ToUpper);
}


std::string
ToLowerString(const std::string& text)
{
	return MapString(text, ToLower);
}


static const uint16_t kMacRoman[128] = {
	0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1,
	0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
	0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3,
	0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,
	0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,
	0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,
	0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211,
	0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,
	0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB,
	0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,
	0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,
	0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,
	0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1,
	0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
	0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC,
	0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7
};

static const uint16_t kCp1252[32] = {
	0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
	0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178
};

static const uint16_t kCp437[128] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
	0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
	0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
	0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
	0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
	0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
	0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
	0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
	0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
	0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};


uint32_t
MacRomanToUnicode(unsigned char c)
{
	return c < 0x80 ? c : kMacRoman[c - 0x80];
}


uint32_t
Cp1252ToUnicode(unsigned char c)
{
	if (c >= 0x80 && c < 0xA0)
		return kCp1252[c - 0x80];
	return c;
}


uint32_t
Cp437ToUnicode(unsigned char c)
{
	return c < 0x80 ? c : kCp437[c - 0x80];
}


std::string
MacRomanToUtf8(const std::string& text)
{
	std::string result;
	result.reserve(text.size() + text.size() / 8);
	for (unsigned char c : text)
		AppendUtf8(result, MacRomanToUnicode(c));
	return result;
}


std::string
Cp1252ToUtf8(const std::string& text)
{
	std::string result;
	result.reserve(text.size() + text.size() / 8);
	for (unsigned char c : text)
		AppendUtf8(result, Cp1252ToUnicode(c));
	return result;
}

}	// namespace quill
