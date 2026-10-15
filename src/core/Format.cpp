/*
 * Quill - format structures.
 */
#include "Format.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace quill {

bool
CharFormat::operator==(const CharFormat& o) const
{
	return font == o.font && size == o.size && bold == o.bold
		&& italic == o.italic && strike == o.strike && outline == o.outline
		&& shadow == o.shadow && smallCaps == o.smallCaps
		&& allCaps == o.allCaps && hidden == o.hidden
		&& underline == o.underline && position == o.position
		&& spacing == o.spacing && color == o.color && picture == o.picture
		&& footnote == o.footnote;
}


void
CopyCharFields(CharFormat& to, const CharFormat& from, uint32_t mask)
{
	if (mask & kCFFont) to.font = from.font;
	if (mask & kCFSize) to.size = from.size;
	if (mask & kCFBold) to.bold = from.bold;
	if (mask & kCFItalic) to.italic = from.italic;
	if (mask & kCFUnderline) to.underline = from.underline;
	if (mask & kCFStrike) to.strike = from.strike;
	if (mask & kCFOutline) to.outline = from.outline;
	if (mask & kCFShadow) to.shadow = from.shadow;
	if (mask & kCFSmallCaps) to.smallCaps = from.smallCaps;
	if (mask & kCFAllCaps) to.allCaps = from.allCaps;
	if (mask & kCFHidden) to.hidden = from.hidden;
	if (mask & kCFPosition) to.position = from.position;
	if (mask & kCFSpacing) to.spacing = from.spacing;
	if (mask & kCFColor) to.color = from.color;
}


uint32_t
SameCharFields(const CharFormat& a, const CharFormat& b)
{
	uint32_t same = 0;
	if (a.font == b.font) same |= kCFFont;
	if (a.size == b.size) same |= kCFSize;
	if (a.bold == b.bold) same |= kCFBold;
	if (a.italic == b.italic) same |= kCFItalic;
	if (a.underline == b.underline) same |= kCFUnderline;
	if (a.strike == b.strike) same |= kCFStrike;
	if (a.outline == b.outline) same |= kCFOutline;
	if (a.shadow == b.shadow) same |= kCFShadow;
	if (a.smallCaps == b.smallCaps) same |= kCFSmallCaps;
	if (a.allCaps == b.allCaps) same |= kCFAllCaps;
	if (a.hidden == b.hidden) same |= kCFHidden;
	if (a.position == b.position) same |= kCFPosition;
	if (a.spacing == b.spacing) same |= kCFSpacing;
	if (a.color == b.color) same |= kCFColor;
	return same;
}


bool
ParaBorder::operator==(const ParaBorder& o) const
{
	return top == o.top && bottom == o.bottom && left == o.left
		&& right == o.right && between == o.between && shadow == o.shadow
		&& shading == o.shading && space == o.space;
}


bool
ParaFormat::operator==(const ParaFormat& o) const
{
	return align == o.align && leftIndent == o.leftIndent
		&& rightIndent == o.rightIndent && firstIndent == o.firstIndent
		&& lineRule == o.lineRule && lineValue == o.lineValue
		&& spaceBefore == o.spaceBefore && spaceAfter == o.spaceAfter
		&& tabs == o.tabs && keepWithNext == o.keepWithNext
		&& keepTogether == o.keepTogether
		&& pageBreakBefore == o.pageBreakBefore
		&& noLineNumbers == o.noLineNumbers && border == o.border
		&& style == o.style && inTable == o.inTable;
}


void
ParaFormat::SetTab(const TabStop& tab)
{
	ClearTab(tab.pos);
	auto it = std::lower_bound(tabs.begin(), tabs.end(), tab,
		[](const TabStop& a, const TabStop& b) { return a.pos < b.pos; });
	tabs.insert(it, tab);
}


void
ParaFormat::ClearTab(float pos)
{
	tabs.erase(std::remove_if(tabs.begin(), tabs.end(),
		[pos](const TabStop& t) { return std::fabs(t.pos - pos) < 0.5f; }),
		tabs.end());
}


const TabStop*
ParaFormat::TabAt(float pos, float tolerance) const
{
	for (const TabStop& tab : tabs) {
		if (std::fabs(tab.pos - pos) <= tolerance)
			return &tab;
	}
	return nullptr;
}


void
CopyParaFields(ParaFormat& to, const ParaFormat& from, uint32_t mask)
{
	if (mask & kPFAlign) to.align = from.align;
	if (mask & kPFLeft) to.leftIndent = from.leftIndent;
	if (mask & kPFRight) to.rightIndent = from.rightIndent;
	if (mask & kPFFirst) to.firstIndent = from.firstIndent;
	if (mask & kPFLine) {
		to.lineRule = from.lineRule;
		to.lineValue = from.lineValue;
	}
	if (mask & kPFBefore) to.spaceBefore = from.spaceBefore;
	if (mask & kPFAfter) to.spaceAfter = from.spaceAfter;
	if (mask & kPFTabs) to.tabs = from.tabs;
	if (mask & kPFKeepNext) to.keepWithNext = from.keepWithNext;
	if (mask & kPFKeepTogether) to.keepTogether = from.keepTogether;
	if (mask & kPFBreakBefore) to.pageBreakBefore = from.pageBreakBefore;
	if (mask & kPFNoLineNum) to.noLineNumbers = from.noLineNumbers;
	if (mask & kPFBorder) to.border = from.border;
	if (mask & kPFStyle) to.style = from.style;
}


uint32_t
SameParaFields(const ParaFormat& a, const ParaFormat& b)
{
	uint32_t same = 0;
	if (a.align == b.align) same |= kPFAlign;
	if (a.leftIndent == b.leftIndent) same |= kPFLeft;
	if (a.rightIndent == b.rightIndent) same |= kPFRight;
	if (a.firstIndent == b.firstIndent) same |= kPFFirst;
	if (a.lineRule == b.lineRule && a.lineValue == b.lineValue)
		same |= kPFLine;
	if (a.spaceBefore == b.spaceBefore) same |= kPFBefore;
	if (a.spaceAfter == b.spaceAfter) same |= kPFAfter;
	if (a.tabs == b.tabs) same |= kPFTabs;
	if (a.keepWithNext == b.keepWithNext) same |= kPFKeepNext;
	if (a.keepTogether == b.keepTogether) same |= kPFKeepTogether;
	if (a.pageBreakBefore == b.pageBreakBefore) same |= kPFBreakBefore;
	if (a.noLineNumbers == b.noLineNumbers) same |= kPFNoLineNum;
	if (a.border == b.border) same |= kPFBorder;
	if (a.style == b.style) same |= kPFStyle;
	return same;
}


bool
CellFormat::operator==(const CellFormat& o) const
{
	return right == o.right && top == o.top && bottom == o.bottom
		&& left == o.left && rightLine == o.rightLine && shading == o.shading;
}


bool
RowFormat::operator==(const RowFormat& o) const
{
	return leftEdge == o.leftEdge && gap == o.gap && height == o.height
		&& align == o.align && cells == o.cells;
}


float
DocSettings::ColumnWidth() const
{
	int32_t count = std::max<int32_t>(1, columns);
	float width = (TextWidth() - (count - 1) * columnSpacing) / count;
	return std::max(36.0f, width);
}


bool
DocSettings::operator==(const DocSettings& o) const
{
	return pageWidth == o.pageWidth && pageHeight == o.pageHeight
		&& landscape == o.landscape && scale == o.scale
		&& marginTop == o.marginTop && marginBottom == o.marginBottom
		&& marginLeft == o.marginLeft && marginRight == o.marginRight
		&& gutter == o.gutter && mirrorMargins == o.mirrorMargins
		&& defaultTab == o.defaultTab && widowControl == o.widowControl
		&& printHidden == o.printHidden && footnotePos == o.footnotePos
		&& footnoteStart == o.footnoteStart
		&& footnoteRestartEachPage == o.footnoteRestartEachPage
		&& sectionStart == o.sectionStart && columns == o.columns
		&& columnSpacing == o.columnSpacing && headerY == o.headerY
		&& footerY == o.footerY && titlePage == o.titlePage
		&& pageNumberStart == o.pageNumberStart
		&& pageNumberFormat == o.pageNumberFormat
		&& marginPageNumbers == o.marginPageNumbers
		&& pageNumberTop == o.pageNumberTop
		&& pageNumberRight == o.pageNumberRight
		&& lineNumbering == o.lineNumbering
		&& lineNumberDistance == o.lineNumberDistance;
}


bool
Style::operator==(const Style& o) const
{
	return name == o.name && basedOn == o.basedOn && next == o.next
		&& cf == o.cf && pf == o.pf && automatic == o.automatic
		&& outlineLevel == o.outlineLevel;
}


int32_t
StyleSheet::Find(const std::string& name) const
{
	for (size_t i = 0; i < styles.size(); i++) {
		if (strcasecmp(styles[i].name.c_str(), name.c_str()) == 0)
			return (int32_t)i;
	}
	return -1;
}


const Style&
StyleSheet::Get(int32_t index) const
{
	if (index < 0 || index >= (int32_t)styles.size())
		return styles[0];
	return styles[index];
}


int32_t
StyleSheet::NextStyle(int32_t index) const
{
	const Style& style = Get(index);
	if (style.next >= 0 && style.next < Count())
		return style.next;
	return index >= 0 && index < Count() ? index : 0;
}


int32_t
StyleSheet::OutlineLevel(int32_t index) const
{
	return Get(index).outlineLevel;
}


static void
AppendItem(std::string& text, const std::string& item)
{
	if (!text.empty())
		text += ", ";
	text += item;
}


std::string
StyleSheet::Describe(int32_t index, int unit) const
{
	const Style& style = Get(index);
	CharFormat baseCF;
	ParaFormat basePF;
	std::string text;
	if (style.basedOn >= 0 && style.basedOn < Count()
		&& style.basedOn != index) {
		baseCF = styles[style.basedOn].cf;
		basePF = styles[style.basedOn].pf;
		text = styles[style.basedOn].name + " +";
	} else {
		// a style of its own lists its font and size first
		baseCF.font = style.cf.font;
		baseCF.size = style.cf.size;
	}

	std::string items;
	const CharFormat& cf = style.cf;
	if (cf.font != baseCF.font)
		AppendItem(items, "Font: " + cf.font);
	if (cf.size != baseCF.size) {
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%g Point", cf.size);
		AppendItem(items, buffer);
	}
	if (cf.bold != baseCF.bold) AppendItem(items, cf.bold ? "Bold" : "Not Bold");
	if (cf.italic != baseCF.italic)
		AppendItem(items, cf.italic ? "Italic" : "Not Italic");
	if (cf.underline != baseCF.underline) {
		static const char* kNames[] = { "No Underline", "Underline",
			"Word Underline", "Double Underline", "Dotted Underline" };
		AppendItem(items, kNames[cf.underline % 5]);
	}
	if (cf.strike != baseCF.strike && cf.strike) AppendItem(items, "Strikethru");
	if (cf.outline != baseCF.outline && cf.outline) AppendItem(items, "Outline");
	if (cf.shadow != baseCF.shadow && cf.shadow) AppendItem(items, "Shadow");
	if (cf.smallCaps != baseCF.smallCaps && cf.smallCaps)
		AppendItem(items, "Small Caps");
	if (cf.allCaps != baseCF.allCaps && cf.allCaps)
		AppendItem(items, "All Caps");
	if (cf.hidden != baseCF.hidden && cf.hidden) AppendItem(items, "Hidden");
	if (cf.position != baseCF.position) {
		char buffer[48];
		snprintf(buffer, sizeof(buffer), "%s %g pt",
			cf.position > 0 ? "Superscript" : "Subscript",
			std::fabs(cf.position));
		AppendItem(items, buffer);
	}
	if (cf.color != baseCF.color) {
		int named = NamedColorIndex(cf.color);
		AppendItem(items, std::string("Color: ")
			+ (named >= 0 ? kNamedColors[named].name : "Custom"));
	}

	const ParaFormat& pf = style.pf;
	if (pf.align != basePF.align) {
		static const char* kNames[] = { "Flush Left", "Centered",
			"Flush Right", "Justified" };
		AppendItem(items, kNames[pf.align % 4]);
	}
	if (pf.leftIndent != basePF.leftIndent
		|| pf.firstIndent != basePF.firstIndent
		|| pf.rightIndent != basePF.rightIndent) {
		std::string indent = "Indent:";
		if (pf.leftIndent != basePF.leftIndent)
			indent += " Left " + FormatMeasure(pf.leftIndent, unit);
		if (pf.firstIndent != basePF.firstIndent)
			indent += " First " + FormatMeasure(pf.firstIndent, unit);
		if (pf.rightIndent != basePF.rightIndent)
			indent += " Right " + FormatMeasure(pf.rightIndent, unit);
		AppendItem(items, indent);
	}
	if (pf.lineRule != basePF.lineRule || pf.lineValue != basePF.lineValue) {
		char buffer[64];
		if (pf.lineRule == kLineAuto)
			snprintf(buffer, sizeof(buffer), "Line Spacing %g li", pf.lineValue);
		else {
			snprintf(buffer, sizeof(buffer), "Line Spacing %s %g pt",
				pf.lineRule == kLineAtLeast ? "At Least" : "Exactly",
				pf.lineValue);
		}
		AppendItem(items, buffer);
	}
	if (pf.spaceBefore != basePF.spaceBefore)
		AppendItem(items, "Space Before " + FormatPoints(pf.spaceBefore));
	if (pf.spaceAfter != basePF.spaceAfter)
		AppendItem(items, "Space After " + FormatPoints(pf.spaceAfter));
	if (pf.keepWithNext != basePF.keepWithNext && pf.keepWithNext)
		AppendItem(items, "Keep With Next");
	if (pf.keepTogether != basePF.keepTogether && pf.keepTogether)
		AppendItem(items, "Keep Lines Together");
	if (pf.pageBreakBefore != basePF.pageBreakBefore && pf.pageBreakBefore)
		AppendItem(items, "Page Break Before");
	if (!(pf.tabs == basePF.tabs) && !pf.tabs.empty()) {
		std::string tabs = "Tabs:";
		static const char* kKinds[] = { "", " (Centered)", " (Right Flush)",
			" (Decimal)", " (Vertical Line)" };
		for (const TabStop& tab : pf.tabs)
			tabs += " " + FormatMeasure(tab.pos, unit) + kKinds[tab.kind % 5];
		AppendItem(items, tabs);
	}
	if (pf.border != basePF.border && pf.border.Any())
		AppendItem(items, "Border");

	if (text.empty()) {
		// A style that is not based on another one lists its font.
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%g Point", cf.size);
		text = cf.font + " " + buffer;
		if (!items.empty())
			text += ", " + items;
		return text;
	}
	if (items.empty())
		return text.substr(0, text.size() - 2);
	return text + " " + items;
}


StyleSheet
StyleSheet::Default(const std::string& serif, const std::string& sans)
{
	StyleSheet sheet;

	Style normal;
	normal.name = "Normal";
	normal.cf.font = serif;
	normal.cf.size = 12;
	normal.automatic = true;
	sheet.styles.push_back(normal);

	Style h1 = normal;
	h1.name = "heading 1";
	h1.basedOn = 0;
	h1.next = 0;
	h1.cf.font = sans;
	h1.cf.bold = true;
	h1.cf.underline = kULSingle;
	h1.pf.spaceBefore = 12;
	h1.pf.keepWithNext = true;
	h1.outlineLevel = 1;
	sheet.styles.push_back(h1);

	Style h2 = normal;
	h2.name = "heading 2";
	h2.basedOn = 0;
	h2.next = 0;
	h2.cf.font = sans;
	h2.cf.bold = true;
	h2.pf.spaceBefore = 6;
	h2.pf.keepWithNext = true;
	h2.outlineLevel = 2;
	sheet.styles.push_back(h2);

	Style h3 = normal;
	h3.name = "heading 3";
	h3.basedOn = 0;
	h3.next = 0;
	h3.cf.bold = true;
	h3.pf.leftIndent = 18;
	h3.pf.keepWithNext = true;
	h3.outlineLevel = 3;
	sheet.styles.push_back(h3);

	Style header = normal;
	header.name = "header";
	header.basedOn = 0;
	TabStop center;
	center.pos = 216;
	center.kind = kTabCenter;
	TabStop right;
	right.pos = 432;
	right.kind = kTabRight;
	header.pf.tabs.push_back(center);
	header.pf.tabs.push_back(right);
	sheet.styles.push_back(header);

	Style footer = header;
	footer.name = "footer";
	sheet.styles.push_back(footer);

	Style footnote = normal;
	footnote.name = "footnote text";
	footnote.basedOn = 0;
	footnote.cf.size = 10;
	sheet.styles.push_back(footnote);

	return sheet;
}


// #pragma mark - units


float
UnitToPoints(int unit)
{
	switch (unit) {
		case kUnitCm: return 72.0f / 2.54f;
		case kUnitPoint: return 1.0f;
		case kUnitPica: return 12.0f;
		default: return 72.0f;
	}
}


static std::string
TrimNumber(double value, int decimals)
{
	char buffer[48];
	snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
	std::string text = buffer;
	if (text.find('.') != std::string::npos) {
		while (!text.empty() && text.back() == '0')
			text.pop_back();
		if (!text.empty() && text.back() == '.')
			text.pop_back();
	}
	if (text == "-0")
		text = "0";
	return text;
}


std::string
FormatMeasure(float points, int unit)
{
	switch (unit) {
		case kUnitCm:
			return TrimNumber(points / UnitToPoints(kUnitCm), 2) + " cm";
		case kUnitPoint:
			return TrimNumber(points, 1) + " pt";
		case kUnitPica:
			return TrimNumber(points / 12.0, 2) + " pi";
		default:
			return TrimNumber(points / 72.0, 2) + " in";
	}
}


std::string
FormatPoints(float points)
{
	return TrimNumber(points, 1) + " pt";
}


bool
ParseMeasure(const std::string& text, int defaultUnit, float* points)
{
	const char* start = text.c_str();
	while (*start == ' ')
		start++;
	if (*start == '\0') {
		*points = 0;
		return true;
	}
	char* end = nullptr;
	double value = strtod(start, &end);
	if (end == start)
		return false;
	while (*end == ' ')
		end++;
	std::string suffix(end);
	while (!suffix.empty() && suffix.back() == ' ')
		suffix.pop_back();
	for (char& c : suffix)
		c = (char)tolower((unsigned char)c);

	double factor;
	if (suffix.empty())
		factor = UnitToPoints(defaultUnit);
	else if (suffix == "in" || suffix == "\"" || suffix == "inch"
		|| suffix == "inches")
		factor = 72;
	else if (suffix == "cm")
		factor = 72 / 2.54;
	else if (suffix == "mm")
		factor = 72 / 25.4;
	else if (suffix == "pt" || suffix == "pts" || suffix == "point"
		|| suffix == "points")
		factor = 1;
	else if (suffix == "pi" || suffix == "pica" || suffix == "picas")
		factor = 12;
	else if (suffix == "li" || suffix == "line" || suffix == "lines")
		factor = 12;
	else
		return false;
	*points = (float)(value * factor);
	return true;
}


const float kStandardSizes[] = { 7, 9, 10, 12, 14, 18, 24, 36, 48, 60, 72 };
const int kStandardSizeCount = sizeof(kStandardSizes) / sizeof(float);


float
NextLargerSize(float size)
{
	for (int i = 0; i < kStandardSizeCount; i++) {
		if (kStandardSizes[i] > size)
			return kStandardSizes[i];
	}
	return std::min(size + 12.0f, 1638.0f);
}


float
NextSmallerSize(float size)
{
	for (int i = kStandardSizeCount - 1; i >= 0; i--) {
		if (kStandardSizes[i] < size)
			return kStandardSizes[i];
	}
	return std::max(size - 1.0f, 4.0f);
}


const NamedColor kNamedColors[] = {
	{ "Auto", kColorAuto },
	{ "Black", 0x000000 },
	{ "Blue", 0x0000FF },
	{ "Cyan", 0x00FFFF },
	{ "Green", 0x00FF00 },
	{ "Magenta", 0xFF00FF },
	{ "Red", 0xFF0000 },
	{ "Yellow", 0xFFFF00 },
	{ "White", 0xFFFFFF }
};
const int kNamedColorCount = sizeof(kNamedColors) / sizeof(NamedColor);


int
NamedColorIndex(uint32_t color)
{
	for (int i = 0; i < kNamedColorCount; i++) {
		if (kNamedColors[i].rgb == color)
			return i;
	}
	return -1;
}


static std::string
Roman(int number, bool upper)
{
	static const int kValues[] = { 1000, 900, 500, 400, 100, 90, 50, 40,
		10, 9, 5, 4, 1 };
	static const char* kUpper[] = { "M", "CM", "D", "CD", "C", "XC", "L",
		"XL", "X", "IX", "V", "IV", "I" };
	static const char* kLower[] = { "m", "cm", "d", "cd", "c", "xc", "l",
		"xl", "x", "ix", "v", "iv", "i" };
	std::string text;
	for (int i = 0; i < 13 && number > 0; i++) {
		while (number >= kValues[i]) {
			text += upper ? kUpper[i] : kLower[i];
			number -= kValues[i];
		}
	}
	return text;
}


std::string
FormatPageNumber(int number, int format)
{
	if (number < 1 && format != kPageArabic)
		number = 1;
	switch (format) {
		case kPageUpperRoman: return Roman(number, true);
		case kPageLowerRoman: return Roman(number, false);
		case kPageUpperLetter:
		case kPageLowerLetter: {
			// A..Z, AA..ZZ, ... like Word
			char letter = (char)((format == kPageUpperLetter ? 'A' : 'a')
				+ (number - 1) % 26);
			return std::string((number - 1) / 26 + 1, letter);
		}
		default:
			return std::to_string(number);
	}
}

}	// namespace quill
