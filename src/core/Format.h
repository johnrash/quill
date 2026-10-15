/*
 * Quill - character, paragraph, section and document formats.
 *
 * Portable code: no Haiku headers. All measurements are in points
 * (1/72 inch); RTF twips are converted at the file boundary.
 */
#ifndef QUILL_FORMAT_H
#define QUILL_FORMAT_H

#include <cstdint>
#include <string>
#include <vector>

namespace quill {

enum UnderlineKind : uint8_t {
	kULNone = 0,
	kULSingle,
	kULWord,
	kULDouble,
	kULDotted
};

static const uint32_t kColorAuto = 0xFF000000u;

struct CharFormat {
	std::string	font;
	float		size = 12.0f;
	bool		bold = false;
	bool		italic = false;
	bool		strike = false;
	bool		outline = false;
	bool		shadow = false;
	bool		smallCaps = false;
	bool		allCaps = false;
	bool		hidden = false;
	uint8_t		underline = kULNone;
	float		position = 0;		// points raised (+) or lowered (-)
	float		spacing = 0;		// points added (+) or removed (-) per char
	uint32_t	color = kColorAuto;	// 0xRRGGBB or kColorAuto
	int32_t		picture = 0;		// picture id for kPictureChar
	int32_t		footnote = 0;		// footnote id for kFootnoteChar

	bool operator==(const CharFormat& other) const;
	bool operator!=(const CharFormat& other) const
		{ return !(*this == other); }
};

// Fields of CharFormat, used to apply or compare only some of them.
enum CharField : uint32_t {
	kCFFont			= 1 << 0,
	kCFSize			= 1 << 1,
	kCFBold			= 1 << 2,
	kCFItalic		= 1 << 3,
	kCFUnderline	= 1 << 4,
	kCFStrike		= 1 << 5,
	kCFOutline		= 1 << 6,
	kCFShadow		= 1 << 7,
	kCFSmallCaps	= 1 << 8,
	kCFAllCaps		= 1 << 9,
	kCFHidden		= 1 << 10,
	kCFPosition		= 1 << 11,
	kCFSpacing		= 1 << 12,
	kCFColor		= 1 << 13,
	kCFAll			= (1 << 14) - 1
};

void CopyCharFields(CharFormat& to, const CharFormat& from, uint32_t mask);
uint32_t SameCharFields(const CharFormat& a, const CharFormat& b);

enum Alignment : uint8_t {
	kAlignLeft = 0,
	kAlignCenter,
	kAlignRight,
	kAlignJustify
};

enum TabKind : uint8_t {
	kTabLeft = 0,
	kTabCenter,
	kTabRight,
	kTabDecimal,
	kTabBar
};

enum TabLeader : uint8_t {
	kLeaderNone = 0,
	kLeaderDot,
	kLeaderHyphen,
	kLeaderLine
};

struct TabStop {
	float	pos = 0;
	uint8_t	kind = kTabLeft;
	uint8_t	leader = kLeaderNone;

	bool operator==(const TabStop& o) const
		{ return pos == o.pos && kind == o.kind && leader == o.leader; }
};

enum LineRule : uint8_t {
	kLineAuto = 0,		// lineValue is a multiple of single spacing
	kLineAtLeast,		// lineValue in points
	kLineExactly		// lineValue in points
};

enum BorderLine : uint8_t {
	kBorderNone = 0,
	kBorderSingle,
	kBorderThick,
	kBorderDouble,
	kBorderDotted,
	kBorderHairline
};

struct ParaBorder {
	uint8_t	top = kBorderNone;
	uint8_t	bottom = kBorderNone;
	uint8_t	left = kBorderNone;
	uint8_t	right = kBorderNone;
	uint8_t	between = kBorderNone;
	bool	shadow = false;
	uint8_t	shading = 0;		// percent gray, 0 = none
	float	space = 2;			// distance from text to the line

	bool Any() const
		{ return top || bottom || left || right || between || shading; }
	bool operator==(const ParaBorder& o) const;
	bool operator!=(const ParaBorder& o) const { return !(*this == o); }
};

struct ParaFormat {
	uint8_t		align = kAlignLeft;
	float		leftIndent = 0;
	float		rightIndent = 0;
	float		firstIndent = 0;		// relative to leftIndent
	uint8_t		lineRule = kLineAuto;
	float		lineValue = 1.0f;
	float		spaceBefore = 0;
	float		spaceAfter = 0;
	std::vector<TabStop> tabs;			// sorted by position
	bool		keepWithNext = false;
	bool		keepTogether = false;
	bool		pageBreakBefore = false;
	bool		noLineNumbers = false;
	ParaBorder	border;
	int32_t		style = 0;				// index into the style sheet
	bool		inTable = false;

	bool operator==(const ParaFormat& other) const;
	bool operator!=(const ParaFormat& other) const
		{ return !(*this == other); }

	void SetTab(const TabStop& tab);
	void ClearTab(float pos);
	const TabStop* TabAt(float pos, float tolerance) const;
};

enum ParaField : uint32_t {
	kPFAlign		= 1 << 0,
	kPFLeft			= 1 << 1,
	kPFRight		= 1 << 2,
	kPFFirst		= 1 << 3,
	kPFLine			= 1 << 4,
	kPFBefore		= 1 << 5,
	kPFAfter		= 1 << 6,
	kPFTabs			= 1 << 7,
	kPFKeepNext		= 1 << 8,
	kPFKeepTogether	= 1 << 9,
	kPFBreakBefore	= 1 << 10,
	kPFNoLineNum	= 1 << 11,
	kPFBorder		= 1 << 12,
	kPFStyle		= 1 << 13,
	kPFAll			= (1 << 14) - 1
};

void CopyParaFields(ParaFormat& to, const ParaFormat& from, uint32_t mask);
uint32_t SameParaFields(const ParaFormat& a, const ParaFormat& b);

// One cell of a table row: its right edge measured from the row's left
// edge, and the borders drawn around it.
struct CellFormat {
	float	right = 144;
	uint8_t	top = kBorderNone;
	uint8_t	bottom = kBorderNone;
	uint8_t	left = kBorderNone;
	uint8_t	rightLine = kBorderNone;
	uint8_t	shading = 0;

	bool operator==(const CellFormat& o) const;
};

struct RowFormat {
	float	leftEdge = 0;		// relative to the left margin
	float	gap = 5.4f;			// half the space between cell texts
	float	height = 0;			// 0 auto, >0 at least, <0 exactly
	uint8_t	align = kAlignLeft;
	std::vector<CellFormat> cells;

	bool operator==(const RowFormat& o) const;
	float CellLeft(int index) const
		{ return index == 0 ? 0 : cells[index - 1].right; }
};

enum PageNumberFormat : uint8_t {
	kPageArabic = 0,
	kPageUpperRoman,
	kPageLowerRoman,
	kPageUpperLetter,
	kPageLowerLetter
};

enum SectionStart : uint8_t {
	kStartNoBreak = 0,
	kStartNewColumn,
	kStartNewPage,
	kStartEvenPage,
	kStartOddPage
};

enum FootnotePosition : uint8_t {
	kFootnoteBottom = 0,
	kFootnoteBeneath,
	kFootnoteEndOfDocument
};

struct DocSettings {
	// Page Setup
	float	pageWidth = 612;
	float	pageHeight = 792;
	bool	landscape = false;
	int32_t	scale = 100;
	// Document dialog
	float	marginTop = 72;
	float	marginBottom = 72;
	float	marginLeft = 90;
	float	marginRight = 90;
	float	gutter = 0;
	bool	mirrorMargins = false;
	float	defaultTab = 36;
	bool	widowControl = true;
	bool	printHidden = false;
	uint8_t	footnotePos = kFootnoteBottom;
	int32_t	footnoteStart = 1;
	bool	footnoteRestartEachPage = false;
	// Section dialog (one section per document)
	uint8_t	sectionStart = kStartNewPage;
	int32_t	columns = 1;
	float	columnSpacing = 36;
	float	headerY = 36;
	float	footerY = 36;
	bool	titlePage = false;
	int32_t	pageNumberStart = 1;
	uint8_t	pageNumberFormat = kPageArabic;
	bool	marginPageNumbers = false;
	float	pageNumberTop = 36;
	float	pageNumberRight = 36;
	int32_t	lineNumbering = 0;	// 0 off, 1 by page, 2 continuous
	float	lineNumberDistance = 18;

	float PaperWidth() const
		{ return landscape ? pageHeight : pageWidth; }
	float PaperHeight() const
		{ return landscape ? pageWidth : pageHeight; }
	float TextWidth() const
		{ return PaperWidth() - marginLeft - marginRight - gutter; }
	float TextHeight() const
		{ return PaperHeight() - marginTop - marginBottom; }
	float ColumnWidth() const;
	bool operator==(const DocSettings& o) const;
	bool operator!=(const DocSettings& o) const { return !(*this == o); }
};

struct Style {
	std::string	name;
	int32_t		basedOn = -1;	// index or -1
	int32_t		next = -1;		// index or -1 (same style)
	CharFormat	cf;
	ParaFormat	pf;
	bool		automatic = false;	// built-in style, cannot be deleted
	int32_t		outlineLevel = 0;	// 1-9 for headings

	bool operator==(const Style& o) const;
};

struct StyleSheet {
	std::vector<Style> styles;

	int32_t Find(const std::string& name) const;
	int32_t Count() const { return (int32_t)styles.size(); }
	const Style& Get(int32_t index) const;
	int32_t NextStyle(int32_t index) const;
	int32_t OutlineLevel(int32_t index) const;
	// Describes the style the way Word's style dialog does:
	// "Normal + Font: Helvetica, Bold, Indent: Left 0.5 in".
	std::string Describe(int32_t index, int unit) const;

	static StyleSheet Default(const std::string& serif,
		const std::string& sans);
	bool operator==(const StyleSheet& o) const { return styles == o.styles; }
};

// Measurement units used in dialogs and on the ruler.
enum Unit {
	kUnitInch = 0,
	kUnitCm,
	kUnitPoint,
	kUnitPica
};

std::string FormatMeasure(float points, int unit);
std::string FormatPoints(float points);			// "12 pt"
bool ParseMeasure(const std::string& text, int defaultUnit, float* points);
float UnitToPoints(int unit);

// Word's standard font sizes for the Font menu, Up and Down.
extern const float kStandardSizes[];
extern const int kStandardSizeCount;
float NextLargerSize(float size);
float NextSmallerSize(float size);

// The eight named colors of Word's Character dialog.
struct NamedColor {
	const char*	name;
	uint32_t	rgb;
};
extern const NamedColor kNamedColors[];
extern const int kNamedColorCount;
int NamedColorIndex(uint32_t color);	// -1 if not one of them

std::string FormatPageNumber(int number, int format);

}	// namespace quill

#endif	// QUILL_FORMAT_H
