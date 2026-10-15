/*
 * Quill - commands of the Tools menu and the Toolbar that work on whole
 * paragraphs: Word Count, Sort, Calculate, Renumber, bullets, indents.
 */
#ifndef QUILL_TOOLS_H
#define QUILL_TOOLS_H

#include "Editor.h"

#include <string>

namespace quill {

struct WordCount {
	int64_t	characters = 0;
	int64_t	words = 0;
	int64_t	paragraphs = 0;
	int64_t	lines = 0;			// filled in by the caller from the layout
	int64_t	footnoteCharacters = 0;
	int64_t	footnoteWords = 0;
};

WordCount CountWords(const Story& story, Pos start, Pos end);
int64_t CountWordsInText(const std::string& text);

// Sorts the selected paragraphs (all when nothing is selected) by their
// text, numbers numerically, ignoring case. Table rows are kept whole.
void SortParagraphs(Editor& editor, bool descending);

// Evaluates the arithmetic in the text like Word's Calculate command:
// + - * / % ^ and parentheses; numbers without an operator are added;
// "$", "," and accounting parentheses are understood.
bool Calculate(const std::string& text, double* result, std::string* shown);

enum NumberFormat {
	kNumberArabic = 0,		// 1.
	kNumberArabicParen,		// 1)
	kNumberUpperRoman,		// I.
	kNumberLowerRoman,		// i.
	kNumberUpperLetter,		// A.
	kNumberLowerLetter,		// a.
	kNumberOutline			// 1.1 by heading level
};

struct RenumberOptions {
	bool	onlyNumbered = false;
	int32_t	start = 1;
	int		format = kNumberArabic;
	bool	remove = false;
};

void Renumber(Editor& editor, const RenumberOptions& options);
bool HasNumber(const std::string& text, size_t* length);

// Toolbar commands
void ToggleBullets(Editor& editor);
void ToggleNumbers(Editor& editor);
void Nest(Editor& editor, bool in);		// indent/unindent by a default tab
void SetHangingIndent(Editor& editor);

// Text to Table and Table to Text, as in Word's Table command: the text
// of whole paragraphs is split into cells at paragraph marks, tabs or
// commas (commas follow CSV quoting: "a, b" stays one cell).
enum TableTextFormat {
	kTableParagraphs = 0,
	kTableTabs,
	kTableCommas
};

struct TextTableAnalysis {
	int32_t	first = 0;				// paragraphs to convert
	int32_t	last = -1;
	std::vector<int32_t> tabFields;		// cells per paragraph
	std::vector<int32_t> commaFields;
	int		suggested = kTableParagraphs;
	bool	convertible = false;	// false when the text is in a table
};

TextTableAnalysis AnalyzeTextForTable(const Editor& editor);
int32_t TextTableColumns(const TextTableAnalysis& analysis, int format);
int32_t TextTableRows(const TextTableAnalysis& analysis, int format,
	int32_t columns);
// columnWidth 0 divides availableWidth among the columns.
bool ConvertTextToTable(Editor& editor, int format, int32_t columns,
	float columnWidth, float availableWidth);
bool ConvertTableToText(Editor& editor, int format);
std::vector<std::pair<int32_t, int32_t> > SplitTableFields(
	const std::string& text, int format);

// Outline view commands. visible tells which paragraphs Outline view
// shows; hidden paragraphs move with the heading above them.
int32_t EnsureHeadingStyle(Document& document, int32_t level);
void OutlineChangeLevel(Editor& editor, int delta);
void OutlineToBody(Editor& editor);
void OutlineMove(Editor& editor, bool up, const std::vector<bool>& visible);
void OutlineSetCollapsed(Editor& editor, bool collapsed);

// Table of contents from the outline headings. pages gives the printed
// page number of each body paragraph.
int32_t EnsureTocStyle(Document& document, int32_t level);
Story BuildTableOfContents(Document& document,
	const std::vector<int32_t>& pages, int32_t maxLevel);

// A fragment made from text with paragraph marks ("\n"), used by Replace,
// Glossary and Date insertion.
Story FragmentFromText(const std::string& text, const CharFormat& format,
	const ParaFormat& paragraph);

std::string FormatDate(time_t when, bool longForm);
std::string FormatTime(time_t when);

}	// namespace quill

#endif	// QUILL_TOOLS_H
