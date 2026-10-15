/*
 * Quill - paragraph tools.
 */
#include "Tools.h"
#include "Text.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace quill {

int64_t
CountWordsInText(const std::string& text)
{
	int64_t words = 0;
	bool inWord = false;
	int32_t length = (int32_t)text.size();
	for (int32_t offset = 0; offset < length;) {
		int32_t consumed;
		uint32_t c = DecodeUtf8(text.c_str() + offset, length - offset,
			&consumed);
		offset += std::max<int32_t>(1, consumed);
		bool white = IsSpace(c) || c == '\t' || c == '\n' || c == kNbspChar
			|| c == kLineBreakChar || c == kPageBreakChar
			|| c == kSectionBreakChar || c == kPictureChar
			|| c == kFootnoteChar;
		bool countsAsWord = !white && (IsWordChar(c) || c > 0x7F
			|| (c >= '!' && c <= '~'));
		if (!white && !inWord && countsAsWord) {
			words++;
			inWord = true;
		} else if (white)
			inWord = false;
	}
	return words;
}


WordCount
CountWords(const Story& story, Pos start, Pos end)
{
	WordCount count;
	if (start == end) {
		start = Pos(0, 0);
		end = Document::EndOf(story);
	}
	for (int32_t p = start.para; p <= end.para && p < (int32_t)story.size();
			p++) {
		const Paragraph& paragraph = story[p];
		if (paragraph.kind == kParaRowEnd)
			continue;
		int32_t from = p == start.para ? start.offset : 0;
		int32_t to = p == end.para ? end.offset : paragraph.Length();
		if (to <= from)
			continue;
		std::string text = paragraph.text.substr(from, to - from);
		int32_t length = (int32_t)text.size();
		for (int32_t offset = 0; offset < length;) {
			int32_t consumed;
			uint32_t c = DecodeUtf8(text.c_str() + offset, length - offset,
				&consumed);
			offset += std::max<int32_t>(1, consumed);
			if (!IsSpecialChar(c) && c != kSoftHyphenChar)
				count.characters++;
		}
		count.words += CountWordsInText(text);
		count.paragraphs++;
	}
	return count;
}


// #pragma mark - sort


static int
NaturalCompare(const std::string& a, const std::string& b)
{
	size_t i = 0;
	size_t j = 0;
	while (i < a.size() && j < b.size()) {
		unsigned char ca = a[i];
		unsigned char cb = b[j];
		if (isdigit(ca) && isdigit(cb)) {
			double va = strtod(a.c_str() + i, nullptr);
			double vb = strtod(b.c_str() + j, nullptr);
			if (va != vb)
				return va < vb ? -1 : 1;
			while (i < a.size() && (isdigit((unsigned char)a[i]) || a[i] == '.'))
				i++;
			while (j < b.size() && (isdigit((unsigned char)b[j]) || b[j] == '.'))
				j++;
			continue;
		}
		int32_t consumedA, consumedB;
		uint32_t ua = ToLower(DecodeUtf8(a.c_str() + i,
			(int32_t)(a.size() - i), &consumedA));
		uint32_t ub = ToLower(DecodeUtf8(b.c_str() + j,
			(int32_t)(b.size() - j), &consumedB));
		if (ua != ub)
			return ua < ub ? -1 : 1;
		i += std::max<int32_t>(1, consumedA);
		j += std::max<int32_t>(1, consumedB);
	}
	if (i < a.size())
		return 1;
	if (j < b.size())
		return -1;
	return 0;
}


void
SortParagraphs(Editor& editor, bool descending)
{
	Story& story = editor.GetStory();
	int32_t first = 0;
	int32_t last = (int32_t)story.size() - 1;
	if (editor.HasSelection()) {
		first = editor.FirstSelectedPara();
		last = editor.LastSelectedPara();
	}
	// Do not split table rows.
	while (first > 0 && story[first].kind == kParaCell
		&& story[first - 1].kind == kParaCell)
		first--;
	while (last + 1 < (int32_t)story.size()
		&& (story[last].kind == kParaCell))
		last++;
	if (last <= first)
		return;

	struct Unit {
		std::string key;
		std::vector<Paragraph> paragraphs;
	};
	std::vector<Unit> units;
	for (int32_t p = first; p <= last; p++) {
		Unit unit;
		unit.key = story[p].text;
		if (story[p].kind == kParaCell) {
			while (p <= last && story[p].kind != kParaRowEnd)
				unit.paragraphs.push_back(story[p++]);
			if (p <= last)
				unit.paragraphs.push_back(story[p]);
		} else
			unit.paragraphs.push_back(story[p]);
		units.push_back(unit);
	}
	std::stable_sort(units.begin(), units.end(),
		[descending](const Unit& a, const Unit& b) {
			int result = NaturalCompare(a.key, b.key);
			return descending ? result > 0 : result < 0;
		});

	editor.BeginBulk(descending ? "Sort Descending" : "Sort");
	std::vector<Paragraph> sorted;
	for (Unit& unit : units) {
		for (Paragraph& paragraph : unit.paragraphs)
			sorted.push_back(paragraph);
	}
	story.erase(story.begin() + first, story.begin() + last + 1);
	story.insert(story.begin() + first, sorted.begin(), sorted.end());
	editor.Select(Pos(first, 0), Pos(last, story[last].Length()));
	editor.EndBulk();
}


// #pragma mark - calculate


namespace {

class Calculator {
public:
	Calculator(const std::string& text)
		: fText(text), fPos(0), fDecimals(0), fDollar(false), fCommas(false),
		fError(false) {}

	bool Evaluate(double* result)
	{
		_Skip();
		if (fPos >= fText.size())
			return false;
		double value = _Expression();
		_Skip();
		while (fPos < fText.size() && !fError) {
			// leftover numbers are added
			if (_StartsOperand()) {
				value += _Term();
			} else
				fPos++;
			_Skip();
		}
		if (fError || std::isnan(value) || std::isinf(value))
			return false;
		*result = value;
		return true;
	}

	int Decimals() const { return fDecimals; }
	bool Dollar() const { return fDollar; }
	bool Commas() const { return fCommas; }

private:
	void _Skip()
	{
		while (fPos < fText.size()) {
			unsigned char c = fText[fPos];
			if (isdigit(c) || c == '.' || c == '(' || c == ')' || c == '+'
				|| c == '-' || c == '*' || c == '/' || c == '%' || c == '^'
				|| c == '$')
				break;
			if ((c == 'x' || c == 'X') && fPos + 1 < fText.size()
				&& (isdigit((unsigned char)fText[fPos + 1])
					|| fText[fPos + 1] == ' '))
				break;
			fPos++;
		}
	}

	bool _StartsOperand()
	{
		_Skip();
		if (fPos >= fText.size())
			return false;
		char c = fText[fPos];
		return isdigit((unsigned char)c) || c == '.' || c == '(' || c == '$'
			|| c == '-';
	}

	double _Expression()
	{
		double value = _Term();
		while (true) {
			_Skip();
			if (fPos >= fText.size())
				break;
			char c = fText[fPos];
			if (c == '+') {
				fPos++;
				value += _Term();
			} else if (c == '-') {
				fPos++;
				value -= _Term();
			} else if (isdigit((unsigned char)c) || c == '.' || c == '$'
				|| c == '(') {
				value += _Term();	// numbers without an operator are added
			} else
				break;
		}
		return value;
	}

	double _Term()
	{
		double value = _Power();
		while (true) {
			_Skip();
			if (fPos >= fText.size())
				break;
			char c = fText[fPos];
			if (c == '*' || c == 'x' || c == 'X') {
				fPos++;
				value *= _Power();
			} else if (c == '/') {
				fPos++;
				double divisor = _Power();
				if (divisor == 0) {
					fError = true;
					return 0;
				}
				value /= divisor;
			} else
				break;
		}
		return value;
	}

	double _Power()
	{
		double value = _Unary();
		_Skip();
		if (fPos < fText.size() && fText[fPos] == '^') {
			fPos++;
			value = std::pow(value, _Power());
		}
		return value;
	}

	double _Unary()
	{
		_Skip();
		if (fPos < fText.size() && fText[fPos] == '-') {
			fPos++;
			return -_Unary();
		}
		if (fPos < fText.size() && fText[fPos] == '+') {
			fPos++;
			return _Unary();
		}
		double value = _Primary();
		_Skip();
		if (fPos < fText.size() && fText[fPos] == '%') {
			fPos++;
			value /= 100;
		}
		return value;
	}

	double _Primary()
	{
		_Skip();
		if (fPos >= fText.size()) {
			fError = true;
			return 0;
		}
		if (fText[fPos] == '(') {
			fPos++;
			size_t mark = fPos;
			double value = _Expression();
			_Skip();
			if (fPos < fText.size() && fText[fPos] == ')')
				fPos++;
			// "(5)" is a negative number, as in accounting
			std::string inside = fText.substr(mark, fPos - mark - 1);
			bool plainNumber = !inside.empty()
				&& inside.find_first_of("+-*/^()") == std::string::npos;
			return plainNumber ? -value : value;
		}
		if (fText[fPos] == '$') {
			fDollar = true;
			fPos++;
		}
		std::string digits;
		int decimals = -1;
		while (fPos < fText.size()) {
			char c = fText[fPos];
			if (isdigit((unsigned char)c)) {
				digits += c;
				if (decimals >= 0)
					decimals++;
			} else if (c == '.' && decimals < 0) {
				digits += c;
				decimals = 0;
			} else if (c == ',' && fPos + 1 < fText.size()
				&& isdigit((unsigned char)fText[fPos + 1]) && decimals < 0) {
				fCommas = true;
			} else
				break;
			fPos++;
		}
		if (digits.empty() || digits == ".") {
			fError = true;
			return 0;
		}
		fDecimals = std::max(fDecimals, std::max(0, decimals));
		return strtod(digits.c_str(), nullptr);
	}

	std::string	fText;
	size_t		fPos;
	int			fDecimals;
	bool		fDollar;
	bool		fCommas;
	bool		fError;
};

}	// namespace


bool
Calculate(const std::string& text, double* result, std::string* shown)
{
	Calculator calculator(text);
	double value;
	if (!calculator.Evaluate(&value))
		return false;
	*result = value;
	if (shown != nullptr) {
		int decimals = calculator.Decimals();
		if (decimals == 0 && std::fabs(value - std::round(value)) > 1e-9)
			decimals = 2;
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%.*f", decimals, std::fabs(value));
		std::string number = buffer;
		if (calculator.Commas()) {
			size_t dot = number.find('.');
			size_t integerEnd = dot == std::string::npos ? number.size() : dot;
			std::string grouped;
			for (size_t i = 0; i < integerEnd; i++) {
				if (i > 0 && (integerEnd - i) % 3 == 0)
					grouped += ',';
				grouped += number[i];
			}
			number = grouped + number.substr(integerEnd);
		}
		if (calculator.Dollar())
			number = "$" + number;
		if (value < 0)
			number = "-" + number;
		*shown = number;
	}
	return true;
}


// #pragma mark - numbering


static bool
IsRomanLetter(char c)
{
	return strchr("IVXLCDMivxlcdm", c) != nullptr && c != '\0';
}


bool
HasNumber(const std::string& text, size_t* length)
{
	size_t i = 0;
	if (i < text.size() && text[i] == '(')
		i++;
	size_t start = i;
	if (i < text.size() && isdigit((unsigned char)text[i])) {
		while (i < text.size() && (isdigit((unsigned char)text[i])
			|| (text[i] == '.' && i + 1 < text.size()
				&& isdigit((unsigned char)text[i + 1]))))
			i++;
	} else if (i < text.size() && IsRomanLetter(text[i])) {
		while (i < text.size() && IsRomanLetter(text[i]))
			i++;
	} else if (i < text.size() && isalpha((unsigned char)text[i])) {
		i++;
	}
	if (i == start || i >= text.size())
		return false;
	if (text[i] != '.' && text[i] != ')')
		return false;
	i++;
	if (i < text.size() && (text[i] == '\t' || text[i] == ' ')) {
		i++;
		*length = i;
		return true;
	}
	return false;
}


static std::string
NumberText(int format, int32_t number, const std::vector<int32_t>& outline)
{
	switch (format) {
		case kNumberArabicParen: return std::to_string(number) + ")";
		case kNumberUpperRoman:
			return FormatPageNumber(number, kPageUpperRoman) + ".";
		case kNumberLowerRoman:
			return FormatPageNumber(number, kPageLowerRoman) + ".";
		case kNumberUpperLetter:
			return FormatPageNumber(number, kPageUpperLetter) + ".";
		case kNumberLowerLetter:
			return FormatPageNumber(number, kPageLowerLetter) + ".";
		case kNumberOutline: {
			std::string text;
			for (size_t i = 0; i < outline.size(); i++) {
				if (i > 0)
					text += '.';
				text += std::to_string(outline[i]);
			}
			return text + ".";
		}
		default: return std::to_string(number) + ".";
	}
}


static void
RenumberRange(Editor& editor, int32_t first, int32_t last,
	const RenumberOptions& options)
{
	Story& story = editor.GetStory();
	Document* document = editor.GetDocument();
	editor.BeginBulk(options.remove ? "Remove Numbers" : "Renumber");
	int32_t number = options.start;
	std::vector<int32_t> levels(10, 0);
	bool firstOutline = true;
	for (int32_t p = first; p <= last; p++) {
		Paragraph& paragraph = story[p];
		if (paragraph.kind != kParaNormal || paragraph.text.empty())
			continue;
		size_t existing = 0;
		bool numbered = HasNumber(paragraph.text, &existing);
		if (options.remove) {
			if (numbered)
				paragraph.Erase(0, (int32_t)existing);
			continue;
		}
		if (options.onlyNumbered && !numbered)
			continue;
		std::vector<int32_t> outline;
		if (options.format == kNumberOutline) {
			int32_t level = document->styles.OutlineLevel(
				paragraph.format.style);
			if (level <= 0)
				continue;
			if (firstOutline) {
				levels[level] = options.start - 1;
				firstOutline = false;
			}
			levels[level]++;
			for (int32_t deeper = level + 1; deeper < 10; deeper++)
				levels[deeper] = 0;
			for (int32_t l = 1; l <= level; l++)
				outline.push_back(std::max<int32_t>(1, levels[l]));
		}
		CharFormat format = paragraph.FormatAt(0);
		format.footnote = 0;
		format.picture = 0;
		if (numbered)
			paragraph.Erase(0, (int32_t)existing);
		paragraph.Insert(0, NumberText(options.format, number, outline) + "\t",
			format);
		number++;
	}
	editor.EndBulk();
}


void
Renumber(Editor& editor, const RenumberOptions& options)
{
	Story& story = editor.GetStory();
	int32_t first = 0;
	int32_t last = (int32_t)story.size() - 1;
	if (editor.HasSelection()) {
		first = editor.FirstSelectedPara();
		last = editor.LastSelectedPara();
	}
	RenumberRange(editor, first, last, options);
}


static const char* kBullet = "\xE2\x80\xA2\t";


void
ToggleBullets(Editor& editor)
{
	Story& story = editor.GetStory();
	int32_t first = editor.FirstSelectedPara();
	int32_t last = editor.LastSelectedPara();
	bool all = true;
	for (int32_t p = first; p <= last; p++) {
		if (story[p].kind == kParaNormal
			&& story[p].text.compare(0, 4, kBullet) != 0)
			all = false;
	}
	Pos anchor = editor.Anchor();
	Pos caret = editor.Caret();
	editor.BeginBulk(all ? "Remove Bullets" : "Bullets");
	for (int32_t p = first; p <= last; p++) {
		Paragraph& paragraph = story[p];
		if (paragraph.kind != kParaNormal)
			continue;
		int32_t delta = 0;
		if (all) {
			paragraph.Erase(0, 4);
			delta = -4;
			if (paragraph.format.firstIndent < 0) {
				paragraph.format.leftIndent = std::max(0.0f,
					paragraph.format.leftIndent + paragraph.format.firstIndent);
				paragraph.format.firstIndent = 0;
			}
		} else if (paragraph.text.compare(0, 4, kBullet) != 0) {
			CharFormat format = paragraph.FormatAt(0);
			format.footnote = 0;
			format.picture = 0;
			paragraph.Insert(0, kBullet, format);
			delta = 4;
			if (paragraph.format.firstIndent >= 0) {
				paragraph.format.leftIndent += 18;
				paragraph.format.firstIndent = -18;
			}
		}
		paragraph.Touch();
		if (anchor.para == p)
			anchor.offset = std::max(0, anchor.offset + delta);
		if (caret.para == p)
			caret.offset = std::max(0, caret.offset + delta);
	}
	editor.Select(anchor, caret);
	editor.EndBulk();
}


void
ToggleNumbers(Editor& editor)
{
	Story& story = editor.GetStory();
	int32_t first = editor.FirstSelectedPara();
	int32_t last = editor.LastSelectedPara();
	bool all = true;
	for (int32_t p = first; p <= last; p++) {
		size_t length;
		if (story[p].kind == kParaNormal && !HasNumber(story[p].text, &length))
			all = false;
	}
	RenumberOptions options;
	options.remove = all;
	editor.BeginBulk(all ? "Remove Numbers" : "Numbers");
	RenumberRange(editor, first, last, options);
	for (int32_t p = first; p <= last; p++) {
		ParaFormat& format = story[p].format;
		if (story[p].kind != kParaNormal)
			continue;
		if (!all && format.firstIndent >= 0) {
			format.leftIndent += 18;
			format.firstIndent = -18;
		} else if (all && format.firstIndent < 0) {
			format.leftIndent = std::max(0.0f,
				format.leftIndent + format.firstIndent);
			format.firstIndent = 0;
		}
		story[p].Touch();
	}
	editor.Select(Pos(first, 0), Pos(last, story[last].Length()));
	editor.EndBulk();
}


void
Nest(Editor& editor, bool in)
{
	float tab = std::max(1.0f, editor.GetDocument()->settings.defaultTab);
	editor.ApplyParaFormat([tab, in](ParaFormat& format) {
		float stops = format.leftIndent / tab;
		if (in)
			format.leftIndent = (std::floor(stops + 0.001f) + 1) * tab;
		else
			format.leftIndent = std::max(0.0f,
				(std::ceil(stops - 0.001f) - 1) * tab);
	});
}


void
SetHangingIndent(Editor& editor)
{
	float tab = std::max(1.0f, editor.GetDocument()->settings.defaultTab);
	editor.ApplyParaFormat([tab](ParaFormat& format) {
		format.leftIndent += tab;
		format.firstIndent -= tab;
	});
}


// #pragma mark - text and tables


std::vector<std::pair<int32_t, int32_t> >
SplitTableFields(const std::string& text, int format)
{
	std::vector<std::pair<int32_t, int32_t> > fields;
	int32_t length = (int32_t)text.size();
	if (format == kTableParagraphs) {
		fields.push_back(std::make_pair(0, length));
		return fields;
	}
	char separator = format == kTableTabs ? '\t' : ',';
	int32_t start = 0;
	bool quoted = false;
	for (int32_t i = 0; i <= length; i++) {
		if (i == length) {
			fields.push_back(std::make_pair(start, length));
			break;
		}
		char c = text[i];
		if (format == kTableCommas && c == '"') {
			// a quote opens a field only at its start (after spaces)
			bool atStart = true;
			for (int32_t k = start; k < i; k++) {
				if (text[k] != ' ')
					atStart = false;
			}
			if (quoted) {
				if (i + 1 < length && text[i + 1] == '"')
					i++;	// doubled quote inside a quoted field
				else
					quoted = false;
			} else if (atStart)
				quoted = true;
			continue;
		}
		if (c == separator && !quoted) {
			fields.push_back(std::make_pair(start, i));
			start = i + 1;
		}
	}
	return fields;
}


static int32_t
CountFields(const std::string& text, int format)
{
	return (int32_t)SplitTableFields(text, format).size();
}


TextTableAnalysis
AnalyzeTextForTable(const Editor& editor)
{
	TextTableAnalysis analysis;
	const Story& story = editor.GetStory();
	analysis.first = editor.FirstSelectedPara();
	analysis.last = editor.LastSelectedPara();
	analysis.convertible = true;
	bool tabs = false;
	bool commas = false;
	for (int32_t p = analysis.first; p <= analysis.last; p++) {
		const Paragraph& paragraph = story[p];
		if (paragraph.IsTablePart())
			analysis.convertible = false;
		analysis.tabFields.push_back(CountFields(paragraph.text, kTableTabs));
		analysis.commaFields.push_back(CountFields(paragraph.text,
			kTableCommas));
		if (paragraph.text.find('\t') != std::string::npos)
			tabs = true;
		else if (paragraph.text.find(',') != std::string::npos)
			commas = true;
	}
	analysis.suggested = tabs ? kTableTabs
		: (commas ? kTableCommas : kTableParagraphs);
	return analysis;
}


int32_t
TextTableColumns(const TextTableAnalysis& analysis, int format)
{
	const std::vector<int32_t>& fields = format == kTableTabs
		? analysis.tabFields : analysis.commaFields;
	if (format == kTableParagraphs || fields.empty())
		return 1;
	return std::max<int32_t>(1, *std::max_element(fields.begin(),
		fields.end()));
}


int32_t
TextTableRows(const TextTableAnalysis& analysis, int format, int32_t columns)
{
	columns = std::max<int32_t>(1, columns);
	int32_t paragraphs = analysis.last - analysis.first + 1;
	if (paragraphs <= 0)
		return 0;
	if (format == kTableParagraphs)
		return (paragraphs + columns - 1) / columns;
	const std::vector<int32_t>& fields = format == kTableTabs
		? analysis.tabFields : analysis.commaFields;
	int32_t rows = 0;
	for (int32_t count : fields)
		rows += std::max<int32_t>(1, (count + columns - 1) / columns);
	return rows;
}


// Makes a cell from part of a paragraph, keeping its character formats.
static Paragraph
MakeCell(const Paragraph& source, int32_t start, int32_t end, int format)
{
	Paragraph cell = source.Slice(start, end);
	if (format == kTableCommas) {
		// spaces around the field and CSV quotes are not part of the text
		while (cell.Length() > 0 && cell.text[0] == ' ')
			cell.Erase(0, 1);
		while (cell.Length() > 0 && cell.text[cell.Length() - 1] == ' ')
			cell.Erase(cell.Length() - 1, cell.Length());
		if (cell.Length() >= 2 && cell.text[0] == '"'
			&& cell.text[cell.Length() - 1] == '"') {
			cell.Erase(cell.Length() - 1, cell.Length());
			cell.Erase(0, 1);
			for (int32_t i = 0; i + 1 < cell.Length(); i++) {
				if (cell.text[i] == '"' && cell.text[i + 1] == '"')
					cell.Erase(i, i + 1);
			}
		}
	}
	cell.kind = kParaCell;
	cell.format.inTable = true;
	cell.format.leftIndent = 0;
	cell.format.firstIndent = 0;
	cell.format.rightIndent = 0;
	cell.format.pageBreakBefore = false;
	cell.format.keepWithNext = false;
	cell.format.border = ParaBorder();
	cell.format.tabs.clear();
	cell.Touch();
	return cell;
}


bool
ConvertTextToTable(Editor& editor, int format, int32_t columns,
	float columnWidth, float availableWidth)
{
	TextTableAnalysis analysis = AnalyzeTextForTable(editor);
	if (!analysis.convertible)
		return false;
	columns = std::max<int32_t>(1, std::min<int32_t>(columns, 31));
	Story& story = editor.GetStory();
	Document* document = editor.GetDocument();

	RowFormat row;
	row.gap = 5.4f;
	row.leftEdge = -row.gap;
	float width = columnWidth > 0 ? columnWidth
		: std::max(18.0f, availableWidth / columns);
	for (int32_t c = 0; c < columns; c++) {
		CellFormat cell;
		cell.right = (c + 1) * width + row.gap;
		row.cells.push_back(cell);
	}

	std::vector<Paragraph> table;
	std::vector<Paragraph> cells;
	auto emptyCell = [&](const Paragraph& like) {
		Paragraph cell = like.Slice(like.Length(), like.Length());
		cell.text.clear();
		cell = MakeCell(cell, 0, 0, kTableParagraphs);
		return cell;
	};
	auto finishRow = [&](const Paragraph& like) {
		while ((int32_t)cells.size() < columns)
			cells.push_back(emptyCell(like));
		for (Paragraph& cell : cells)
			table.push_back(cell);
		Paragraph end = document->NewParagraph(like.format.style);
		end.kind = kParaRowEnd;
		end.format.inTable = true;
		end.row = row;
		end.SetMarkFormat(like.MarkFormat());
		table.push_back(end);
		cells.clear();
	};

	const Paragraph* lastSource = &story[analysis.last];
	for (int32_t p = analysis.first; p <= analysis.last; p++) {
		const Paragraph& paragraph = story[p];
		if (format == kTableParagraphs) {
			cells.push_back(MakeCell(paragraph, 0, paragraph.Length(),
				format));
			if ((int32_t)cells.size() == columns)
				finishRow(paragraph);
			continue;
		}
		// every paragraph starts a row; long ones continue on new rows
		for (const auto& field : SplitTableFields(paragraph.text, format)) {
			cells.push_back(MakeCell(paragraph, field.first, field.second,
				format));
			if ((int32_t)cells.size() == columns)
				finishRow(paragraph);
		}
		if (!cells.empty())
			finishRow(paragraph);
	}
	if (!cells.empty())
		finishRow(*lastSource);

	editor.BeginBulk("Text to Table");
	Paragraph after = document->NewParagraph(lastSource->format.style);
	after.SetMarkFormat(lastSource->MarkFormat());
	bool atEnd = analysis.last + 1 >= (int32_t)story.size();
	story.erase(story.begin() + analysis.first,
		story.begin() + analysis.last + 1);
	story.insert(story.begin() + analysis.first, table.begin(), table.end());
	if (atEnd)
		story.push_back(after);
	editor.Select(Pos(analysis.first, 0), Pos(analysis.first, 0));
	editor.EndBulk();
	return true;
}


bool
ConvertTableToText(Editor& editor, int format)
{
	Story& story = editor.GetStory();
	int32_t count = (int32_t)story.size();
	int32_t first = editor.FirstSelectedPara();
	int32_t last = editor.LastSelectedPara();
	if (!story[first].IsTablePart() && !story[last].IsTablePart())
		return false;
	auto rowStart = [&](int32_t p) {
		if (story[p].kind == kParaRowEnd && p > 0
			&& story[p - 1].kind == kParaCell)
			p--;
		while (p > 0 && story[p - 1].kind == kParaCell)
			p--;
		return p;
	};
	auto rowEnd = [&](int32_t p) {
		while (p < count && story[p].kind == kParaCell)
			p++;
		return p;
	};
	int32_t start;
	int32_t end;
	bool spansCells = editor.SelStart().para != editor.SelEnd().para;
	if (spansCells) {
		// the rows the selection touches
		start = rowStart(story[first].IsTablePart() ? first : last);
		end = rowEnd(story[last].IsTablePart() ? last : first);
	} else {
		// the whole table around the insertion point (or a selection
		// inside one cell)
		start = rowStart(first);
		while (start > 0 && story[start - 1].kind == kParaRowEnd)
			start = rowStart(start - 1);
		end = rowEnd(first);
		while (end + 1 < count && story[end + 1].kind == kParaCell)
			end = rowEnd(end + 1);
	}
	if (end >= count || story[end].kind != kParaRowEnd)
		return false;

	std::vector<Paragraph> text;
	int32_t p = start;
	while (p <= end) {
		std::vector<Paragraph> cells;
		while (p <= end && story[p].kind == kParaCell)
			cells.push_back(story[p++]);
		p++;	// the row end
		for (Paragraph& cell : cells) {
			cell.kind = kParaNormal;
			cell.format.inTable = false;
			cell.Touch();
		}
		if (cells.empty())
			continue;
		if (format == kTableParagraphs) {
			for (Paragraph& cell : cells)
				text.push_back(cell);
			continue;
		}
		const char* separator = format == kTableTabs ? "\t" : ",";
		Paragraph line = cells[0];
		for (size_t c = 1; c < cells.size(); c++) {
			CharFormat mark = line.MarkFormat();
			mark.footnote = 0;
			mark.picture = 0;
			std::string cellText = cells[c].text;
			bool quote = format == kTableCommas
				&& (cellText.find(',') != std::string::npos
					|| cellText.find('"') != std::string::npos);
			line.Insert(line.Length(), separator, mark);
			if (quote) {
				Paragraph quotedCell = cells[c];
				for (int32_t i = quotedCell.Length() - 1; i >= 0; i--) {
					if (quotedCell.text[i] == '"')
						quotedCell.Insert(i, "\"", quotedCell.FormatAt(i));
				}
				quotedCell.Insert(0, "\"", quotedCell.FormatAt(0));
				quotedCell.Insert(quotedCell.Length(), "\"",
					quotedCell.MarkFormat());
				line.Append(quotedCell);
			} else
				line.Append(cells[c]);
		}
		line.Touch();
		text.push_back(line);
	}
	if (text.empty())
		return false;

	editor.BeginBulk("Table to Text");
	story.erase(story.begin() + start, story.begin() + end + 1);
	story.insert(story.begin() + start, text.begin(), text.end());
	int32_t lastText = start + (int32_t)text.size() - 1;
	editor.Select(Pos(start, 0), Pos(lastText, story[lastText].Length()));
	editor.EndBulk();
	return true;
}


// #pragma mark - outline


int32_t
EnsureHeadingStyle(Document& document, int32_t level)
{
	level = std::max<int32_t>(1, std::min<int32_t>(level, 9));
	std::string name = "heading " + std::to_string(level);
	int32_t index = document.styles.Find(name);
	if (index >= 0)
		return index;
	Style style = document.styles.Get(0);
	style.name = name;
	style.basedOn = 0;
	style.next = 0;
	style.automatic = true;
	style.outlineLevel = level;
	style.cf.bold = level <= 5;
	style.cf.italic = level > 5;
	style.pf.keepWithNext = true;
	style.pf.leftIndent = level >= 4 ? 18 : 0;
	index = document.styles.Count();
	style.pf.style = index;
	document.styles.styles.push_back(style);
	return index;
}


void
OutlineChangeLevel(Editor& editor, int delta)
{
	Document* document = editor.GetDocument();
	Story& story = editor.GetStory();
	int32_t first = editor.FirstSelectedPara();
	int32_t last = editor.LastSelectedPara();
	Pos anchor = editor.Anchor();
	Pos caret = editor.Caret();
	editor.BeginBulk(delta < 0 ? "Promote" : "Demote", true);
	for (int32_t p = first; p <= last; p++) {
		if (story[p].IsTablePart())
			continue;
		int32_t level = document->styles.OutlineLevel(story[p].format.style);
		int32_t target;
		if (delta < 0) {
			if (level == 0) {
				// body text becomes a heading at the level of the one above
				target = 1;
				for (int32_t q = p - 1; q >= 0; q--) {
					int32_t above = document->styles.OutlineLevel(
						story[q].format.style);
					if (above > 0) {
						target = above;
						break;
					}
				}
			} else
				target = std::max<int32_t>(1, level - 1);
		} else {
			if (level == 0)
				continue;
			target = std::min<int32_t>(9, level + 1);
		}
		if (target == level)
			continue;
		int32_t style = EnsureHeadingStyle(*document, target);
		editor.Select(Pos(p, 0), Pos(p, 0));
		editor.ApplyStyle(style);
	}
	editor.Select(anchor, caret);
	editor.EndBulk();
}


void
OutlineToBody(Editor& editor)
{
	Story& story = editor.GetStory();
	int32_t first = editor.FirstSelectedPara();
	int32_t last = editor.LastSelectedPara();
	Pos anchor = editor.Anchor();
	Pos caret = editor.Caret();
	editor.BeginBulk("Body Text", true);
	for (int32_t p = first; p <= last; p++) {
		if (story[p].IsTablePart())
			continue;
		editor.Select(Pos(p, 0), Pos(p, 0));
		editor.ApplyStyle(0);
	}
	editor.Select(anchor, caret);
	editor.EndBulk();
}


void
OutlineMove(Editor& editor, bool up, const std::vector<bool>& visible)
{
	Story& story = editor.GetStory();
	int32_t count = (int32_t)story.size();
	if ((int32_t)visible.size() != count)
		return;
	int32_t first = editor.FirstSelectedPara();
	int32_t last = editor.LastSelectedPara();
	// a collapsed heading takes its hidden text along
	while (last + 1 < count && !visible[last + 1])
		last++;
	for (int32_t p = first; p <= last; p++) {
		if (story[p].IsTablePart())
			return;
	}
	int32_t destination;
	if (up) {
		int32_t previous = first - 1;
		while (previous >= 0 && !visible[previous])
			previous--;
		if (previous < 0)
			return;
		destination = previous;
	} else {
		int32_t next = last + 1;
		if (next >= count)
			return;
		int32_t end = next + 1;
		while (end < count && !visible[end])
			end++;
		// keep the final paragraph of the story last
		destination = end - (last - first + 1);
		if (end > count)
			return;
	}
	for (int32_t p = std::min(first, destination);
			p <= std::max(last, destination); p++) {
		if (p < count && story[p].IsTablePart())
			return;
	}
	Pos anchor = editor.Anchor();
	Pos caret = editor.Caret();
	editor.BeginBulk(up ? "Move Up" : "Move Down");
	std::vector<Paragraph> block(story.begin() + first,
		story.begin() + last + 1);
	story.erase(story.begin() + first, story.begin() + last + 1);
	story.insert(story.begin() + destination, block.begin(), block.end());
	int32_t shift = destination - first;
	anchor.para += shift;
	caret.para += shift;
	editor.Select(anchor, caret);
	editor.EndBulk();
}


void
OutlineSetCollapsed(Editor& editor, bool collapsed)
{
	Story& story = editor.GetStory();
	int32_t first = editor.FirstSelectedPara();
	int32_t last = editor.LastSelectedPara();
	for (int32_t p = first; p <= last; p++)
		story[p].collapsed = collapsed;
	editor.MarkChanged();
}


int32_t
EnsureTocStyle(Document& document, int32_t level)
{
	level = std::max<int32_t>(1, std::min<int32_t>(level, 9));
	std::string name = "toc " + std::to_string(level);
	int32_t index = document.styles.Find(name);
	if (index >= 0)
		return index;
	Style style = document.styles.Get(0);
	style.name = name;
	style.basedOn = 0;
	style.next = 0;
	style.automatic = true;
	style.outlineLevel = 0;
	style.pf.leftIndent = (level - 1) * 18.0f;
	style.pf.rightIndent = 0;
	style.pf.tabs.clear();
	TabStop tab;
	tab.pos = document.settings.TextWidth();
	tab.kind = kTabRight;
	tab.leader = kLeaderDot;
	style.pf.tabs.push_back(tab);
	index = document.styles.Count();
	style.pf.style = index;
	document.styles.styles.push_back(style);
	return index;
}


Story
BuildTableOfContents(Document& document, const std::vector<int32_t>& pages,
	int32_t maxLevel)
{
	Story fragment;
	const Story& body = document.body;
	for (int32_t p = 0; p < (int32_t)body.size(); p++) {
		const Paragraph& paragraph = body[p];
		if (paragraph.IsTablePart())
			continue;
		int32_t level = document.styles.OutlineLevel(paragraph.format.style);
		if (level <= 0 || (maxLevel > 0 && level > maxLevel))
			continue;
		// the heading's text without tabs, breaks and special characters
		std::string text;
		const std::string& source = paragraph.text;
		for (int32_t offset = 0; offset < (int32_t)source.size();) {
			uint32_t c = CharAt(source, offset);
			int32_t next = NextOffset(source, offset);
			if (c == '\t' || c == kLineBreakChar)
				text += ' ';
			else if (!IsSpecialChar(c) && c != kSoftHyphenChar)
				text.append(source, offset, next - offset);
			offset = next;
		}
		while (!text.empty() && text.back() == ' ')
			text.pop_back();
		if (text.empty())
			continue;
		int32_t style = EnsureTocStyle(document, level);
		Paragraph entry = document.NewParagraph(style);
		std::string page = p < (int32_t)pages.size()
			? std::to_string(pages[p]) : std::string("1");
		entry.Insert(0, text + "\t" + page, entry.FormatAt(0));
		fragment.push_back(entry);
	}
	return fragment;
}


Story
FragmentFromText(const std::string& text, const CharFormat& format,
	const ParaFormat& paragraphFormat)
{
	Story fragment;
	size_t begin = 0;
	while (true) {
		size_t stop = text.find('\n', begin);
		std::string line = text.substr(begin,
			stop == std::string::npos ? std::string::npos : stop - begin);
		Paragraph paragraph(format);
		paragraph.format = paragraphFormat;
		paragraph.format.inTable = false;
		paragraph.Insert(0, line, format);
		fragment.push_back(paragraph);
		if (stop == std::string::npos)
			break;
		begin = stop + 1;
	}
	return fragment;
}


std::string
FormatDate(time_t when, bool longForm)
{
	struct tm parts;
	localtime_r(&when, &parts);
	char buffer[96];
	if (longForm)
		strftime(buffer, sizeof(buffer), "%A, %B %e, %Y", &parts);
	else {
		snprintf(buffer, sizeof(buffer), "%d/%d/%02d", parts.tm_mon + 1,
			parts.tm_mday, parts.tm_year % 100);
	}
	std::string text = buffer;
	size_t doubleSpace = text.find("  ");
	if (doubleSpace != std::string::npos)
		text.erase(doubleSpace, 1);
	return text;
}


std::string
FormatTime(time_t when)
{
	struct tm parts;
	localtime_r(&when, &parts);
	int hour = parts.tm_hour % 12;
	if (hour == 0)
		hour = 12;
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%d:%02d %s", hour, parts.tm_min,
		parts.tm_hour < 12 ? "AM" : "PM");
	return buffer;
}

}	// namespace quill
