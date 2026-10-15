/*
 * Quill - Rich Text Format reader and writer.
 */
#include "RTF.h"
#include "Text.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

namespace quill {

static int
Twips(float points)
{
	return (int)std::lround(points * 20.0f);
}


static float
FromTwips(int twips)
{
	return twips / 20.0f;
}


std::string
SubstituteFontName(const std::string& name)
{
	static const char* kSerif[] = { "Times", "Times New Roman", "New York",
		"Times Roman", "Palatino", "Book Antiqua", "Georgia", "Garamond",
		"Bookman", "Century Schoolbook", "Cambria", "Charter", "Serif",
		"Tms Rmn", "Roman", nullptr };
	static const char* kSans[] = { "Helvetica", "Arial", "Geneva", "Chicago",
		"Verdana", "Tahoma", "Calibri", "Avant Garde", "Helv", "Swiss",
		"Lucida Grande", "Gill Sans", "Futura", "Optima", "Univers",
		"Sans Serif", "Charcoal", "Segoe UI", "MS Sans Serif", nullptr };
	static const char* kMono[] = { "Courier", "Courier New", "Monaco",
		"Consolas", "Lucida Console", "Modern", "Menlo", "Andale Mono",
		"Monospace", nullptr };
	static const char* kSymbol[] = { "Symbol", "Zapf Dingbats", "Wingdings",
		nullptr };
	for (int i = 0; kSerif[i]; i++) {
		if (strcasecmp(name.c_str(), kSerif[i]) == 0)
			return gDefaultSerif;
	}
	for (int i = 0; kSans[i]; i++) {
		if (strcasecmp(name.c_str(), kSans[i]) == 0)
			return gDefaultSans;
	}
	for (int i = 0; kMono[i]; i++) {
		if (strcasecmp(name.c_str(), kMono[i]) == 0)
			return "Noto Sans Mono";
	}
	for (int i = 0; kSymbol[i]; i++) {
		if (strcasecmp(name.c_str(), kSymbol[i]) == 0)
			return "Noto Sans Symbols";
	}
	return std::string();
}


// #pragma mark - writer


namespace {

class RtfWriter {
public:
							RtfWriter(const Document& document);

			std::string		WriteDocument();
			std::string		WriteFragment(const Story& fragment);

private:
			void			_Collect(const Story& story);
			void			_CollectAll();
			int				_FontIndex(const std::string& font) const;
			int				_ColorIndex(uint32_t color) const;
			void			_WriteHeader();
			void			_WriteStyleSheet();
			void			_WriteInfo();
			void			_WriteDocumentFormat();
			void			_WriteStory(const Story& story, bool terminateLast,
								bool inFootnote);
			void			_WriteParagraph(const Story& story, size_t index,
								bool terminate, bool inFootnote);
			void			_WriteRowDefinition(const RowFormat& row);
			void			_WriteParaProps(const ParaFormat& format);
			void			_WriteCharProps(const CharFormat& format);
			void			_WriteBorder(const char* side, uint8_t line,
								const ParaBorder& border);
			void			_WriteText(const Paragraph& paragraph,
								int32_t start, int32_t end, bool inFootnote);
			void			_WritePicture(const Picture* picture);
			void			_WriteEscaped(const std::string& text);
			void			_Control(const char* word);
			void			_Control(const char* word, int value);

			const Document&	fDocument;
			std::string		fOut;
			std::vector<std::string> fFonts;
			std::vector<uint32_t> fColors;
			std::set<int32_t> fWrittenFootnotes;
};


RtfWriter::RtfWriter(const Document& document)
	:
	fDocument(document)
{
}


void
RtfWriter::_Collect(const Story& story)
{
	for (const Paragraph& paragraph : story) {
		for (const Run& run : paragraph.runs) {
			if (_FontIndex(run.format.font) < 0)
				fFonts.push_back(run.format.font);
			if (run.format.color != kColorAuto
				&& _ColorIndex(run.format.color) < 0)
				fColors.push_back(run.format.color);
		}
	}
}


void
RtfWriter::_CollectAll()
{
	fFonts.clear();
	fColors.clear();
	fFonts.push_back(fDocument.StyleCharFormat(0).font);
	for (const Style& style : fDocument.styles.styles) {
		if (_FontIndex(style.cf.font) < 0 && !style.cf.font.empty())
			fFonts.push_back(style.cf.font);
		if (style.cf.color != kColorAuto && _ColorIndex(style.cf.color) < 0)
			fColors.push_back(style.cf.color);
	}
	_Collect(fDocument.body);
	_Collect(fDocument.header);
	_Collect(fDocument.footer);
	_Collect(fDocument.firstHeader);
	_Collect(fDocument.firstFooter);
	for (const auto& entry : fDocument.footnotes)
		_Collect(entry.second);
}


int
RtfWriter::_FontIndex(const std::string& font) const
{
	for (size_t i = 0; i < fFonts.size(); i++) {
		if (fFonts[i] == font)
			return (int)i;
	}
	return -1;
}


int
RtfWriter::_ColorIndex(uint32_t color) const
{
	if (color == kColorAuto)
		return 0;
	for (size_t i = 0; i < fColors.size(); i++) {
		if (fColors[i] == color)
			return (int)i + 1;
	}
	return -1;
}


void
RtfWriter::_Control(const char* word)
{
	fOut += '\\';
	fOut += word;
}


void
RtfWriter::_Control(const char* word, int value)
{
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "\\%s%d", word, value);
	fOut += buffer;
}


void
RtfWriter::_WriteEscaped(const std::string& text)
{
	int32_t length = (int32_t)text.size();
	for (int32_t offset = 0; offset < length;) {
		int32_t consumed;
		uint32_t c = DecodeUtf8(text.c_str() + offset, length - offset,
			&consumed);
		offset += std::max<int32_t>(1, consumed);
		switch (c) {
			case '\\': fOut += "\\\\"; continue;
			case '{': fOut += "\\{"; continue;
			case '}': fOut += "\\}"; continue;
			case '\t': fOut += "\\tab "; continue;
			case kNbspChar: fOut += "\\~"; continue;
			case kSoftHyphenChar: fOut += "\\-"; continue;
			case kNbHyphenChar: fOut += "\\_"; continue;
			case 0x2014: fOut += "\\emdash "; continue;
			case 0x2013: fOut += "\\endash "; continue;
			case 0x2018: fOut += "\\lquote "; continue;
			case 0x2019: fOut += "\\rquote "; continue;
			case 0x201C: fOut += "\\ldblquote "; continue;
			case 0x201D: fOut += "\\rdblquote "; continue;
			case 0x2022: fOut += "\\bullet "; continue;
		}
		if (c < 0x20)
			continue;
		if (c < 0x80) {
			fOut += (char)c;
			continue;
		}
		if (c >= 0x10000) {
			uint32_t v = c - 0x10000;
			int high = (int)(0xD800 + (v >> 10));
			int low = (int)(0xDC00 + (v & 0x3FF));
			_Control("u", high - 65536);
			fOut += '?';
			_Control("u", low - 65536);
			fOut += '?';
			continue;
		}
		_Control("u", c > 32767 ? (int)c - 65536 : (int)c);
		fOut += '?';
	}
}


void
RtfWriter::_WriteCharProps(const CharFormat& format)
{
	int font = std::max(0, _FontIndex(format.font));
	_Control("f", font);
	_Control("fs", (int)std::lround(format.size * 2));
	if (format.bold) _Control("b");
	if (format.italic) _Control("i");
	switch (format.underline) {
		case kULSingle: _Control("ul"); break;
		case kULWord: _Control("ulw"); break;
		case kULDouble: _Control("uldb"); break;
		case kULDotted: _Control("uld"); break;
	}
	if (format.strike) _Control("strike");
	if (format.outline) _Control("outl");
	if (format.shadow) _Control("shad");
	if (format.smallCaps) _Control("scaps");
	if (format.allCaps) _Control("caps");
	if (format.hidden) _Control("v");
	if (format.position > 0)
		_Control("up", (int)std::lround(format.position * 2));
	else if (format.position < 0)
		_Control("dn", (int)std::lround(-format.position * 2));
	if (format.spacing != 0)
		_Control("expndtw", Twips(format.spacing));
	if (format.color != kColorAuto)
		_Control("cf", std::max(0, _ColorIndex(format.color)));
	fOut += ' ';
}


void
RtfWriter::_WriteBorder(const char* side, uint8_t line,
	const ParaBorder& border)
{
	if (line == kBorderNone)
		return;
	_Control(side);
	switch (line) {
		case kBorderThick: _Control("brdrth"); break;
		case kBorderDouble: _Control("brdrdb"); break;
		case kBorderDotted: _Control("brdrdot"); break;
		case kBorderHairline: _Control("brdrhair"); break;
		default: _Control("brdrs"); break;
	}
	_Control("brdrw", line == kBorderThick ? 30 : 15);
	if (border.shadow)
		_Control("brdrsh");
	_Control("brsp", Twips(border.space));
}


void
RtfWriter::_WriteParaProps(const ParaFormat& format)
{
	_Control("s", format.style);
	switch (format.align) {
		case kAlignCenter: _Control("qc"); break;
		case kAlignRight: _Control("qr"); break;
		case kAlignJustify: _Control("qj"); break;
		default: _Control("ql"); break;
	}
	if (format.leftIndent != 0) _Control("li", Twips(format.leftIndent));
	if (format.rightIndent != 0) _Control("ri", Twips(format.rightIndent));
	if (format.firstIndent != 0) _Control("fi", Twips(format.firstIndent));
	if (format.spaceBefore != 0) _Control("sb", Twips(format.spaceBefore));
	if (format.spaceAfter != 0) _Control("sa", Twips(format.spaceAfter));
	switch (format.lineRule) {
		case kLineAtLeast:
			_Control("sl", Twips(format.lineValue));
			_Control("slmult", 0);
			break;
		case kLineExactly:
			_Control("sl", -Twips(format.lineValue));
			_Control("slmult", 0);
			break;
		default:
			if (format.lineValue != 1.0f) {
				_Control("sl", (int)std::lround(format.lineValue * 240));
				_Control("slmult", 1);
			}
			break;
	}
	if (format.keepWithNext) _Control("keepn");
	if (format.keepTogether) _Control("keep");
	if (format.pageBreakBefore) _Control("pagebb");
	if (format.noLineNumbers) _Control("noline");
	const ParaBorder& border = format.border;
	_WriteBorder("brdrt", border.top, border);
	_WriteBorder("brdrb", border.bottom, border);
	_WriteBorder("brdrl", border.left, border);
	_WriteBorder("brdrr", border.right, border);
	_WriteBorder("brdrbtw", border.between, border);
	if (border.shading > 0)
		_Control("shading", border.shading * 100);
	for (const TabStop& tab : format.tabs) {
		switch (tab.leader) {
			case kLeaderDot: _Control("tldot"); break;
			case kLeaderHyphen: _Control("tlhyph"); break;
			case kLeaderLine: _Control("tlul"); break;
		}
		switch (tab.kind) {
			case kTabCenter: _Control("tqc"); break;
			case kTabRight: _Control("tqr"); break;
			case kTabDecimal: _Control("tqdec"); break;
		}
		_Control(tab.kind == kTabBar ? "tb" : "tx", Twips(tab.pos));
	}
	if (format.inTable)
		_Control("intbl");
}


void
RtfWriter::_WriteHeader()
{
	fOut += "{\\rtf1\\ansi\\ansicpg1252\\deff0\\uc1\n{\\fonttbl";
	for (size_t i = 0; i < fFonts.size(); i++) {
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "{\\f%d\\fnil\\fcharset0 ", (int)i);
		fOut += buffer;
		_WriteEscaped(fFonts[i]);
		fOut += ";}";
	}
	fOut += "}\n{\\colortbl;";
	for (uint32_t color : fColors) {
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "\\red%u\\green%u\\blue%u;",
			(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
		fOut += buffer;
	}
	fOut += "}\n";
}


void
RtfWriter::_WriteStyleSheet()
{
	fOut += "{\\stylesheet";
	const StyleSheet& sheet = fDocument.styles;
	for (int32_t i = 0; i < sheet.Count(); i++) {
		const Style& style = sheet.styles[i];
		fOut += "{";
		ParaFormat format = style.pf;
		format.style = i;
		_WriteParaProps(format);
		_WriteCharProps(style.cf);
		if (style.basedOn >= 0 && style.basedOn != i)
			_Control("sbasedon", style.basedOn);
		else if (i != 0)
			_Control("sbasedon", 222);
		_Control("snext", style.next >= 0 ? style.next : i);
		fOut += ' ';
		_WriteEscaped(style.name);
		fOut += ";}\n";
	}
	fOut += "}\n";
}


static void
AppendTime(std::string& out, const char* word, time_t value)
{
	if (value == 0)
		return;
	struct tm parts;
	localtime_r(&value, &parts);
	char buffer[128];
	snprintf(buffer, sizeof(buffer), "{\\%s\\yr%d\\mo%d\\dy%d\\hr%d\\min%d}",
		word, parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
		parts.tm_hour, parts.tm_min);
	out += buffer;
}


void
RtfWriter::_WriteInfo()
{
	const DocInfo& info = fDocument.info;
	fOut += "{\\info";
	struct { const char* word; const std::string* value; } fields[] = {
		{ "title", &info.title }, { "subject", &info.subject },
		{ "author", &info.author }, { "keywords", &info.keywords },
		{ "doccomm", &info.comments }
	};
	for (auto& field : fields) {
		if (field.value->empty())
			continue;
		fOut += "{\\";
		fOut += field.word;
		fOut += ' ';
		_WriteEscaped(*field.value);
		fOut += '}';
	}
	if (!info.version.empty()) {
		fOut += "{\\*\\quillversion ";
		_WriteEscaped(info.version);
		fOut += '}';
	}
	AppendTime(fOut, "creatim", info.created);
	AppendTime(fOut, "revtim", info.revised);
	if (info.revisions > 0)
		_Control("version", info.revisions);
	fOut += "}\n";
}


void
RtfWriter::_WriteDocumentFormat()
{
	const DocSettings& s = fDocument.settings;
	_Control("paperw", Twips(s.PaperWidth()));
	_Control("paperh", Twips(s.PaperHeight()));
	_Control("margl", Twips(s.marginLeft));
	_Control("margr", Twips(s.marginRight));
	_Control("margt", Twips(s.marginTop));
	_Control("margb", Twips(s.marginBottom));
	_Control("gutter", Twips(s.gutter));
	if (s.landscape) _Control("landscape");
	if (s.mirrorMargins) _Control("margmirror");
	_Control("deftab", Twips(s.defaultTab));
	if (s.widowControl) _Control("widowctrl");
	switch (s.footnotePos) {
		case kFootnoteBeneath: _Control("ftntj"); break;
		case kFootnoteEndOfDocument: _Control("enddoc"); break;
		default: _Control("ftnbj"); break;
	}
	_Control("ftnstart", s.footnoteStart);
	if (s.footnoteRestartEachPage) _Control("ftnrstpg");
	if (s.scale != 100)
		fOut += "{\\*\\quillscale " + std::to_string(s.scale) + "}";
	fOut += "\n\\sectd";
	switch (s.sectionStart) {
		case kStartNoBreak: _Control("sbknone"); break;
		case kStartNewColumn: _Control("sbkcol"); break;
		case kStartEvenPage: _Control("sbkeven"); break;
		case kStartOddPage: _Control("sbkodd"); break;
		default: _Control("sbkpage"); break;
	}
	_Control("cols", s.columns);
	_Control("colsx", Twips(s.columnSpacing));
	_Control("headery", Twips(s.headerY));
	_Control("footery", Twips(s.footerY));
	if (s.titlePage) _Control("titlepg");
	_Control("pgnstarts", s.pageNumberStart);
	if (s.pageNumberStart != 1) _Control("pgnrestart");
	switch (s.pageNumberFormat) {
		case kPageUpperRoman: _Control("pgnucrm"); break;
		case kPageLowerRoman: _Control("pgnlcrm"); break;
		case kPageUpperLetter: _Control("pgnucltr"); break;
		case kPageLowerLetter: _Control("pgnlcltr"); break;
		default: _Control("pgndec"); break;
	}
	if (s.marginPageNumbers) {
		_Control("pgnx", Twips(s.pageNumberRight));
		_Control("pgny", Twips(s.pageNumberTop));
	}
	if (s.lineNumbering != 0) {
		_Control("linemod", 1);
		_Control("linex", Twips(s.lineNumberDistance));
		_Control(s.lineNumbering == 1 ? "lineppage" : "linecont");
	}
	fOut += "\n";

	struct { const char* word; const Story* story; } parts[] = {
		{ "header", &fDocument.header }, { "footer", &fDocument.footer },
		{ "headerf", &fDocument.firstHeader },
		{ "footerf", &fDocument.firstFooter }
	};
	for (auto& part : parts) {
		if (Document::StoryEmpty(*part.story)
			&& part.story->front().format.style
				== fDocument.styles.Find(part.word[0] == 'h' ? "header"
					: "footer"))
			continue;
		fOut += "{\\";
		fOut += part.word;
		fOut += ' ';
		_WriteStory(*part.story, true, false);
		fOut += "}\n";
	}
}


void
RtfWriter::_WritePicture(const Picture* picture)
{
	fOut += "{\\pict";
	if (picture == nullptr || picture->data.empty()) {
		_Control("picw", 1);
		_Control("pich", 1);
		_Control("picwgoal", Twips(picture ? picture->width : 36));
		_Control("pichgoal", Twips(picture ? picture->height : 36));
		fOut += " }";
		return;
	}
	if (picture->type == "jpeg")
		_Control("jpegblip");
	else
		_Control("pngblip");
	_Control("picw", picture->pixelWidth);
	_Control("pich", picture->pixelHeight);
	_Control("picwgoal", Twips(picture->width));
	_Control("pichgoal", Twips(picture->height));
	fOut += '\n';
	static const char kHex[] = "0123456789abcdef";
	int column = 0;
	for (unsigned char c : picture->data) {
		fOut += kHex[c >> 4];
		fOut += kHex[c & 15];
		if (++column == 64) {
			fOut += '\n';
			column = 0;
		}
	}
	fOut += "}";
}


void
RtfWriter::_WriteText(const Paragraph& paragraph, int32_t start, int32_t end,
	bool inFootnote)
{
	const std::string& text = paragraph.text;
	std::string plain;
	auto flush = [&]() {
		if (!plain.empty()) {
			_WriteEscaped(plain);
			plain.clear();
		}
	};
	const CharFormat& format = paragraph.FormatAt(start);
	for (int32_t offset = start; offset < end;) {
		int32_t consumed;
		uint32_t c = DecodeUtf8(text.c_str() + offset, end - offset,
			&consumed);
		int32_t next = offset + std::max<int32_t>(1, consumed);
		switch (c) {
			case kLineBreakChar: flush(); fOut += "\\line "; break;
			case kPageBreakChar: flush(); fOut += "\\page "; break;
			case kSectionBreakChar: flush(); fOut += "\\sect "; break;
			case kFieldPageChar: flush(); fOut += "\\chpgn "; break;
			case kFieldDateChar: flush(); fOut += "\\chdate "; break;
			case kFieldTimeChar: flush(); fOut += "\\chtime "; break;
			case kFieldPagesChar:
				flush();
				fOut += "{\\field{\\*\\fldinst NUMPAGES }{\\fldrslt 1}}";
				break;
			case kPictureChar:
				flush();
				_WritePicture(fDocument.PictureFor(format.picture));
				break;
			case kFootnoteChar: {
				flush();
				fOut += "\\chftn ";
				if (!inFootnote && format.footnote != 0
					&& fWrittenFootnotes.insert(format.footnote).second) {
					auto it = fDocument.footnotes.find(format.footnote);
					if (it != fDocument.footnotes.end()) {
						fOut += "{\\footnote ";
						_WriteStory(it->second, true, true);
						fOut += "}";
					}
				}
				break;
			}
			default:
				plain.append(text, offset, next - offset);
				break;
		}
		offset = next;
	}
	flush();
}


void
RtfWriter::_WriteRowDefinition(const RowFormat& row)
{
	fOut += "\\trowd";
	_Control("trgaph", Twips(row.gap));
	_Control("trleft", Twips(row.leftEdge));
	if (row.align == kAlignCenter) _Control("trqc");
	else if (row.align == kAlignRight) _Control("trqr");
	if (row.height != 0) _Control("trrh", Twips(row.height));
	for (const CellFormat& cell : row.cells) {
		ParaBorder dummy;
		_WriteBorder("clbrdrt", cell.top, dummy);
		_WriteBorder("clbrdrl", cell.left, dummy);
		_WriteBorder("clbrdrb", cell.bottom, dummy);
		_WriteBorder("clbrdrr", cell.rightLine, dummy);
		if (cell.shading > 0)
			_Control("clshdng", cell.shading * 100);
		_Control("cellx", Twips(row.leftEdge + cell.right));
	}
	fOut += '\n';
}


void
RtfWriter::_WriteParagraph(const Story& story, size_t index, bool terminate,
	bool inFootnote)
{
	const Paragraph& paragraph = story[index];
	if (paragraph.kind == kParaRowEnd) {
		fOut += "\\row\n";
		return;
	}
	if (paragraph.kind == kParaCell
		&& (index == 0 || story[index - 1].kind != kParaCell)) {
		// first cell of a row: write the row definition
		for (size_t j = index; j < story.size(); j++) {
			if (story[j].kind == kParaRowEnd) {
				_WriteRowDefinition(story[j].row);
				break;
			}
			if (story[j].kind != kParaCell)
				break;
		}
	}
	fOut += "\\pard\\plain ";
	ParaFormat format = paragraph.format;
	if (paragraph.kind == kParaCell)
		format.inTable = true;
	_WriteParaProps(format);
	fOut += ' ';
	for (size_t r = 0; r < paragraph.runs.size(); r++) {
		int32_t start = paragraph.runs[r].start;
		int32_t end = paragraph.RunEnd((int32_t)r);
		if (end <= start)
			continue;
		fOut += '{';
		_WriteCharProps(paragraph.runs[r].format);
		_WriteText(paragraph, start, end, inFootnote);
		fOut += '}';
	}
	if (paragraph.kind == kParaCell) {
		fOut += "\\cell\n";
		return;
	}
	if (terminate) {
		fOut += "\\plain ";
		_WriteCharProps(paragraph.MarkFormat());
		fOut += "\\par\n";
	}
}


void
RtfWriter::_WriteStory(const Story& story, bool terminateLast, bool inFootnote)
{
	for (size_t i = 0; i < story.size(); i++) {
		bool last = i + 1 == story.size();
		_WriteParagraph(story, i, !last || terminateLast, inFootnote);
	}
}


std::string
RtfWriter::WriteDocument()
{
	_CollectAll();
	fOut.clear();
	_WriteHeader();
	_WriteStyleSheet();
	_WriteInfo();
	_WriteDocumentFormat();
	_WriteStory(fDocument.body, true, false);
	fOut += "}\n";
	return fOut;
}


std::string
RtfWriter::WriteFragment(const Story& fragment)
{
	_CollectAll();
	_Collect(fragment);
	fOut.clear();
	_WriteHeader();
	_WriteStyleSheet();
	bool lastEmpty = fragment.size() > 1 && fragment.back().text.empty()
		&& fragment.back().kind == kParaNormal;
	for (size_t i = 0; i < fragment.size(); i++) {
		bool last = i + 1 == fragment.size();
		if (last && lastEmpty)
			break;
		_WriteParagraph(fragment, i, !last, false);
	}
	fOut += "}\n";
	return fOut;
}

}	// namespace


std::string
WriteRTF(const Document& document)
{
	RtfWriter writer(document);
	return writer.WriteDocument();
}


std::string
WriteRTFFragment(const Document& document, const Story& fragment)
{
	RtfWriter writer(document);
	return writer.WriteFragment(fragment);
}


// #pragma mark - reader


namespace {

enum Destination {
	kDestText = 0,
	kDestSkip,
	kDestFontTable,
	kDestColorTable,
	kDestStyleSheet,
	kDestStyleEntry,
	kDestInfo,
	kDestInfoText,
	kDestInfoTime,
	kDestPicture,
	kDestFieldInstruction,
	kDestQuillExtension
};

enum Codepage {
	kCp1252 = 0,
	kCpMac,
	kCp437,
	kCpSymbol
};

struct BorderTarget {
	enum { kTop = 1, kBottom = 2, kLeft = 4, kRight = 8, kBetween = 16 };
};

struct ReaderState {
	int			dest = kDestText;
	CharFormat	cf;
	ParaFormat	pf;
	int			uc = 1;
	int			builder = 0;
	int			borderTarget = 0;
	bool		cellBorder = false;
	int			infoField = 0;
	int			field = -1;
	int			pictureType = 0;
	int			fontCodepage = kCp1252;
	int			beforeUprDest = -1;
	int			footnoteId = 0;
};

struct Builder {
	Story*		story = nullptr;
	Paragraph	paragraph;
	bool		inUse = false;
};

struct StyleEntry {
	int			number = 0;
	std::string	name;
	CharFormat	cf;
	ParaFormat	pf;
	int			basedOn = 222;
	int			next = -1;
	bool		characterStyle = false;
};

struct FieldRecord {
	std::string	instruction;
};

struct PictureRecord {
	std::string	hex;
	std::string	binary;
	std::string	type;
	int			pixelWidth = 0;
	int			pixelHeight = 0;
	int			goalWidth = 0;
	int			goalHeight = 0;
	int			scaleX = 100;
	int			scaleY = 100;
};


class RtfReader {
public:
							RtfReader(const std::string& data,
								Document& document, bool fragment);
			bool			Parse(std::string* error);
			Story&			Fragment() { return fFragment; }

private:
			void			_PushGroup();
			void			_PopGroup();
			void			_ControlWord(const std::string& word,
								bool hasParam, int param);
			void			_ControlSymbol(char c);
			void			_Text(uint32_t c);
			void			_Byte(unsigned char c);
			void			_Flush();
			void			_EndParagraph();
			void			_EndCell();
			void			_EndRow();
			void			_InsertSpecial(uint32_t c,
								const CharFormat* format = nullptr);
			bool			_Destination(const std::string& word);
			int				_NewBuilder(Story* story);
			Builder&		_Current() { return fBuilders[fState.builder]; }
			CharFormat		_PlainFormat() const;
			ParaFormat		_PlainParaFormat() const;
			void			_FinishStyleSheet();
			void			_FinishPicture();
			void			_FinishBuilder(int index, bool fragment,
								const ParaFormat& format);
			void			_SetBorderLine(uint8_t line);
			void			_ApplyBorder(const std::function<void(uint8_t&)>&
								modify);

			const std::string& fData;
			size_t			fPos;
			Document&		fDocument;
			bool			fFragmentMode;
			Story			fFragment;
			std::vector<ReaderState> fStack;
			ReaderState		fState;
			std::vector<Builder> fBuilders;

			std::string		fPending;
			CharFormat		fPendingFormat;
			int				fSkipCount;
			bool			fIgnorable;
			uint32_t		fHighSurrogate;

			int				fCodepage;
			int				fDefaultFont;
			std::map<int, std::string> fFonts;
			std::map<int, int> fFontCodepages;
			int				fFontNumber;
			std::string		fFontName;
			std::vector<uint32_t> fColors;
			int				fRed, fGreen, fBlue;
			bool			fColorSet;

			std::vector<StyleEntry> fStyleEntries;
			StyleEntry		fStyleEntry;
			std::map<int, int> fStyleMap;
			bool			fHaveStyleSheet;

			std::vector<FieldRecord> fFields;
			PictureRecord	fPicture;
			std::string		fInfoText;
			int				fTimeParts[5];

			RowFormat		fRow;
			CellFormat		fCell;
			int32_t			fPendingFootnote;
			std::string		fExtension;
			TabStop			fPendingTab;
};


RtfReader::RtfReader(const std::string& data, Document& document,
	bool fragment)
	:
	fData(data),
	fPos(0),
	fDocument(document),
	fFragmentMode(fragment),
	fSkipCount(0),
	fIgnorable(false),
	fHighSurrogate(0),
	fCodepage(kCp1252),
	fDefaultFont(0),
	fFontNumber(0),
	fRed(0),
	fGreen(0),
	fBlue(0),
	fColorSet(false),
	fHaveStyleSheet(false),
	fPendingFootnote(0)
{
	memset(fTimeParts, 0, sizeof(fTimeParts));
}


CharFormat
RtfReader::_PlainFormat() const
{
	CharFormat format;
	auto it = fFonts.find(fDefaultFont);
	format.font = it != fFonts.end() ? it->second : gDefaultSerif;
	format.size = 12;
	return format;
}


ParaFormat
RtfReader::_PlainParaFormat() const
{
	ParaFormat format;
	format.style = 0;
	return format;
}


int
RtfReader::_NewBuilder(Story* story)
{
	Builder builder;
	builder.story = story;
	builder.paragraph = Paragraph(_PlainFormat());
	builder.inUse = true;
	story->clear();
	fBuilders.push_back(builder);
	return (int)fBuilders.size() - 1;
}


void
RtfReader::_Flush()
{
	if (fPending.empty())
		return;
	Builder& builder = _Current();
	CharFormat format = fPendingFormat;
	builder.paragraph.Insert(builder.paragraph.Length(), fPending, format);
	fPending.clear();
}


void
RtfReader::_Text(uint32_t c)
{
	if (fSkipCount > 0) {
		fSkipCount--;
		return;
	}
	switch (fState.dest) {
		case kDestSkip:
		case kDestPicture:
			return;
		case kDestFontTable:
			if (c == ';') {
				std::string name = fFontName;
				while (!name.empty() && name.back() == ' ')
					name.pop_back();
				size_t start = name.find_first_not_of(' ');
				name = start == std::string::npos ? "" : name.substr(start);
				if (!name.empty())
					fFonts[fFontNumber] = name;
				fFontName.clear();
			} else
				AppendUtf8(fFontName, c);
			return;
		case kDestColorTable:
			if (c == ';') {
				fColors.push_back(fColorSet
					? (uint32_t)((fRed << 16) | (fGreen << 8) | fBlue)
					: kColorAuto);
				fRed = fGreen = fBlue = 0;
				fColorSet = false;
			}
			return;
		case kDestStyleEntry:
			if (c == ';') {
				// the name ends; anything after it is ignored
				fStyleEntry.name += '\x01';
			} else if (fStyleEntry.name.empty()
				|| fStyleEntry.name.back() != '\x01')
				AppendUtf8(fStyleEntry.name, c);
			return;
		case kDestStyleSheet:
		case kDestInfo:
		case kDestInfoTime:
			return;
		case kDestInfoText:
			AppendUtf8(fInfoText, c);
			return;
		case kDestFieldInstruction:
			if (fState.field >= 0 && fState.field < (int)fFields.size())
				AppendUtf8(fFields[fState.field].instruction, c);
			return;
		case kDestQuillExtension:
			AppendUtf8(fExtension, c);
			return;
	}
	if (!fPending.empty() && fPendingFormat != fState.cf)
		_Flush();
	if (fPending.empty())
		fPendingFormat = fState.cf;
	AppendUtf8(fPending, c);
}


void
RtfReader::_Byte(unsigned char c)
{
	int codepage = fState.fontCodepage != kCp1252 ? fState.fontCodepage
		: fCodepage;
	uint32_t u;
	switch (codepage) {
		case kCpMac: u = MacRomanToUnicode(c); break;
		case kCp437: u = Cp437ToUnicode(c); break;
		case kCpSymbol: {
			// Symbol font: Greek letters and common math symbols
			if (c >= 'A' && c <= 'Z') {
				static const uint16_t kUpper[26] = { 0x391, 0x392, 0x3A7,
					0x394, 0x395, 0x3A6, 0x393, 0x397, 0x399, 0x3D1, 0x39A,
					0x39B, 0x39C, 0x39D, 0x39F, 0x3A0, 0x398, 0x3A1, 0x3A3,
					0x3A4, 0x3A5, 0x3C2, 0x3A9, 0x39E, 0x3A8, 0x396 };
				u = kUpper[c - 'A'];
			} else if (c >= 'a' && c <= 'z') {
				static const uint16_t kLower[26] = { 0x3B1, 0x3B2, 0x3C7,
					0x3B4, 0x3B5, 0x3C6, 0x3B3, 0x3B7, 0x3B9, 0x3D5, 0x3BA,
					0x3BB, 0x3BC, 0x3BD, 0x3BF, 0x3C0, 0x3B8, 0x3C1, 0x3C3,
					0x3C4, 0x3C5, 0x3D6, 0x3C9, 0x3BE, 0x3C8, 0x3B6 };
				u = kLower[c - 'a'];
			} else if (c == 0xB7)
				u = 0x2022;
			else if (c == 0xA5)
				u = 0x221E;
			else if (c == 0xB1)
				u = 0xB1;
			else if (c == 0xB4)
				u = 0xD7;
			else if (c == 0xB8)
				u = 0xF7;
			else if (c == 0xB9)
				u = 0x2260;
			else if (c == 0xA3)
				u = 0x2264;
			else if (c == 0xB3)
				u = 0x2265;
			else if (c == 0xAE)
				u = 0x2192;
			else if (c == 0xAC)
				u = 0x2190;
			else
				u = Cp1252ToUnicode(c);
			break;
		}
		default: u = Cp1252ToUnicode(c); break;
	}
	_Text(u);
}


void
RtfReader::_InsertSpecial(uint32_t c, const CharFormat* format)
{
	if (fState.dest != kDestText)
		return;
	if (fSkipCount > 0) {
		fSkipCount--;
		return;
	}
	_Flush();
	Builder& builder = _Current();
	CharFormat insert = format ? *format : fState.cf;
	builder.paragraph.Insert(builder.paragraph.Length(), Utf8(c), insert);
}


void
RtfReader::_EndParagraph()
{
	if (fState.dest != kDestText)
		return;
	_Flush();
	Builder& builder = _Current();
	Paragraph& paragraph = builder.paragraph;
	if (fState.pf.inTable) {
		// A paragraph inside a cell becomes a line break.
		paragraph.Insert(paragraph.Length(), Utf8(kLineBreakChar), fState.cf);
		return;
	}
	if (paragraph.text.empty())
		paragraph.SetMarkFormat(fState.cf);
	paragraph.format = fState.pf;
	paragraph.kind = kParaNormal;
	paragraph.format.inTable = false;
	paragraph.Touch();
	builder.story->push_back(paragraph);
	paragraph = Paragraph(fState.cf);
}


void
RtfReader::_EndCell()
{
	if (fState.dest != kDestText)
		return;
	_Flush();
	Builder& builder = _Current();
	Paragraph& paragraph = builder.paragraph;
	if (paragraph.text.empty())
		paragraph.SetMarkFormat(fState.cf);
	paragraph.format = fState.pf;
	paragraph.format.inTable = true;
	paragraph.kind = kParaCell;
	paragraph.Touch();
	builder.story->push_back(paragraph);
	paragraph = Paragraph(fState.cf);
}


void
RtfReader::_EndRow()
{
	if (fState.dest != kDestText)
		return;
	_Flush();
	Builder& builder = _Current();
	Story& story = *builder.story;
	// Count the cells of this row.
	int cells = 0;
	for (int i = (int)story.size() - 1; i >= 0 && story[i].kind == kParaCell;
			i--)
		cells++;
	if (cells == 0) {
		// \row without cells: make one empty cell
		Paragraph cell(fState.cf);
		cell.format = fState.pf;
		cell.format.inTable = true;
		cell.kind = kParaCell;
		story.push_back(cell);
		cells = 1;
	}
	RowFormat row = fRow;
	while ((int)row.cells.size() < cells) {
		CellFormat cell;
		cell.right = (row.cells.empty() ? 0 : row.cells.back().right) + 144;
		row.cells.push_back(cell);
	}
	if ((int)row.cells.size() > cells)
		row.cells.resize(cells);
	Paragraph end(fState.cf);
	end.format = fState.pf;
	end.format.inTable = true;
	end.kind = kParaRowEnd;
	end.row = row;
	story.push_back(end);
	builder.paragraph = Paragraph(fState.cf);
}


void
RtfReader::_PushGroup()
{
	_Flush();
	fStack.push_back(fState);
	if (fState.dest == kDestStyleSheet) {
		fState.dest = kDestStyleEntry;
		fStyleEntry = StyleEntry();
		fState.cf = _PlainFormat();
		fState.pf = _PlainParaFormat();
	}
}


void
RtfReader::_FinishBuilder(int index, bool fragment, const ParaFormat& format)
{
	Builder& builder = fBuilders[index];
	if (!builder.inUse)
		return;
	Paragraph& pending = builder.paragraph;
	bool keep = fragment || !pending.text.empty() || builder.story->empty();
	if (keep) {
		pending.format = format;
		pending.format.inTable = false;
		pending.kind = kParaNormal;
		pending.Touch();
		builder.story->push_back(pending);
	}
	builder.inUse = false;
}


void
RtfReader::_PopGroup()
{
	_Flush();
	if (fStack.empty())
		return;
	ReaderState inner = fState;
	fState = fStack.back();
	fStack.pop_back();
	fSkipCount = 0;

	if (inner.dest == kDestStyleEntry && fState.dest == kDestStyleSheet) {
		StyleEntry entry = fStyleEntry;
		size_t marker = entry.name.find('\x01');
		if (marker != std::string::npos)
			entry.name.resize(marker);
		size_t comma = entry.name.find(',');
		if (comma != std::string::npos)
			entry.name.resize(comma);
		while (!entry.name.empty() && entry.name.back() == ' ')
			entry.name.pop_back();
		size_t start = entry.name.find_first_not_of(' ');
		entry.name = start == std::string::npos ? "" : entry.name.substr(start);
		entry.cf = inner.cf;
		entry.pf = inner.pf;
		if (!entry.name.empty() && !entry.characterStyle)
			fStyleEntries.push_back(entry);
	}
	if (inner.dest == kDestStyleSheet && fState.dest != kDestStyleSheet)
		_FinishStyleSheet();
	if (inner.dest == kDestPicture && fState.dest != kDestPicture)
		_FinishPicture();
	if (inner.dest == kDestInfoText && fState.dest != kDestInfoText) {
		DocInfo& info = fDocument.info;
		switch (inner.infoField) {
			case 1: info.title = fInfoText; break;
			case 2: info.subject = fInfoText; break;
			case 3: info.author = fInfoText; break;
			case 4: info.keywords = fInfoText; break;
			case 5: info.comments = fInfoText; break;
			case 6: info.version = fInfoText; break;
		}
		fInfoText.clear();
	}
	if (inner.dest == kDestInfoTime && fState.dest != kDestInfoTime) {
		struct tm parts = {};
		parts.tm_year = fTimeParts[0] - 1900;
		parts.tm_mon = fTimeParts[1] - 1;
		parts.tm_mday = fTimeParts[2];
		parts.tm_hour = fTimeParts[3];
		parts.tm_min = fTimeParts[4];
		parts.tm_isdst = -1;
		time_t value = fTimeParts[0] > 0 ? mktime(&parts) : 0;
		if (inner.infoField == 7)
			fDocument.info.created = value;
		else if (inner.infoField == 8)
			fDocument.info.revised = value;
		memset(fTimeParts, 0, sizeof(fTimeParts));
	}
	if (inner.dest == kDestQuillExtension && fState.dest != kDestQuillExtension) {
		if (inner.infoField == 20)
			fDocument.settings.scale = atoi(fExtension.c_str());
		else if (inner.infoField == 21)
			fDocument.info.version = fExtension;
		fExtension.clear();
	}
	if (inner.builder != fState.builder)
		_FinishBuilder(inner.builder, false, inner.pf);
}


bool
RtfReader::_Destination(const std::string& word)
{
	static const char* kSkipped[] = { "pn", "listtable", "listoverridetable",
		"rsidtbl", "generator", "themedata", "colorschememapping",
		"latentstyles", "datastore", "xmlnstbl", "mmathPr", "pgdsctbl",
		"bkmkstart", "bkmkend", "xe", "tc", "txe", "annotation", "atnid",
		"atnauthor", "atrfstart", "atrfend", "objdata", "shpinst", "sp", "sn",
		"sv", "nonshppict", "do", "template", "operator", "company",
		"manager", "category", "hlinkbase", "printim", "buptim", "edmins",
		"nofpages", "nofwords", "nofchars", "nofcharsws", "id", "userprops",
		"docvar", "fchars", "lchars", "ftnsep", "ftnsepc", "ftncn",
		"aftnsep", "aftnsepc", "aftncn", "revtbl", "headerl", "footerl",
		"filetbl", "panose", "falt", "fname", "file", "blipuid", "comment",
		"wgrffmtfilter", "listpicture", "pnseclvl", "background", "ud",
		"mmath", "factoidname", "pgp", "rsid", "oldcprops", "oldpprops",
		"oldtprops", "oldsprops", "trackchange", "protusertbl", "fldtype",
		"datafield", "formfield", "ffname", "ffdeftext", "fontemb",
		"fontfile", "keycode", "macro", "leveltext", "levelnumbers",
		"picprop", "dptxbxtext", "objclass", "objname", "objtime", nullptr };
	for (int i = 0; kSkipped[i]; i++) {
		if (word == kSkipped[i]) {
			fState.dest = kDestSkip;
			return true;
		}
	}
	return false;
}


void
RtfReader::_FinishStyleSheet()
{
	if (fStyleEntries.empty())
		return;
	StyleSheet defaults = StyleSheet::Default(gDefaultSerif, gDefaultSans);
	StyleSheet sheet;
	fStyleMap.clear();

	// Normal comes first.
	int normal = -1;
	for (size_t i = 0; i < fStyleEntries.size(); i++) {
		if (fStyleEntries[i].number == 0
			|| strcasecmp(fStyleEntries[i].name.c_str(), "Normal") == 0) {
			normal = (int)i;
			break;
		}
	}
	std::vector<int> order;
	if (normal >= 0)
		order.push_back(normal);
	for (size_t i = 0; i < fStyleEntries.size(); i++) {
		if ((int)i != normal)
			order.push_back((int)i);
	}
	if (normal < 0)
		sheet.styles.push_back(defaults.styles[0]);
	for (int index : order) {
		const StyleEntry& entry = fStyleEntries[index];
		if (sheet.Find(entry.name) >= 0)
			continue;
		Style style;
		style.name = entry.name;
		style.cf = entry.cf;
		style.pf = entry.pf;
		int defaultIndex = defaults.Find(entry.name);
		if (defaultIndex >= 0) {
			style.automatic = defaults.styles[defaultIndex].automatic;
			style.outlineLevel = defaults.styles[defaultIndex].outlineLevel;
		}
		if (strncasecmp(entry.name.c_str(), "heading ", 8) == 0)
			style.outlineLevel = atoi(entry.name.c_str() + 8);
		fStyleMap[entry.number] = sheet.Count();
		sheet.styles.push_back(style);
	}
	if (normal >= 0)
		sheet.styles[0].name = "Normal";
	for (size_t i = 0; i < order.size(); i++) {
		const StyleEntry& entry = fStyleEntries[order[i]];
		auto self = fStyleMap.find(entry.number);
		if (self == fStyleMap.end())
			continue;
		Style& style = sheet.styles[self->second];
		auto based = fStyleMap.find(entry.basedOn);
		if (entry.basedOn == 222 || self->second == 0)
			style.basedOn = -1;
		else
			style.basedOn = based != fStyleMap.end()
				&& based->second != self->second ? based->second : 0;
		auto next = fStyleMap.find(entry.next);
		style.next = next != fStyleMap.end() ? next->second : -1;
		style.pf.style = self->second;
	}
	// Keep the standard styles Quill relies on.
	for (const Style& style : defaults.styles) {
		if (sheet.Find(style.name) < 0) {
			Style copy = style;
			copy.pf.style = sheet.Count();
			sheet.styles.push_back(copy);
		}
	}
	fDocument.styles = sheet;
	fHaveStyleSheet = true;
}


static std::string
HexDecode(const std::string& hex)
{
	std::string data;
	data.reserve(hex.size() / 2);
	int high = -1;
	for (char c : hex) {
		int value;
		if (c >= '0' && c <= '9') value = c - '0';
		else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
		else continue;
		if (high < 0)
			high = value;
		else {
			data += (char)((high << 4) | value);
			high = -1;
		}
	}
	return data;
}


void
RtfReader::_FinishPicture()
{
	auto picture = std::make_shared<Picture>();
	picture->type = fPicture.type;
	if (picture->type == "png" || picture->type == "jpeg") {
		picture->data = fPicture.binary.empty() ? HexDecode(fPicture.hex)
			: fPicture.binary;
	} else
		picture->type.clear();
	picture->pixelWidth = fPicture.pixelWidth;
	picture->pixelHeight = fPicture.pixelHeight;
	float width = fPicture.goalWidth > 0 ? FromTwips(fPicture.goalWidth)
		: fPicture.pixelWidth * 0.75f;
	float height = fPicture.goalHeight > 0 ? FromTwips(fPicture.goalHeight)
		: fPicture.pixelHeight * 0.75f;
	picture->width = std::max(4.0f, width * fPicture.scaleX / 100.0f);
	picture->height = std::max(4.0f, height * fPicture.scaleY / 100.0f);
	fPicture = PictureRecord();

	if (fState.dest != kDestText)
		return;
	CharFormat format = fState.cf;
	format.picture = fDocument.AddPicture(picture);
	_InsertSpecial(kPictureChar, &format);
}


void
RtfReader::_ApplyBorder(const std::function<void(uint8_t&)>& modify)
{
	if (fState.cellBorder) {
		if (fState.borderTarget & BorderTarget::kTop) modify(fCell.top);
		if (fState.borderTarget & BorderTarget::kBottom) modify(fCell.bottom);
		if (fState.borderTarget & BorderTarget::kLeft) modify(fCell.left);
		if (fState.borderTarget & BorderTarget::kRight) modify(fCell.rightLine);
		return;
	}
	ParaBorder& border = fState.pf.border;
	if (fState.borderTarget & BorderTarget::kTop) modify(border.top);
	if (fState.borderTarget & BorderTarget::kBottom) modify(border.bottom);
	if (fState.borderTarget & BorderTarget::kLeft) modify(border.left);
	if (fState.borderTarget & BorderTarget::kRight) modify(border.right);
	if (fState.borderTarget & BorderTarget::kBetween) modify(border.between);
}


void
RtfReader::_SetBorderLine(uint8_t line)
{
	_ApplyBorder([line](uint8_t& value) { value = line; });
}


void
RtfReader::_ControlSymbol(char c)
{
	switch (c) {
		case '\\': case '{': case '}':
			_Text((unsigned char)c);
			break;
		case '~':
			_Text(kNbspChar);
			break;
		case '-':
			_Text(kSoftHyphenChar);
			break;
		case '_':
			_Text(kNbHyphenChar);
			break;
		case '*':
			fIgnorable = true;
			break;
		case '\n': case '\r':
			_EndParagraph();
			break;
		case '\'': {
			if (fPos + 2 <= fData.size()) {
				char hex[3] = { fData[fPos], fData[fPos + 1], 0 };
				fPos += 2;
				unsigned char value = (unsigned char)strtol(hex, nullptr, 16);
				if (fState.dest == kDestPicture)
					break;
				_Byte(value);
			}
			break;
		}
		default:
			break;
	}
}


void
RtfReader::_ControlWord(const std::string& word, bool hasParam, int param)
{
	bool ignorable = fIgnorable;
	fIgnorable = false;
	ReaderState& s = fState;
	CharFormat& cf = s.cf;
	ParaFormat& pf = s.pf;
	DocSettings& settings = fDocument.settings;
	int on = hasParam ? param : 1;

	// Destinations
	if (word == "fonttbl") { s.dest = kDestFontTable; return; }
	if (word == "colortbl") {
		s.dest = kDestColorTable;
		fColors.clear();
		return;
	}
	if (word == "stylesheet") {
		s.dest = kDestStyleSheet;
		fStyleEntries.clear();
		return;
	}
	if (word == "info") { s.dest = kDestInfo; return; }
	if (s.dest == kDestInfo || s.dest == kDestInfoTime
		|| s.dest == kDestInfoText) {
		static const char* kInfo[] = { "", "title", "subject", "author",
			"keywords", "doccomm", nullptr };
		for (int i = 1; kInfo[i]; i++) {
			if (word == kInfo[i]) {
				s.dest = kDestInfoText;
				s.infoField = i;
				fInfoText.clear();
				return;
			}
		}
		if (word == "creatim" || word == "revtim") {
			s.dest = kDestInfoTime;
			s.infoField = word == "creatim" ? 7 : 8;
			return;
		}
		if (word == "yr") { fTimeParts[0] = param; return; }
		if (word == "mo") { fTimeParts[1] = param; return; }
		if (word == "dy") { fTimeParts[2] = param; return; }
		if (word == "hr") { fTimeParts[3] = param; return; }
		if (word == "min") { fTimeParts[4] = param; return; }
		if (word == "version") {
			fDocument.info.revisions = param;
			return;
		}
		if (word == "quillversion") {
			s.dest = kDestQuillExtension;
			s.infoField = 21;
			return;
		}
		if (ignorable || word == "operator" || word == "company"
			|| word == "manager" || word == "category" || word == "comment"
			|| word == "printim" || word == "buptim" || word == "nofpages"
			|| word == "nofwords" || word == "nofchars" || word == "edmins"
			|| word == "vern" || word == "id" || word == "hlinkbase") {
			s.dest = kDestSkip;
			return;
		}
	}
	if (word == "quillscale") {
		s.dest = kDestQuillExtension;
		s.infoField = 20;
		return;
	}
	if (word == "header" || word == "headerr" || word == "footer"
		|| word == "footerr" || word == "headerf" || word == "footerf") {
		_Flush();
		Story* story;
		if (word == "headerf") story = &fDocument.firstHeader;
		else if (word == "footerf") story = &fDocument.firstFooter;
		else if (word[0] == 'h') story = &fDocument.header;
		else story = &fDocument.footer;
		s.builder = _NewBuilder(story);
		s.dest = kDestText;
		s.pf = _PlainParaFormat();
		s.cf = _PlainFormat();
		return;
	}
	if (word == "footnote") {
		_Flush();
		int32_t id = fPendingFootnote;
		fPendingFootnote = 0;
		if (id == 0) {
			id = fDocument.nextFootnoteId++;
			CharFormat format = cf;
			format.footnote = id;
			format.position = 3;
			_InsertSpecial(kFootnoteChar, &format);
		}
		Story& story = fDocument.footnotes[id];
		s.builder = _NewBuilder(&story);
		s.footnoteId = id;
		s.dest = kDestText;
		s.pf = _PlainParaFormat();
		s.cf = _PlainFormat();
		return;
	}
	if (word == "pict") {
		_Flush();
		s.dest = kDestPicture;
		fPicture = PictureRecord();
		return;
	}
	if (s.dest == kDestPicture) {
		if (word == "pngblip") fPicture.type = "png";
		else if (word == "jpegblip") fPicture.type = "jpeg";
		else if (word == "macpict") fPicture.type = "pict";
		else if (word == "wmetafile") fPicture.type = "wmf";
		else if (word == "emfblip") fPicture.type = "emf";
		else if (word == "picw") fPicture.pixelWidth = param;
		else if (word == "pich") fPicture.pixelHeight = param;
		else if (word == "picwgoal") fPicture.goalWidth = param;
		else if (word == "pichgoal") fPicture.goalHeight = param;
		else if (word == "picscalex") fPicture.scaleX = std::max(1, param);
		else if (word == "picscaley") fPicture.scaleY = std::max(1, param);
		else if (word == "bin" && param > 0) {
			size_t count = std::min<size_t>(param, fData.size() - fPos);
			fPicture.binary.append(fData, fPos, count);
			fPos += count;
		}
		return;
	}
	if (word == "field") {
		fFields.push_back(FieldRecord());
		s.field = (int)fFields.size() - 1;
		return;
	}
	if (word == "fldinst") {
		s.dest = kDestFieldInstruction;
		return;
	}
	if (word == "fldrslt") {
		if (s.field >= 0 && s.field < (int)fFields.size()) {
			std::string instruction = fFields[s.field].instruction;
			size_t start = instruction.find_first_not_of(" ");
			instruction = start == std::string::npos ? ""
				: instruction.substr(start);
			std::string name;
			for (char c : instruction) {
				if (c == ' ' || c == '\\')
					break;
				name += (char)toupper((unsigned char)c);
			}
			uint32_t special = 0;
			if (name == "PAGE") special = kFieldPageChar;
			else if (name == "NUMPAGES") special = kFieldPagesChar;
			else if (name == "DATE" || name == "CREATEDATE"
				|| name == "SAVEDATE" || name == "PRINTDATE")
				special = kFieldDateChar;
			else if (name == "TIME") special = kFieldTimeChar;
			if (special != 0) {
				_InsertSpecial(special);
				s.dest = kDestSkip;
				return;
			}
		}
		s.dest = s.dest == kDestSkip ? kDestSkip : kDestText;
		return;
	}
	if (word == "pntext" || word == "listtext" || word == "shppict"
		|| word == "result") {
		return;
	}
	if (word == "upr") {
		s.beforeUprDest = s.dest;
		s.dest = kDestSkip;
		return;
	}
	if (word == "ud") {
		if (fStack.size() > 0 && fStack.back().beforeUprDest >= 0)
			s.dest = fStack.back().beforeUprDest;
		return;
	}
	if (word == "object") {
		return;
	}
	if (_Destination(word))
		return;
	if (ignorable) {
		s.dest = kDestSkip;
		return;
	}

	// Font and color tables
	if (s.dest == kDestFontTable) {
		if (word == "f") {
			fFontNumber = param;
			fFontName.clear();
		} else if (word == "fcharset") {
			if (param == 2)
				fFontCodepages[fFontNumber] = kCpSymbol;
			else if (param == 77)
				fFontCodepages[fFontNumber] = kCpMac;
		}
		return;
	}
	if (s.dest == kDestColorTable) {
		if (word == "red") { fRed = param; fColorSet = true; }
		else if (word == "green") { fGreen = param; fColorSet = true; }
		else if (word == "blue") { fBlue = param; fColorSet = true; }
		return;
	}
	if (s.dest == kDestStyleEntry) {
		if (word == "s") { fStyleEntry.number = param; return; }
		if (word == "cs" || word == "ds" || word == "ts") {
			fStyleEntry.characterStyle = true;
			return;
		}
		if (word == "sbasedon") { fStyleEntry.basedOn = param; return; }
		if (word == "snext") { fStyleEntry.next = param; return; }
	}

	// Document
	if (word == "rtf") return;
	if (word == "ansi") { fCodepage = kCp1252; return; }
	if (word == "mac") { fCodepage = kCpMac; return; }
	if (word == "pc" || word == "pca") { fCodepage = kCp437; return; }
	if (word == "ansicpg") {
		fCodepage = param == 10000 ? kCpMac
			: (param == 437 || param == 850) ? kCp437 : kCp1252;
		return;
	}
	if (word == "deff") { fDefaultFont = param; return; }
	if (word == "uc") { s.uc = std::max(0, param); return; }
	if (word == "u") {
		uint32_t c = (uint32_t)(param < 0 ? param + 65536 : param);
		if (c >= 0xD800 && c < 0xDC00) {
			fHighSurrogate = c;
		} else if (c >= 0xDC00 && c < 0xE000 && fHighSurrogate != 0) {
			uint32_t full = 0x10000 + ((fHighSurrogate - 0xD800) << 10)
				+ (c - 0xDC00);
			fHighSurrogate = 0;
			fSkipCount = 0;
			_Text(full);
		} else {
			fHighSurrogate = 0;
			fSkipCount = 0;
			_Text(c);
		}
		fSkipCount = s.uc;
		return;
	}
	if (word == "paperw") { settings.pageWidth = FromTwips(param); return; }
	if (word == "paperh") { settings.pageHeight = FromTwips(param); return; }
	if (word == "margl") { settings.marginLeft = FromTwips(param); return; }
	if (word == "margr") { settings.marginRight = FromTwips(param); return; }
	if (word == "margt") { settings.marginTop = FromTwips(param); return; }
	if (word == "margb") { settings.marginBottom = FromTwips(param); return; }
	if (word == "gutter") { settings.gutter = FromTwips(param); return; }
	if (word == "landscape") { settings.landscape = true; return; }
	if (word == "margmirror") { settings.mirrorMargins = true; return; }
	if (word == "deftab") {
		settings.defaultTab = std::max(1.0f, FromTwips(param));
		return;
	}
	if (word == "widowctrl") { settings.widowControl = true; return; }
	if (word == "ftnbj") { settings.footnotePos = kFootnoteBottom; return; }
	if (word == "ftntj") { settings.footnotePos = kFootnoteBeneath; return; }
	if (word == "enddoc" || word == "endnotes") {
		settings.footnotePos = kFootnoteEndOfDocument;
		return;
	}
	if (word == "ftnstart") { settings.footnoteStart = param; return; }
	if (word == "ftnrstpg") {
		settings.footnoteRestartEachPage = true;
		return;
	}
	if (word == "sectd") return;
	if (word == "sbknone") { settings.sectionStart = kStartNoBreak; return; }
	if (word == "sbkcol") { settings.sectionStart = kStartNewColumn; return; }
	if (word == "sbkpage") { settings.sectionStart = kStartNewPage; return; }
	if (word == "sbkeven") { settings.sectionStart = kStartEvenPage; return; }
	if (word == "sbkodd") { settings.sectionStart = kStartOddPage; return; }
	if (word == "cols") {
		settings.columns = std::max(1, std::min(param, 12));
		return;
	}
	if (word == "colsx") { settings.columnSpacing = FromTwips(param); return; }
	if (word == "headery") { settings.headerY = FromTwips(param); return; }
	if (word == "footery") { settings.footerY = FromTwips(param); return; }
	if (word == "titlepg") { settings.titlePage = true; return; }
	if (word == "pgnstarts") { settings.pageNumberStart = param; return; }
	if (word == "pgndec") { settings.pageNumberFormat = kPageArabic; return; }
	if (word == "pgnucrm") {
		settings.pageNumberFormat = kPageUpperRoman;
		return;
	}
	if (word == "pgnlcrm") {
		settings.pageNumberFormat = kPageLowerRoman;
		return;
	}
	if (word == "pgnucltr") {
		settings.pageNumberFormat = kPageUpperLetter;
		return;
	}
	if (word == "pgnlcltr") {
		settings.pageNumberFormat = kPageLowerLetter;
		return;
	}
	if (word == "pgnx") {
		settings.marginPageNumbers = true;
		settings.pageNumberRight = FromTwips(param);
		return;
	}
	if (word == "pgny") {
		settings.marginPageNumbers = true;
		settings.pageNumberTop = FromTwips(param);
		return;
	}
	if (word == "linemod") {
		if (param > 0 && settings.lineNumbering == 0)
			settings.lineNumbering = 1;
		return;
	}
	if (word == "linex") {
		settings.lineNumberDistance = FromTwips(param);
		return;
	}
	if (word == "lineppage" || word == "linerestart") {
		settings.lineNumbering = 1;
		return;
	}
	if (word == "linecont") { settings.lineNumbering = 2; return; }

	// Special characters
	if (word == "par") { _EndParagraph(); return; }
	if (word == "line") { _InsertSpecial(kLineBreakChar); return; }
	if (word == "tab") { _Text('\t'); return; }
	if (word == "page") { _InsertSpecial(kPageBreakChar); return; }
	if (word == "sect") { _InsertSpecial(kSectionBreakChar); return; }
	if (word == "cell") { _EndCell(); return; }
	if (word == "row") { _EndRow(); return; }
	if (word == "emdash") { _Text(0x2014); return; }
	if (word == "endash") { _Text(0x2013); return; }
	if (word == "emspace") { _Text(0x2003); return; }
	if (word == "enspace") { _Text(0x2002); return; }
	if (word == "bullet") { _Text(0x2022); return; }
	if (word == "lquote") { _Text(0x2018); return; }
	if (word == "rquote") { _Text(0x2019); return; }
	if (word == "ldblquote") { _Text(0x201C); return; }
	if (word == "rdblquote") { _Text(0x201D); return; }
	if (word == "chpgn") { _InsertSpecial(kFieldPageChar); return; }
	if (word == "chdate" || word == "chdpl" || word == "chdpa") {
		_InsertSpecial(kFieldDateChar);
		return;
	}
	if (word == "chtime") { _InsertSpecial(kFieldTimeChar); return; }
	if (word == "chftn") {
		CharFormat format = cf;
		if (s.footnoteId != 0) {
			format.footnote = s.footnoteId;
		} else {
			if (fPendingFootnote == 0)
				fPendingFootnote = fDocument.nextFootnoteId++;
			format.footnote = fPendingFootnote;
		}
		_InsertSpecial(kFootnoteChar, &format);
		return;
	}

	// Character formatting
	if (word == "plain") {
		int32_t picture = cf.picture;
		cf = _PlainFormat();
		cf.picture = picture;
		s.fontCodepage = kCp1252;
		auto it = fFontCodepages.find(fDefaultFont);
		if (it != fFontCodepages.end())
			s.fontCodepage = it->second;
		return;
	}
	if (word == "f") {
		auto it = fFonts.find(param);
		if (it != fFonts.end())
			cf.font = it->second;
		auto codepage = fFontCodepages.find(param);
		s.fontCodepage = codepage != fFontCodepages.end()
			? codepage->second : kCp1252;
		return;
	}
	if (word == "fs") { cf.size = std::max(1, param) / 2.0f; return; }
	if (word == "b") { cf.bold = on != 0; return; }
	if (word == "i") { cf.italic = on != 0; return; }
	if (word == "ul" || word == "ulth" || word == "uldash"
		|| word == "uldashd" || word == "uldashdd" || word == "ulwave"
		|| word == "ulhwave" || word == "ulthd" || word == "ulthdash") {
		cf.underline = on ? kULSingle : kULNone;
		return;
	}
	if (word == "ulnone") { cf.underline = kULNone; return; }
	if (word == "ulw") { cf.underline = on ? kULWord : kULNone; return; }
	if (word == "uldb") { cf.underline = on ? kULDouble : kULNone; return; }
	if (word == "uld") { cf.underline = on ? kULDotted : kULNone; return; }
	if (word == "strike" || word == "striked") {
		cf.strike = on != 0;
		return;
	}
	if (word == "outl") { cf.outline = on != 0; return; }
	if (word == "shad") { cf.shadow = on != 0; return; }
	if (word == "scaps") { cf.smallCaps = on != 0; return; }
	if (word == "caps") { cf.allCaps = on != 0; return; }
	if (word == "v") { cf.hidden = on != 0; return; }
	if (word == "up") { cf.position = (hasParam ? param : 6) / 2.0f; return; }
	if (word == "dn") { cf.position = -(hasParam ? param : 6) / 2.0f; return; }
	if (word == "super") {
		cf.position = std::max(2.0f, cf.size / 3);
		cf.size = NextSmallerSize(cf.size);
		return;
	}
	if (word == "sub") {
		cf.position = -std::max(2.0f, cf.size / 4);
		cf.size = NextSmallerSize(cf.size);
		return;
	}
	if (word == "nosupersub") { cf.position = 0; return; }
	if (word == "expnd") { cf.spacing = param / 4.0f; return; }
	if (word == "expndtw") { cf.spacing = FromTwips(param); return; }
	if (word == "cf") {
		// index 0 of the color table is the empty "auto" entry
		cf.color = param > 0 && param < (int)fColors.size()
			? fColors[param] : kColorAuto;
		return;
	}

	// Paragraph formatting
	if (word == "pard") {
		pf = _PlainParaFormat();
		s.borderTarget = 0;
		s.cellBorder = false;
		return;
	}
	if (word == "s") {
		auto it = fStyleMap.find(param);
		pf.style = it != fStyleMap.end() ? it->second : 0;
		return;
	}
	if (word == "ql") { pf.align = kAlignLeft; return; }
	if (word == "qc") { pf.align = kAlignCenter; return; }
	if (word == "qr") { pf.align = kAlignRight; return; }
	if (word == "qj" || word == "qd") { pf.align = kAlignJustify; return; }
	if (word == "li") { pf.leftIndent = FromTwips(param); return; }
	if (word == "ri") { pf.rightIndent = FromTwips(param); return; }
	if (word == "fi") { pf.firstIndent = FromTwips(param); return; }
	if (word == "sb") { pf.spaceBefore = FromTwips(param); return; }
	if (word == "sa") { pf.spaceAfter = FromTwips(param); return; }
	if (word == "sl") {
		if (param == 0) {
			pf.lineRule = kLineAuto;
			pf.lineValue = 1;
		} else if (param > 0) {
			pf.lineRule = kLineAtLeast;
			pf.lineValue = FromTwips(param);
		} else {
			pf.lineRule = kLineExactly;
			pf.lineValue = FromTwips(-param);
		}
		return;
	}
	if (word == "slmult") {
		if (param == 1 && pf.lineRule == kLineAtLeast) {
			pf.lineRule = kLineAuto;
			pf.lineValue = pf.lineValue * 20 / 240.0f;
		}
		return;
	}
	if (word == "keepn") { pf.keepWithNext = on != 0; return; }
	if (word == "keep") { pf.keepTogether = on != 0; return; }
	if (word == "pagebb") { pf.pageBreakBefore = on != 0; return; }
	if (word == "noline") { pf.noLineNumbers = true; return; }
	if (word == "intbl") { pf.inTable = true; return; }
	if (word == "tqr") { fPendingTab.kind = kTabRight; return; }
	if (word == "tqc") { fPendingTab.kind = kTabCenter; return; }
	if (word == "tqdec") { fPendingTab.kind = kTabDecimal; return; }
	if (word == "tldot" || word == "tlmdot") {
		fPendingTab.leader = kLeaderDot;
		return;
	}
	if (word == "tlhyph") { fPendingTab.leader = kLeaderHyphen; return; }
	if (word == "tlul" || word == "tlth") {
		fPendingTab.leader = kLeaderLine;
		return;
	}
	if (word == "tx" || word == "tb") {
		TabStop tab = fPendingTab;
		tab.pos = FromTwips(param);
		if (word == "tb")
			tab.kind = kTabBar;
		pf.SetTab(tab);
		fPendingTab = TabStop();
		return;
	}
	if (word == "brdrt") { s.borderTarget = BorderTarget::kTop; s.cellBorder = false; return; }
	if (word == "brdrb") { s.borderTarget = BorderTarget::kBottom; s.cellBorder = false; return; }
	if (word == "brdrl") { s.borderTarget = BorderTarget::kLeft; s.cellBorder = false; return; }
	if (word == "brdrr") { s.borderTarget = BorderTarget::kRight; s.cellBorder = false; return; }
	if (word == "brdrbtw") { s.borderTarget = BorderTarget::kBetween; s.cellBorder = false; return; }
	if (word == "box") {
		s.borderTarget = BorderTarget::kTop | BorderTarget::kBottom
			| BorderTarget::kLeft | BorderTarget::kRight;
		s.cellBorder = false;
		return;
	}
	if (word == "clbrdrt") { s.borderTarget = BorderTarget::kTop; s.cellBorder = true; return; }
	if (word == "clbrdrb") { s.borderTarget = BorderTarget::kBottom; s.cellBorder = true; return; }
	if (word == "clbrdrl") { s.borderTarget = BorderTarget::kLeft; s.cellBorder = true; return; }
	if (word == "clbrdrr") { s.borderTarget = BorderTarget::kRight; s.cellBorder = true; return; }
	if (word == "brdrs" || word == "brdrdash" || word == "brdrengrave"
		|| word == "brdremboss" || word == "brdroutset" || word == "brdrinset"
		|| word == "brdrframe") {
		_SetBorderLine(kBorderSingle);
		return;
	}
	if (word == "brdrth" || word == "brdrthtnsg" || word == "brdrtnthsg") {
		_SetBorderLine(kBorderThick);
		return;
	}
	if (word == "brdrdb" || word == "brdrtriple") {
		_SetBorderLine(kBorderDouble);
		return;
	}
	if (word == "brdrdot") { _SetBorderLine(kBorderDotted); return; }
	if (word == "brdrhair") { _SetBorderLine(kBorderHairline); return; }
	if (word == "brdrnone" || word == "brdrnil") {
		_SetBorderLine(kBorderNone);
		return;
	}
	if (word == "brdrw") {
		if (param >= 30 && !s.cellBorder) {
			_ApplyBorder([](uint8_t& value) {
				if (value == kBorderSingle)
					value = kBorderThick;
			});
		}
		return;
	}
	if (word == "brdrsh") {
		if (!s.cellBorder)
			pf.border.shadow = true;
		return;
	}
	if (word == "brsp") {
		if (!s.cellBorder)
			pf.border.space = FromTwips(param);
		return;
	}
	if (word == "shading") {
		pf.border.shading = (uint8_t)std::min(100, std::max(0, param / 100));
		return;
	}

	// Tables
	if (word == "trowd") {
		fRow = RowFormat();
		fRow.gap = 0;
		fCell = CellFormat();
		return;
	}
	if (word == "trgaph") { fRow.gap = FromTwips(param); return; }
	if (word == "trleft") { fRow.leftEdge = FromTwips(param); return; }
	if (word == "trrh") { fRow.height = FromTwips(param); return; }
	if (word == "trqc") { fRow.align = kAlignCenter; return; }
	if (word == "trqr") { fRow.align = kAlignRight; return; }
	if (word == "clshdng") {
		fCell.shading = (uint8_t)std::min(100, std::max(0, param / 100));
		return;
	}
	if (word == "cellx") {
		CellFormat cell = fCell;
		cell.right = FromTwips(param) - fRow.leftEdge;
		if (!fRow.cells.empty() && cell.right <= fRow.cells.back().right)
			cell.right = fRow.cells.back().right + 18;
		fRow.cells.push_back(cell);
		fCell = CellFormat();
		s.borderTarget = 0;
		s.cellBorder = false;
		return;
	}
	// Anything else is ignored.
}


bool
RtfReader::Parse(std::string* error)
{
	if (fData.compare(0, 5, "{\\rtf") != 0) {
		if (error)
			*error = "The file is not in Rich Text Format.";
		return false;
	}
	fFonts.clear();
	fState = ReaderState();
	fState.cf = _PlainFormat();
	fState.pf = _PlainParaFormat();

	if (fFragmentMode) {
		fState.builder = _NewBuilder(&fFragment);
	} else {
		fDocument.Reset(gDefaultSerif, gDefaultSans);
		fState.builder = _NewBuilder(&fDocument.body);
	}

	size_t length = fData.size();
	while (fPos < length) {
		char c = fData[fPos++];
		switch (c) {
			case '{':
				_PushGroup();
				break;
			case '}':
				_PopGroup();
				if (fStack.empty())
					fPos = length;	// end of the document
				break;
			case '\\': {
				if (fPos >= length)
					break;
				char next = fData[fPos];
				if (isalpha((unsigned char)next)) {
					size_t start = fPos;
					while (fPos < length && isalpha((unsigned char)fData[fPos]))
						fPos++;
					std::string word = fData.substr(start, fPos - start);
					bool hasParam = false;
					int param = 0;
					bool negative = false;
					if (fPos < length && fData[fPos] == '-') {
						negative = true;
						fPos++;
					}
					long value = 0;
					while (fPos < length && isdigit((unsigned char)fData[fPos])) {
						hasParam = true;
						if (value < 100000000)
							value = value * 10 + (fData[fPos] - '0');
						fPos++;
					}
					param = (int)(negative ? -value : value);
					if (fPos < length && fData[fPos] == ' ')
						fPos++;
					_Flush();
					if (fSkipCount > 0 && word != "u") {
						// the fallback after \uN may be a control word
						fSkipCount--;
						if (word == "lquote" || word == "rquote"
							|| word == "ldblquote" || word == "rdblquote"
							|| word == "bullet" || word == "emdash"
							|| word == "endash" || word == "tab"
							|| word == "emspace" || word == "enspace")
							break;
					}
					_ControlWord(word, hasParam, param);
				} else {
					fPos++;
					_ControlSymbol(next);
				}
				break;
			}
			case '\r':
			case '\n':
				break;
			default:
				if (fState.dest == kDestPicture) {
					fPicture.hex += c;
					break;
				}
				if ((unsigned char)c >= 0x80) {
					// Raw 8-bit text: UTF-8 if valid, else the code page.
					int32_t count = Utf8CharLength((unsigned char)c);
					if (count > 1 && fPos - 1 + count <= length
						&& IsValidUtf8(fData.c_str() + fPos - 1, count)) {
						int32_t consumed;
						uint32_t u = DecodeUtf8(fData.c_str() + fPos - 1,
							count, &consumed);
						fPos += count - 1;
						_Text(u);
					} else
						_Byte((unsigned char)c);
				} else
					_Text((unsigned char)c);
				break;
		}
	}
	_Flush();

	// Close whatever is still open.
	while (!fStack.empty())
		_PopGroup();
	for (size_t i = 0; i < fBuilders.size(); i++) {
		bool fragment = fFragmentMode && i == 0;
		_FinishBuilder((int)i, fragment, fState.pf);
	}

	if (!fFragmentMode) {
		Story& body = fDocument.body;
		if (body.empty())
			body.push_back(fDocument.NewParagraph(0));
		// A final section break is just the end of the last section.
		Paragraph& last = body.back();
		if (!last.text.empty()
			&& CharAt(last.text, PrevOffset(last.text, last.Length()))
				== kSectionBreakChar) {
			last.Erase(PrevOffset(last.text, last.Length()), last.Length());
			if (last.text.empty() && body.size() > 1)
				body.pop_back();
		}
		const char* storyStyles[] = { "header", "footer", "header", "footer" };
		Story* stories[] = { &fDocument.header, &fDocument.footer,
			&fDocument.firstHeader, &fDocument.firstFooter };
		for (int i = 0; i < 4; i++) {
			if (stories[i]->empty()) {
				*stories[i] = fDocument.NewStory(std::max<int32_t>(0,
					fDocument.styles.Find(storyStyles[i])));
			}
		}
		for (auto& entry : fDocument.footnotes) {
			if (entry.second.empty())
				entry.second.push_back(fDocument.NewParagraph(0));
			if (entry.first >= fDocument.nextFootnoteId)
				fDocument.nextFootnoteId = entry.first + 1;
		}
		if (fDocument.settings.pageWidth > fDocument.settings.pageHeight
			&& fDocument.settings.landscape) {
			std::swap(fDocument.settings.pageWidth,
				fDocument.settings.pageHeight);
		}
	} else if (fFragment.empty())
		fFragment.push_back(Paragraph(_PlainFormat()));

	// Fix style indices that point past the style sheet.
	auto fixStory = [this](Story& story) {
		for (Paragraph& paragraph : story) {
			if (paragraph.format.style < 0
				|| paragraph.format.style >= fDocument.styles.Count())
				paragraph.format.style = 0;
		}
	};
	fixStory(fDocument.body);
	fixStory(fFragment);
	return true;
}

}	// namespace


bool
ReadRTF(const std::string& data, Document& document, std::string* error)
{
	RtfReader reader(data, document, false);
	return reader.Parse(error);
}


bool
ReadRTFFragment(const std::string& data, Document& scratch, Story& fragment)
{
	scratch.Reset(gDefaultSerif, gDefaultSans);
	RtfReader reader(data, scratch, true);
	if (!reader.Parse(nullptr))
		return false;
	fragment = reader.Fragment();
	return true;
}


// #pragma mark - import


namespace {

struct ImportMaps {
	std::map<int32_t, int32_t> styles;
	std::map<int32_t, int32_t> pictures;
	std::map<int32_t, int32_t> footnotes;
};


Story
ImportStoryWith(Document& target, const Document& source, const Story& story,
	ImportMaps& maps)
{
	Story result = story;
	for (Paragraph& paragraph : result) {
		int32_t style = paragraph.format.style;
		auto known = maps.styles.find(style);
		if (known == maps.styles.end()) {
			const Style& sourceStyle = source.styles.Get(style);
			int32_t index = target.styles.Find(sourceStyle.name);
			if (index < 0) {
				Style copy = sourceStyle;
				copy.basedOn = 0;
				copy.next = -1;
				index = target.styles.Count();
				copy.pf.style = index;
				target.styles.styles.push_back(copy);
			}
			known = maps.styles.insert(std::make_pair(style, index)).first;
		}
		paragraph.format.style = known->second;
		for (Run& run : paragraph.runs) {
			if (run.format.picture != 0) {
				auto mapped = maps.pictures.find(run.format.picture);
				if (mapped == maps.pictures.end()) {
					auto it = source.pictures.find(run.format.picture);
					int32_t id = 0;
					if (it != source.pictures.end())
						id = target.AddPicture(it->second);
					mapped = maps.pictures.insert(std::make_pair(
						run.format.picture, id)).first;
				}
				run.format.picture = mapped->second;
			}
			if (run.format.footnote != 0) {
				int32_t old = run.format.footnote;
				auto mapped = maps.footnotes.find(old);
				if (mapped == maps.footnotes.end()) {
					int32_t id = target.nextFootnoteId++;
					// map first: the footnote text refers to itself
					mapped = maps.footnotes.insert(std::make_pair(old, id)).first;
					auto it = source.footnotes.find(old);
					if (it != source.footnotes.end())
						target.footnotes[id] = ImportStoryWith(target, source,
							it->second, maps);
					else
						target.footnotes[id] = target.NewStory(0);
				}
				run.format.footnote = mapped->second;
			}
		}
		paragraph.Touch();
	}
	return result;
}

}	// namespace


Story
ImportStory(Document& target, const Document& source, const Story& story)
{
	ImportMaps maps;
	return ImportStoryWith(target, source, story, maps);
}

}	// namespace quill
