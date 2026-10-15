/*
 * Quill - plain text import and export.
 */
#include "TextIO.h"
#include "Layout.h"
#include "Text.h"

namespace quill {

bool
LooksLikeRTF(const std::string& data)
{
	size_t start = 0;
	if (data.compare(0, 3, "\xEF\xBB\xBF") == 0)
		start = 3;
	return data.compare(start, 5, "{\\rtf") == 0;
}


static std::string
Utf16ToUtf8(const std::string& data, bool bigEndian)
{
	std::string result;
	size_t i = 2;
	uint32_t high = 0;
	while (i + 1 < data.size()) {
		unsigned char a = data[i];
		unsigned char b = data[i + 1];
		uint32_t unit = bigEndian ? (a << 8) | b : (b << 8) | a;
		i += 2;
		if (unit >= 0xD800 && unit < 0xDC00) {
			high = unit;
			continue;
		}
		if (unit >= 0xDC00 && unit < 0xE000 && high != 0) {
			AppendUtf8(result, 0x10000 + ((high - 0xD800) << 10)
				+ (unit - 0xDC00));
			high = 0;
			continue;
		}
		high = 0;
		AppendUtf8(result, unit);
	}
	return result;
}


std::string
TextToUtf8(const std::string& data, std::string* encoding)
{
	if (data.compare(0, 3, "\xEF\xBB\xBF") == 0) {
		if (encoding)
			*encoding = "UTF-8";
		return data.substr(3);
	}
	if (data.size() >= 2 && (unsigned char)data[0] == 0xFF
		&& (unsigned char)data[1] == 0xFE) {
		if (encoding)
			*encoding = "UTF-16";
		return Utf16ToUtf8(data, false);
	}
	if (data.size() >= 2 && (unsigned char)data[0] == 0xFE
		&& (unsigned char)data[1] == 0xFF) {
		if (encoding)
			*encoding = "UTF-16";
		return Utf16ToUtf8(data, true);
	}
	if (IsValidUtf8(data.c_str(), data.size())) {
		if (encoding)
			*encoding = "UTF-8";
		return data;
	}
	// Classic Mac text uses CR line ends; DOS/Windows text uses CRLF.
	size_t cr = 0;
	size_t lf = 0;
	for (char c : data) {
		if (c == '\r')
			cr++;
		else if (c == '\n')
			lf++;
	}
	if (cr > 0 && lf == 0) {
		if (encoding)
			*encoding = "Mac Roman";
		return MacRomanToUtf8(data);
	}
	if (encoding)
		*encoding = "Windows-1252";
	return Cp1252ToUtf8(data);
}


Story
TextToStory(const std::string& utf8, const Document& document, int32_t style)
{
	Story story;
	CharFormat format = document.StyleCharFormat(style);
	size_t begin = 0;
	while (true) {
		size_t stop = utf8.find_first_of("\r\n", begin);
		std::string line = utf8.substr(begin,
			stop == std::string::npos ? std::string::npos : stop - begin);
		// keep tabs and page breaks, drop other control characters
		std::string clean;
		clean.reserve(line.size());
		for (char c : line) {
			unsigned char u = (unsigned char)c;
			if (u >= 0x20 || c == '\t' || c == '\f')
				clean += c;
		}
		Paragraph paragraph = document.NewParagraph(style);
		paragraph.Insert(0, clean, format);
		story.push_back(paragraph);
		if (stop == std::string::npos)
			break;
		begin = stop + 1;
		if (utf8[stop] == '\r' && begin < utf8.size() && utf8[begin] == '\n')
			begin++;
	}
	if (story.empty())
		story.push_back(document.NewParagraph(style));
	return story;
}


void
ReadPlainText(const std::string& data, Document& document)
{
	document.Reset(gDefaultSerif, gDefaultSans);
	std::string utf8 = TextToUtf8(data, nullptr);
	document.body = TextToStory(utf8, document, 0);
	// A final line end does not start another paragraph.
	if (document.body.size() > 1 && document.body.back().text.empty())
		document.body.pop_back();
}


static void
AppendParagraphText(std::string& out, const Paragraph& paragraph)
{
	const std::string& text = paragraph.text;
	int32_t length = (int32_t)text.size();
	for (int32_t offset = 0; offset < length;) {
		int32_t consumed;
		uint32_t c = DecodeUtf8(text.c_str() + offset, length - offset,
			&consumed);
		int32_t next = offset + std::max<int32_t>(1, consumed);
		switch (c) {
			case kLineBreakChar:
				out += '\n';
				break;
			case kPageBreakChar:
			case kSectionBreakChar:
				out += '\f';
				break;
			case kSoftHyphenChar:
			case kPictureChar:
			case kFootnoteChar:
				break;
			case kFieldPageChar:
			case kFieldPagesChar:
				out += '1';
				break;
			case kFieldDateChar:
			case kFieldTimeChar:
				break;
			case kNbspChar:
				out += ' ';
				break;
			case kNbHyphenChar:
				out += '-';
				break;
			default:
				out.append(text, offset, next - offset);
				break;
		}
		offset = next;
	}
}


std::string
StoryToText(const Story& story)
{
	std::string out;
	for (size_t i = 0; i < story.size(); i++) {
		const Paragraph& paragraph = story[i];
		AppendParagraphText(out, paragraph);
		if (paragraph.kind == kParaCell) {
			bool lastCell = i + 1 < story.size()
				&& story[i + 1].kind == kParaRowEnd;
			if (!lastCell)
				out += '\t';
			continue;
		}
		if (i + 1 < story.size())
			out += '\n';
	}
	return out;
}


std::string
WritePlainText(const Document& document)
{
	std::string out = StoryToText(document.body);
	out += '\n';
	return out;
}


std::string
WritePlainTextWithLineBreaks(const Document& document, const DocLayout& layout)
{
	std::string out;
	const Story& story = document.body;
	for (int32_t p = 0; p < (int32_t)story.size() && p < layout.ParaCount();
			p++) {
		const Paragraph& paragraph = story[p];
		if (paragraph.kind == kParaRowEnd) {
			out += '\n';
			continue;
		}
		const ParaLayout& paraLayout = layout.Para(p);
		for (const LineBox& line : paraLayout.lines) {
			int32_t from = paraLayout.cpByte[line.startCp];
			int32_t to = paraLayout.cpByte[line.endCp];
			Paragraph slice = paragraph.Slice(from, to);
			std::string text;
			AppendParagraphText(text, slice);
			while (!text.empty() && (text.back() == ' ' || text.back() == '\n'))
				text.pop_back();
			out += text;
			if (paragraph.kind == kParaCell && line.last)
				out += '\t';
			else
				out += '\n';
		}
	}
	return out;
}

}	// namespace quill
