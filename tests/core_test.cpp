/*
 * Quill - unit tests for the portable core. Build and run on any POSIX
 * system (tests/run-core-tests.sh builds them with AddressSanitizer).
 */
#include "../src/core/Document.h"
#include "../src/core/Editor.h"
#include "../src/core/Layout.h"
#include "../src/core/RTF.h"
#include "../src/core/Search.h"
#include "../src/core/Spell.h"
#include "../src/core/Text.h"
#include "../src/core/TextIO.h"
#include "../src/core/Tools.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace quill;

static int sFailures = 0;
static int sChecks = 0;

#define CHECK(condition) do { \
	sChecks++; \
	if (!(condition)) { \
		sFailures++; \
		fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
			#condition); \
	} \
} while (0)

#define CHECK_EQ(a, b) do { \
	sChecks++; \
	auto _va = (a); \
	auto _vb = (b); \
	if (!(_va == _vb)) { \
		sFailures++; \
		fprintf(stderr, "%s:%d: CHECK_EQ failed: %s != %s\n", __FILE__, \
			__LINE__, #a, #b); \
	} \
} while (0)

#define CHECK_STR(a, b) do { \
	sChecks++; \
	std::string _va = (a); \
	std::string _vb = (b); \
	if (_va != _vb) { \
		sFailures++; \
		fprintf(stderr, "%s:%d: CHECK_STR failed:\n  got:      \"%s\"\n" \
			"  expected: \"%s\"\n", __FILE__, __LINE__, _va.c_str(), \
			_vb.c_str()); \
	} \
} while (0)


// Every character is half its point size wide; bold adds 10%.
class FakeMetrics : public FontMetrics {
public:
	void Widths(const CharFormat& format, float size, const char* text,
		int32_t bytes, int32_t count, float* widths) override
	{
		(void)text;
		(void)bytes;
		for (int32_t i = 0; i < count; i++)
			widths[i] = size * 0.5f * (format.bold ? 1.1f : 1.0f);
	}
	void Heights(const CharFormat& format, float size, float* ascent,
		float* descent, float* leading) override
	{
		(void)format;
		*ascent = size * 0.8f;
		*descent = size * 0.2f;
		*leading = 0;
	}
};


static std::string
Text(const Story& story)
{
	return Document::PlainText(story);
}


static void
TestParagraph()
{
	CharFormat plain;
	plain.font = "Serif";
	CharFormat bold = plain;
	bold.bold = true;

	Paragraph p(plain);
	p.Insert(0, "Hello world", plain);
	CHECK_EQ(p.runs.size(), (size_t)1);
	p.ApplyFormat(6, 11, [](CharFormat& f) { f.bold = true; });
	CHECK_EQ(p.runs.size(), (size_t)2);
	CHECK_EQ(p.runs[1].start, 6);
	CHECK(p.FormatAt(7).bold);
	CHECK(!p.FormatAt(2).bold);

	p.Insert(6, "big ", bold);
	CHECK_STR(p.text, "Hello big world");
	CHECK_EQ(p.runs.size(), (size_t)2);	// merged with following bold run

	p.Erase(3, 8);
	CHECK_STR(p.text, "Helg world");
	CHECK(p.FormatAt(3).bold);
	CHECK(!p.FormatAt(2).bold);

	Paragraph tail = p.SplitAt(4);
	CHECK_STR(p.text, "Helg");
	CHECK_STR(tail.text, " world");
	CHECK(tail.FormatAt(0).bold);
	p.Append(tail);
	CHECK_STR(p.text, "Helg world");
	CHECK_EQ(p.runs.size(), (size_t)2);

	Paragraph slice = p.Slice(2, 6);
	CHECK_STR(slice.text, "lg w");
	CHECK_EQ(slice.runs.size(), (size_t)2);
	CHECK_EQ(slice.runs[1].start, 1);

	p.Erase(0, p.Length());
	CHECK(p.text.empty());
	CHECK_EQ(p.runs.size(), (size_t)1);

	// UTF-8
	Paragraph u(plain);
	u.Insert(0, "na\xC3\xAFve caf\xC3\xA9", plain);
	CHECK_EQ(CountChars(u.text), 10);
	CHECK_EQ(NextOffset(u.text, 2), 4);
	CHECK_EQ(PrevOffset(u.text, 4), 2);
	CHECK_STR(ToUpperString("na\xC3\xAFve"), "NA\xC3\x8FVE");
}


static void
TestEditor()
{
	Document document;
	Editor editor(&document, &document.body);
	editor.InsertText("Hello");
	editor.InsertText(" world");
	CHECK_STR(Text(document.body), "Hello world");
	CHECK_EQ(editor.Caret().offset, 11);
	CHECK(editor.CanUndo());
	CHECK_STR(editor.UndoName(), "Typing");

	editor.Undo();
	CHECK_STR(Text(document.body), "");	// typing is one step
	editor.Redo();
	CHECK_STR(Text(document.body), "Hello world");

	editor.SetCaret(Pos(0, 5));
	editor.InsertParagraphBreak();
	CHECK_EQ(document.body.size(), (size_t)2);
	CHECK_STR(Text(document.body), "Hello\n world");
	editor.Backspace(false);
	CHECK_STR(Text(document.body), "Hello world");
	CHECK_EQ(document.body.size(), (size_t)1);

	// selection replace and format
	editor.Select(Pos(0, 0), Pos(0, 5));
	editor.ToggleCharField(kCFBold);
	CHECK(document.body[0].FormatAt(0).bold);
	CHECK(!document.body[0].FormatAt(6).bold);
	editor.Undo();
	CHECK(!document.body[0].FormatAt(0).bold);
	editor.Redo();
	CHECK(document.body[0].FormatAt(0).bold);

	// copy and paste across paragraphs
	editor.SetCaret(Pos(0, 11));
	editor.InsertText("\nSecond line\nThird");
	CHECK_EQ(document.body.size(), (size_t)3);
	Story fragment = editor.CopyRange(Pos(0, 6), Pos(1, 6));
	CHECK_EQ(fragment.size(), (size_t)2);
	editor.SetCaret(Pos(2, 5));
	editor.ReplaceSelection(fragment, "Paste");
	CHECK_STR(Text(document.body), "Hello world\nSecond line\nThirdworld\nSecond");
	CHECK_EQ(editor.Caret().para, 3);
	CHECK_EQ(editor.Caret().offset, 6);
	editor.Undo();
	CHECK_STR(Text(document.body), "Hello world\nSecond line\nThird");

	// delete across paragraphs
	editor.Select(Pos(0, 5), Pos(2, 0));
	editor.DeleteSelection("Clear");
	CHECK_STR(Text(document.body), "HelloThird");

	// word navigation
	editor.SelectAll();
	editor.InsertText("one two  three");
	CHECK_EQ(editor.NextWord(Pos(0, 0)).offset, 4);
	CHECK_EQ(editor.NextWord(Pos(0, 4)).offset, 9);
	CHECK_EQ(editor.PrevWord(Pos(0, 9)).offset, 4);
	Pos s, e;
	editor.WordAt(Pos(0, 5), &s, &e);
	CHECK_EQ(s.offset, 4);
	CHECK_EQ(e.offset, 9);

	// styles: Return after a heading gives Normal
	editor.SelectAll();
	editor.InsertText("Title");
	int32_t heading = document.styles.Find("heading 1");
	editor.ApplyStyle(heading);
	CHECK_EQ(document.body[0].format.style, heading);
	CHECK(document.body[0].FormatAt(0).bold);
	editor.SetCaret(Pos(0, 5));
	editor.InsertParagraphBreak();
	CHECK_EQ(document.body[1].format.style, 0);
	editor.InsertText("Body");
	CHECK(!document.body[1].FormatAt(0).bold);

	// change case
	editor.Select(Pos(1, 0), Pos(1, 4));
	editor.ChangeCase(kCaseUpper);
	CHECK_STR(document.body[1].text, "BODY");
	editor.ChangeCase(kCaseTitle);
	CHECK_STR(document.body[1].text, "Body");

	// redefine a style updates its users
	Style style = document.styles.Get(heading);
	style.cf.size = 18;
	editor.RedefineStyle(heading, style);
	CHECK_EQ(document.body[0].FormatAt(0).size, 18.0f);
	editor.Undo();
	CHECK_EQ(document.body[0].FormatAt(0).size, 12.0f);
	CHECK_EQ(document.styles.Get(heading).cf.size, 12.0f);
}


static void
TestLayout()
{
	FakeMetrics metrics;
	Document document;
	Editor editor(&document, &document.body);
	// 12pt chars are 6pt wide; text width 612-180 = 432pt = 72 chars
	std::string words;
	for (int i = 0; i < 30; i++)
		words += "word ";
	editor.InsertText(words);
	DocLayout layout(&metrics);
	layout.SetStory(&document, &document.body, true);
	layout.Update();
	CHECK_EQ(layout.ParaCount(), 1);
	const ParaLayout& para = layout.Para(0);
	// 72 chars per line: "word " * 14 = 70 chars fit, 15th word wraps
	CHECK_EQ(para.lines.size(), (size_t)3);
	CHECK_EQ(para.lines[0].endCp, 70);
	CHECK(std::fabs(para.lines[0].height - 12) < 0.01);

	// caret and hit test round trip
	CaretInfo caret;
	CHECK(layout.Caret(Pos(0, 72), kGeomNormal, false, &caret));
	CHECK(std::fabs(caret.x - 12) < 0.01);
	CHECK(std::fabs(caret.top - 12) < 0.01);
	bool lineEnd;
	Pos hit = layout.HitTest(kGeomNormal, 0, caret.x + 1, caret.top + 3,
		&lineEnd);
	CHECK_EQ(hit.offset, 72);
	// page view: margin 90, top 72
	CHECK(layout.Caret(Pos(0, 72), kGeomPage, false, &caret));
	CHECK(std::fabs(caret.x - 102) < 0.01);
	CHECK(std::fabs(caret.top - 84) < 0.01);

	// alignment
	editor.SelectAll();
	editor.InsertText("abc");
	editor.ApplyParaFormat([](ParaFormat& f) { f.align = kAlignRight; });
	layout.Update();
	CHECK(std::fabs(layout.Para(0).x[0] - (432 - 18)) < 0.01);
	editor.ApplyParaFormat([](ParaFormat& f) { f.align = kAlignCenter; });
	layout.Update();
	CHECK(std::fabs(layout.Para(0).x[0] - (432 - 18) / 2) < 0.01);

	// tabs: right tab at 3in
	editor.ApplyParaFormat([](ParaFormat& f) {
		f.align = kAlignLeft;
		TabStop tab;
		tab.pos = 216;
		tab.kind = kTabRight;
		f.SetTab(tab);
	});
	editor.SelectAll();
	editor.InsertText("a\tbcd");
	layout.Update();
	const ParaLayout& tabbed = layout.Para(0);
	CHECK(std::fabs(tabbed.x[2] - (216 - 18)) < 0.01);
	// default tabs every half inch
	editor.ApplyParaFormat([](ParaFormat& f) { f.tabs.clear(); });
	layout.Update();
	CHECK(std::fabs(layout.Para(0).x[2] - 36) < 0.01);

	// justification stretches spaces on all but the last line
	editor.SelectAll();
	editor.InsertText(words);
	editor.ApplyParaFormat([](ParaFormat& f) { f.align = kAlignJustify; });
	layout.Update();
	const ParaLayout& justified = layout.Para(0);
	CHECK(std::fabs(justified.lines[0].contentRight - 432) < 0.05);

	// pagination: 100 paragraphs of 12pt lines, 648pt per page = 54 lines
	editor.SelectAll();
	std::string many;
	for (int i = 0; i < 100; i++)
		many += "line\n";
	many += "end";
	editor.InsertText(many);
	editor.SelectAll();
	editor.ApplyParaFormat([](ParaFormat& f) { f.align = kAlignLeft; });
	layout.Update();
	CHECK_EQ(layout.PageCount(), 2);
	CHECK_EQ(layout.Placed(54, 0).page, 1);
	CHECK(std::fabs(layout.Placed(54, 0).pageTop - 72) < 0.01);

	// page break before
	editor.SetCaret(Pos(10, 0));
	editor.ApplyParaFormat([](ParaFormat& f) { f.pageBreakBefore = true; });
	layout.Update();
	CHECK_EQ(layout.Placed(10, 0).page, 1);
	CHECK_EQ(layout.Placed(9, 0).page, 0);
	CHECK_EQ(layout.PageCount(), 3);	// 10 + 54 + 37 lines

	// manual page break character
	editor.SetCaret(Pos(20, 4));
	editor.InsertSpecial(kPageBreakChar);
	layout.Update();
	CHECK_EQ(layout.Placed(20, 0).page, 1);
	CHECK_EQ(layout.Placed(21, 0).page, 2);
	CHECK_EQ(layout.PageCount(), 4);	// 10 + 11 + 54 + 26 lines

	// selection rectangles
	std::vector<SelectionRect> rects;
	layout.SelectionRects(Pos(0, 1), Pos(2, 2), kGeomNormal, 6, rects);
	CHECK_EQ(rects.size(), (size_t)3);

	// line breaks within a paragraph
	editor.SelectAll();
	editor.InsertText("ab");
	editor.InsertSpecial(kLineBreakChar);
	editor.InsertText("cd");
	layout.Update();
	CHECK_EQ(layout.Para(0).lines.size(), (size_t)2);
	CHECK(layout.Caret(Pos(0, 3), kGeomNormal, false, &caret));
	CHECK(std::fabs(caret.top - 12) < 0.01);
	// final line break creates an empty line
	editor.SetCaret(Pos(0, 5));
	editor.InsertSpecial(kLineBreakChar);
	layout.Update();
	CHECK_EQ(layout.Para(0).lines.size(), (size_t)3);
	CHECK(layout.Caret(Pos(0, 6), kGeomNormal, false, &caret));
	CHECK(std::fabs(caret.top - 24) < 0.01);
}


static void
TestTables()
{
	FakeMetrics metrics;
	Document document;
	// two cells and a row end
	Story& body = document.body;
	body.clear();
	for (int i = 0; i < 2; i++) {
		Paragraph cell = document.NewParagraph(0);
		cell.kind = kParaCell;
		cell.format.inTable = true;
		cell.Insert(0, i == 0 ? "left" : "right cell text", cell.FormatAt(0));
		body.push_back(cell);
	}
	Paragraph end = document.NewParagraph(0);
	end.kind = kParaRowEnd;
	end.format.inTable = true;
	CellFormat c1;
	c1.right = 100;
	CellFormat c2;
	c2.right = 200;
	end.row.cells = { c1, c2 };
	end.row.leftEdge = -5.4f;
	body.push_back(end);
	body.push_back(document.NewParagraph(0));

	DocLayout layout(&metrics);
	layout.SetStory(&document, &body, true);
	layout.Update();
	CHECK_EQ(layout.Blocks().size(), (size_t)2);
	CHECK(layout.Blocks()[0].row);
	// second cell starts at its left edge plus the gap
	CHECK(std::fabs(layout.Placed(1, 0).normalX - (100 - 5.4f + 5.4f)) < 0.01);
	CHECK(std::fabs(layout.Placed(0, 0).normalTop
		- layout.Placed(1, 0).normalTop) < 0.01);

	// typing Return in a cell inserts a line break
	Editor editor(&document, &body);
	editor.SetCaret(Pos(0, 4));
	editor.InsertParagraphBreak();
	CHECK_EQ(body.size(), (size_t)4);
	CHECK_EQ(CharAt(body[0].text, 4), (uint32_t)kLineBreakChar);
	// deleting across cells clears text but keeps the structure
	editor.Select(Pos(0, 2), Pos(1, 3));
	editor.DeleteSelection("Clear");
	CHECK_EQ(body.size(), (size_t)4);
	CHECK_STR(body[0].text, "le");
	CHECK_STR(body[1].text, "ht cell text");

	// a row taller than a page continues on the next page
	{
		Document tall;
		tall.body.clear();
		Paragraph cell = tall.NewParagraph(0);
		cell.kind = kParaCell;
		cell.format.inTable = true;
		std::string lines;
		for (int i = 0; i < 80; i++)
			lines += i == 0 ? "x" : "\vx";
		cell.Insert(0, lines, cell.FormatAt(0));
		tall.body.push_back(cell);
		Paragraph rowEnd = tall.NewParagraph(0);
		rowEnd.kind = kParaRowEnd;
		rowEnd.format.inTable = true;
		CellFormat only;
		only.right = 200;
		rowEnd.row.cells = { only };
		tall.body.push_back(rowEnd);
		tall.body.push_back(tall.NewParagraph(0));
		DocLayout tallLayout(&metrics);
		tallLayout.SetStory(&tall, &tall.body, true);
		tallLayout.Update();
		// 648 pt per page, 12 pt lines: 54 lines on the first page
		CHECK_EQ(tallLayout.PageCount(), 2);
		CHECK_EQ(tallLayout.Placed(0, 53).page, 0);
		CHECK_EQ(tallLayout.Placed(0, 54).page, 1);
		CHECK(std::fabs(tallLayout.Placed(0, 54).pageTop - 72) < 0.01);
		CHECK_EQ(tallLayout.Placed(2, 0).page, 1);
		CHECK(std::fabs(tallLayout.Placed(2, 0).pageTop - (72 + 26 * 12))
			< 0.01);
	}

	// RTF round trip of the table
	std::string rtf = WriteRTF(document);
	Document copy;
	CHECK(ReadRTF(rtf, copy));
	CHECK_EQ(copy.body.size(), (size_t)4);
	CHECK_EQ(copy.body[0].kind, (uint8_t)kParaCell);
	CHECK_EQ(copy.body[2].kind, (uint8_t)kParaRowEnd);
	CHECK_EQ(copy.body[2].row.cells.size(), (size_t)2);
	CHECK(std::fabs(copy.body[2].row.cells[1].right - 200) < 0.06);
	CHECK_STR(WriteRTF(copy), rtf);
}


static void
TestRTF()
{
	Document document;
	Editor editor(&document, &document.body);
	editor.InsertText("Plain bold \xE2\x80\x9Cquoted\xE2\x80\x9D caf\xC3\xA9 {brace} \\ back");
	editor.Select(Pos(0, 6), Pos(0, 10));
	editor.ToggleCharField(kCFBold);
	editor.SetCaret(editor.End());
	editor.InsertParagraphBreak();
	editor.InsertText("Second\tparagraph");
	editor.ApplyParaFormat([](ParaFormat& f) {
		f.align = kAlignJustify;
		f.leftIndent = 36;
		f.firstIndent = -18;
		f.spaceBefore = 12;
		f.lineRule = kLineAuto;
		f.lineValue = 1.5f;
		TabStop tab;
		tab.pos = 144;
		tab.kind = kTabDecimal;
		tab.leader = kLeaderDot;
		f.SetTab(tab);
		f.border.top = kBorderSingle;
		f.border.bottom = kBorderDouble;
		f.keepWithNext = true;
	});
	editor.Select(Pos(1, 0), Pos(1, 6));
	CharFormat values;
	values.color = 0xFF0000;
	values.underline = kULDouble;
	values.position = 3;
	values.spacing = -0.5f;
	editor.ApplyCharFields(values, kCFColor | kCFUnderline | kCFPosition
		| kCFSpacing);
	editor.SetCaret(editor.End());
	editor.InsertSpecial(kFieldPageChar);
	editor.InsertSpecial(kSectionBreakChar);
	editor.InsertText("after break");

	// footnote
	int32_t id = document.nextFootnoteId++;
	CharFormat ref = editor.InsertionFormat();
	ref.footnote = id;
	ref.position = 3;
	editor.InsertSpecial(kFootnoteChar, &ref);
	Story note = document.NewStory(document.styles.Find("footnote text"));
	CharFormat noteRef = note[0].FormatAt(0);
	noteRef.footnote = id;
	noteRef.position = 3;
	CharFormat noteText = note[0].FormatAt(0);
	note[0].Insert(0, Utf8(kFootnoteChar), noteRef);
	note[0].Insert(note[0].Length(), " The note.", noteText);
	document.footnotes[id] = note;

	// picture
	auto picture = std::make_shared<Picture>();
	picture->type = "png";
	picture->data = std::string("\x89PNG\r\n\x1a\n\0\1\2", 11);
	picture->pixelWidth = 10;
	picture->pixelHeight = 20;
	picture->width = 7.5f;
	picture->height = 15;
	CharFormat pictureFormat = editor.InsertionFormat();
	pictureFormat.picture = document.AddPicture(picture);
	editor.InsertSpecial(kPictureChar, &pictureFormat);

	document.header[0].Insert(0, "Header ", document.header[0].FormatAt(0));
	document.header[0].Insert(7, Utf8(kFieldPageChar),
		document.header[0].FormatAt(0));
	document.settings.marginLeft = 72;
	document.settings.columns = 2;
	document.settings.titlePage = true;
	document.settings.pageNumberFormat = kPageLowerRoman;
	document.info.title = "Test document";
	document.info.author = "Somebody";

	std::string rtf = WriteRTF(document);
	Document copy;
	std::string error;
	CHECK(ReadRTF(rtf, copy, &error));
	CHECK_EQ(copy.body.size(), document.body.size());
	CHECK_STR(Text(copy.body), Text(document.body));
	CHECK(copy.body[0].FormatAt(7).bold);
	CHECK(!copy.body[0].FormatAt(2).bold);
	CHECK_EQ(copy.body[1].format.align, (uint8_t)kAlignJustify);
	CHECK_EQ(copy.body[1].format.firstIndent, -18.0f);
	CHECK_EQ(copy.body[1].format.lineValue, 1.5f);
	CHECK_EQ(copy.body[1].format.tabs.size(), (size_t)1);
	CHECK_EQ(copy.body[1].format.tabs[0].kind, (uint8_t)kTabDecimal);
	CHECK_EQ(copy.body[1].format.border.bottom, (uint8_t)kBorderDouble);
	CHECK(copy.body[1].format.keepWithNext);
	CHECK_EQ(copy.body[1].FormatAt(0).color, (uint32_t)0xFF0000);
	CHECK_EQ(copy.body[1].FormatAt(0).underline, (uint8_t)kULDouble);
	CHECK_EQ(copy.body[1].FormatAt(0).position, 3.0f);
	CHECK_EQ(copy.body[1].FormatAt(0).spacing, -0.5f);
	CHECK_EQ(copy.footnotes.size(), (size_t)1);
	CHECK_EQ(copy.pictures.size(), (size_t)1);
	CHECK_EQ(copy.pictures.begin()->second->data, picture->data);
	CHECK_EQ(copy.settings.columns, 2);
	CHECK(copy.settings.titlePage);
	CHECK_EQ(copy.settings.marginLeft, 72.0f);
	CHECK_STR(copy.info.title, "Test document");
	CHECK_STR(Text(copy.header), Text(document.header));
	CHECK_EQ(copy.styles.Count(), document.styles.Count());
	// writing the copy gives the same RTF
	CHECK_STR(WriteRTF(copy), rtf);

	// fragment round trip keeps the partial last paragraph open
	Story fragment = editor.CopyRange(Pos(0, 6), Pos(1, 6));
	std::string clip = WriteRTFFragment(document, fragment);
	Document scratch;
	Story read;
	CHECK(ReadRTFFragment(clip, scratch, read));
	CHECK_EQ(read.size(), (size_t)2);
	CHECK_STR(read[0].text, fragment[0].text);
	CHECK_STR(read[1].text, "Second");
	Story imported = ImportStory(document, scratch, read);
	CHECK_EQ(imported.size(), (size_t)2);

	// foreign RTF: Mac code page, \u with fallback, field, tables, pntext
	const char* mac = "{\\rtf1\\mac\\deff0{\\fonttbl{\\f0\\froman Times;}"
		"{\\f1\\fswiss Helvetica;}}{\\colortbl;\\red0\\green0\\blue255;}"
		"{\\stylesheet{\\f0\\fs24 \\snext0 Normal;}{\\s1\\sb240\\keepn "
		"\\b\\f1\\fs28 \\sbasedon0\\snext0 heading 1;}}"
		"\\pard\\plain\\s1 \\b\\f1\\fs28 Caf\\'8e Title\\par"
		"\\pard\\plain \\f0\\fs24 Unicode \\u8364\\'80 euro and "
		"{\\field{\\*\\fldinst {PAGE }}{\\fldrslt {7}}} page"
		"{\\*\\bkmkstart x}{\\cf1 blue}\\par"
		"{\\pntext\\f2 \\'a5\\tab}normal\\par}";
	Document foreign;
	CHECK(ReadRTF(mac, foreign, &error));
	CHECK_EQ(foreign.body.size(), (size_t)3);
	CHECK_STR(foreign.body[0].text, "Caf\xC3\xA9 Title");
	CHECK_EQ(foreign.body[0].format.style, 1);
	CHECK_STR(foreign.styles.Get(1).name, "heading 1");
	CHECK(foreign.styles.Get(1).pf.keepWithNext);
	CHECK_STR(foreign.body[1].text, "Unicode \xE2\x82\xAC euro and "
		"\xEE\x80\x80 pageblue");
	CHECK_EQ(foreign.body[1].FormatAt(foreign.body[1].Length() - 1).color,
		(uint32_t)0x0000FF);
	CHECK_STR(foreign.body[2].text, "\xE2\x80\xA2\tnormal");

	// not RTF
	CHECK(!ReadRTF("hello", foreign, &error));
}


static void
TestSearch()
{
	Document document;
	Editor editor(&document, &document.body);
	editor.InsertText("The cat sat.\nThe Cattle\tran.\nconcat");
	SearchOptions options;
	SearchPattern cat("cat", options);
	Pos start, end;
	CHECK(cat.Find(document.body, Pos(0, 0), true, editor.End(), &start, &end));
	CHECK_EQ(start.offset, 4);
	CHECK(cat.Find(document.body, end, true, editor.End(), &start, &end));
	CHECK_EQ(start.para, 1);
	options.wholeWord = true;
	SearchPattern word("cat", options);
	CHECK(word.Find(document.body, Pos(0, 5), true, editor.End(), &start,
		&end) == false);
	options.wholeWord = false;
	options.matchCase = true;
	SearchPattern cased("Cat", options);
	CHECK(cased.Find(document.body, Pos(0, 0), true, editor.End(), &start,
		&end));
	CHECK_EQ(start.para, 1);
	options.matchCase = false;
	SearchPattern special("sat.^pthe", options);
	CHECK(special.Find(document.body, Pos(0, 0), true, editor.End(), &start,
		&end));
	CHECK_EQ(start.offset, 8);
	CHECK_EQ(end.para, 1);
	CHECK_EQ(end.offset, 3);
	SearchPattern tab("e^tr^?n", options);
	CHECK(tab.Find(document.body, Pos(0, 0), true, editor.End(), &start,
		&end));
	SearchPattern backwards("the", options);
	CHECK(backwards.Find(document.body, editor.End(), false, Pos(0, 0),
		&start, &end));
	CHECK_EQ(start.para, 1);
	CHECK_STR(ExpandReplacement("a^tb^pc^^"), "a\tb\nc^");
}


static void
TestTools()
{
	double value;
	std::string shown;
	CHECK(Calculate("2+3*4", &value, &shown));
	CHECK_EQ(value, 14.0);
	CHECK_STR(shown, "14");
	CHECK(Calculate("10 20 30", &value, &shown));
	CHECK_EQ(value, 60.0);
	CHECK(Calculate("$1,200.50 - $200", &value, &shown));
	CHECK_STR(shown, "$1,000.50");
	CHECK(Calculate("(5) + 10", &value, &shown));
	CHECK_EQ(value, 5.0);
	CHECK(Calculate("(2+3)*4", &value, &shown));
	CHECK_EQ(value, 20.0);
	CHECK(Calculate("50%*200", &value, &shown));
	CHECK_EQ(value, 100.0);
	CHECK(!Calculate("1/0", &value, &shown));
	CHECK(!Calculate("hello", &value, &shown));

	Document document;
	Editor editor(&document, &document.body);
	editor.InsertText("pear\nApple\n10 items\n9 items\nbanana");
	SortParagraphs(editor, false);
	CHECK_STR(Text(document.body), "9 items\n10 items\nApple\nbanana\npear");
	editor.Undo();
	CHECK_STR(Text(document.body), "pear\nApple\n10 items\n9 items\nbanana");

	editor.SelectAll();
	RenumberOptions options;
	Renumber(editor, options);
	CHECK_STR(document.body[0].text, "1.\tpear");
	CHECK_STR(document.body[4].text, "5.\tbanana");
	options.format = kNumberUpperRoman;
	options.onlyNumbered = true;
	Renumber(editor, options);
	CHECK_STR(document.body[3].text, "IV.\t9 items");
	options.remove = true;
	Renumber(editor, options);
	CHECK_STR(document.body[3].text, "9 items");

	editor.SetCaret(Pos(0, 0));
	ToggleBullets(editor);
	CHECK_STR(document.body[0].text, "\xE2\x80\xA2\tpear");
	CHECK_EQ(document.body[0].format.firstIndent, -18.0f);
	ToggleBullets(editor);
	CHECK_STR(document.body[0].text, "pear");
	CHECK_EQ(document.body[0].format.firstIndent, 0.0f);

	WordCount count = CountWords(document.body, Pos(0, 0), Pos(0, 0));
	CHECK_EQ(count.words, (int64_t)7);
	CHECK_EQ(count.paragraphs, (int64_t)5);

	// text to table and back
	{
		Document doc;
		Editor te(&doc, &doc.body);
		te.InsertText("Name,Size,Kind\nApple,\"1,200\",Fruit\nLeek,3,Vegetable");
		te.Select(Pos(0, 0), Pos(2, 3));
		TextTableAnalysis analysis = AnalyzeTextForTable(te);
		CHECK(analysis.convertible);
		CHECK_EQ(analysis.suggested, (int)kTableCommas);
		CHECK_EQ(TextTableColumns(analysis, kTableCommas), 3);
		CHECK_EQ(TextTableRows(analysis, kTableCommas, 3), 3);
		CHECK_EQ(TextTableRows(analysis, kTableParagraphs, 2), 2);
		CHECK(ConvertTextToTable(te, kTableCommas, 3, 0, 432));
		// 3 rows of 3 cells and a row end, and a paragraph after the table
		CHECK_EQ(doc.body.size(), (size_t)13);
		CHECK_EQ(doc.body[0].kind, (uint8_t)kParaCell);
		CHECK_STR(doc.body[5].text, "1,200");
		CHECK_EQ(doc.body[11].kind, (uint8_t)kParaRowEnd);
		CHECK_EQ(doc.body[11].row.cells.size(), (size_t)3);
		CHECK(std::fabs(doc.body[11].row.cells[2].right - (432 + 5.4f)) < 0.01);
		CHECK_EQ(doc.body[12].kind, (uint8_t)kParaNormal);
		te.Undo();
		CHECK_STR(Document::PlainText(doc.body),
			"Name,Size,Kind\nApple,\"1,200\",Fruit\nLeek,3,Vegetable");
		te.Redo();
		// selected rows only
		te.Select(Pos(4, 0), Pos(6, 2));
		CHECK(ConvertTableToText(te, kTableTabs));
		CHECK_STR(doc.body[4].text, "Apple\t1,200\tFruit");
		CHECK_EQ(doc.body[3].kind, (uint8_t)kParaRowEnd);
		CHECK_EQ(doc.body[5].kind, (uint8_t)kParaCell);
		te.Undo();
		// back to text, quoting the cell with a comma; a selection inside
		// one cell converts the whole table
		te.Select(Pos(4, 1), Pos(4, 3));
		CHECK(ConvertTableToText(te, kTableCommas));
		CHECK_STR(Document::PlainText(doc.body),
			"Name,Size,Kind\nApple,\"1,200\",Fruit\nLeek,3,Vegetable\n");
		CHECK_EQ(doc.body[0].kind, (uint8_t)kParaNormal);

		// tabs keep character formatting; short lines get empty cells
		te.SelectAll();
		te.InsertText("a\tb\tc\nd\te");
		te.Select(Pos(0, 2), Pos(0, 3));
		te.ToggleCharField(kCFBold);
		te.Select(Pos(0, 0), Pos(1, 4));
		analysis = AnalyzeTextForTable(te);
		CHECK_EQ(analysis.suggested, (int)kTableTabs);
		CHECK(ConvertTextToTable(te, kTableTabs, 3, 0, 432));
		CHECK_EQ(doc.body.size(), (size_t)9);
		CHECK_STR(doc.body[1].text, "b");
		CHECK(doc.body[1].FormatAt(0).bold);
		CHECK_STR(doc.body[5].text, "e");
		CHECK_STR(doc.body[6].text, "");
		CHECK_EQ(doc.body[7].kind, (uint8_t)kParaRowEnd);
		te.Select(Pos(0, 0), Pos(0, 0));
		CHECK(ConvertTableToText(te, kTableTabs));
		CHECK_STR(Document::PlainText(doc.body), "a\tb\tc\nd\te\t\n");
		CHECK(doc.body[0].FormatAt(2).bold);

		// paragraphs fill the columns of each row in turn
		te.SelectAll();
		te.InsertText("one\ntwo\nthree");
		te.SelectAll();
		CHECK(ConvertTextToTable(te, kTableParagraphs, 2, 72, 432));
		CHECK_STR(doc.body[0].text, "one");
		CHECK_STR(doc.body[1].text, "two");
		CHECK_STR(doc.body[3].text, "three");
		CHECK_STR(doc.body[4].text, "");
		CHECK(std::fabs(doc.body[2].row.cells[1].right - (144 + 5.4f)) < 0.01);
		te.Select(Pos(0, 0), Pos(0, 0));
		CHECK(!AnalyzeTextForTable(te).convertible);
		CHECK(ConvertTableToText(te, kTableParagraphs));
		CHECK_STR(Document::PlainText(doc.body), "one\ntwo\nthree\n\n");
	}

	// outline: headings, body text, promote, demote, move
	{
		Document outline;
		Editor oe(&outline, &outline.body);
		oe.InsertText("One\nbody one\nTwo\nbody two");
		int32_t h1 = outline.styles.Find("heading 1");
		oe.Select(Pos(0, 0), Pos(0, 0));
		oe.ApplyStyle(h1);
		oe.Select(Pos(2, 0), Pos(2, 0));
		oe.ApplyStyle(h1);
		oe.Select(Pos(2, 0), Pos(2, 0));
		OutlineChangeLevel(oe, 1);
		CHECK_STR(outline.styles.Get(outline.body[2].format.style).name,
			"heading 2");
		OutlineChangeLevel(oe, 1);
		OutlineChangeLevel(oe, 1);
		CHECK_STR(outline.styles.Get(outline.body[2].format.style).name,
			"heading 4");
		CHECK_EQ(outline.styles.OutlineLevel(outline.body[2].format.style), 4);
		oe.Undo();
		CHECK_STR(outline.styles.Get(outline.body[2].format.style).name,
			"heading 3");
		oe.Select(Pos(1, 0), Pos(1, 0));
		OutlineChangeLevel(oe, -1);
		CHECK_EQ(outline.body[1].format.style, h1);
		OutlineToBody(oe);
		CHECK_EQ(outline.body[1].format.style, 0);
		// move "Two" (with its body text hidden under it) above "One"
		std::vector<bool> visible = { true, true, true, false };
		oe.Select(Pos(2, 0), Pos(2, 0));
		OutlineMove(oe, true, visible);
		CHECK_STR(Document::PlainText(outline.body),
			"One\nTwo\nbody two\nbody one");
		oe.Undo();
		CHECK_STR(Document::PlainText(outline.body),
			"One\nbody one\nTwo\nbody two");

		FakeMetrics outlineMetrics;
		DocLayout layout(&outlineMetrics);
		layout.SetStory(&outline, &outline.body, true);
		LayoutOptions options;
		options.outline = true;
		layout.SetOptions(options);
		outline.body[2].collapsed = true;
		layout.Update();
		CHECK(layout.Outline(3) != nullptr && !layout.Outline(3)->visible);
		CHECK(layout.Outline(2)->hasChildren);
		CHECK_EQ(layout.Para(3).height, 0.0f);
		CHECK(layout.Para(1).x[0] >= 18 + 36 - 0.01f);
		options.outlineShowLevel = 1;
		layout.SetOptions(options);
		layout.Update();
		CHECK(!layout.Outline(1)->visible);
		CHECK(layout.Outline(0)->visible);
		CHECK_EQ(layout.PageCount(), 1);
	}

	Nest(editor, true);
	CHECK_EQ(document.body[0].format.leftIndent, 36.0f);
	Nest(editor, false);
	CHECK_EQ(document.body[0].format.leftIndent, 0.0f);
}


static void
TestSpell(const char* wordList)
{
	SpellChecker checker;
	checker.AddRoot("bake/DGRSZ");
	checker.AddRoot("try/DGS");
	checker.AddRoot("fix/DGS");
	checker.AddRoot("Paris/M");
	checker.AddRoot("happy/PTY");
	checker.AddRoot("do/AU");
	CHECK(checker.IsCorrect("baked"));
	CHECK(checker.IsCorrect("baking"));
	CHECK(checker.IsCorrect("bakers"));
	CHECK(checker.IsCorrect("tried"));
	CHECK(checker.IsCorrect("tries"));
	CHECK(checker.IsCorrect("fixes"));
	CHECK(checker.IsCorrect("Paris's"));
	CHECK(checker.IsCorrect("PARIS"));
	CHECK(!checker.IsCorrect("paris"));
	CHECK(checker.IsCorrect("Baked"));
	CHECK(checker.IsCorrect("happiness"));
	CHECK(checker.IsCorrect("happiest"));
	CHECK(checker.IsCorrect("redo"));
	CHECK(checker.IsCorrect("undo"));
	CHECK(!checker.IsCorrect("bakd"));
	std::vector<std::string> suggestions = checker.Suggest("bakd");
	CHECK(!suggestions.empty());
	CHECK(std::find(suggestions.begin(), suggestions.end(), "baked")
		!= suggestions.end());
	suggestions = checker.Suggest("Tryed");
	CHECK(std::find(suggestions.begin(), suggestions.end(), "Tried")
		!= suggestions.end());

	if (wordList != nullptr) {
		SpellChecker real;
		CHECK(real.LoadWordList(wordList));
		fprintf(stderr, "  dictionary: %zu words\n", real.WordCount());
		CHECK(real.IsCorrect("processor"));
		CHECK(real.IsCorrect("documents"));
		CHECK(real.IsCorrect("writing"));
		CHECK(real.IsCorrect("Macintosh"));
		CHECK(!real.IsCorrect("documnet"));
		suggestions = real.Suggest("documnet");
		CHECK(!suggestions.empty() && suggestions[0] == "document");

		Document document;
		Editor editor(&document, &document.body);
		editor.InsertText("This sentense has a mistaek in it.");
		Pos start, end;
		std::string word;
		CHECK(real.NextMisspelling(document.body, Pos(0, 0), editor.End(),
			&start, &end, &word));
		CHECK_STR(word, "sentense");
		CHECK(real.NextMisspelling(document.body, end, editor.End(), &start,
			&end, &word));
		CHECK_STR(word, "mistaek");
		CHECK(!real.NextMisspelling(document.body, end, editor.End(), &start,
			&end, &word));
	}
}


static void
TestTextIO()
{
	std::string encoding;
	CHECK_STR(TextToUtf8("caf\x8e\rnext", &encoding), "caf\xC3\xA9\rnext");
	CHECK_STR(encoding, "Mac Roman");
	CHECK_STR(TextToUtf8("caf\xe9\r\nnext", &encoding), "caf\xC3\xA9\r\nnext");
	CHECK_STR(encoding, "Windows-1252");
	Document document;
	ReadPlainText("one\r\ntwo\rthree\n", document);
	CHECK_EQ(document.body.size(), (size_t)3);
	CHECK_STR(WritePlainText(document), "one\ntwo\nthree\n");
}


int
main(int argc, char** argv)
{
	TestParagraph();
	TestEditor();
	TestLayout();
	TestTables();
	TestRTF();
	TestSearch();
	TestTools();
	TestSpell(argc > 1 ? argv[1] : nullptr);
	TestTextIO();
	printf("%d checks, %d failures\n", sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}
