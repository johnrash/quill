/*
 * Quill - line layout and pagination.
 *
 * Layout works in points and is independent of zoom and of the view mode:
 * every line gets a position in the Normal view (one continuous column)
 * and in the Page Layout view (page, column, position on the page).
 */
#ifndef QUILL_LAYOUT_H
#define QUILL_LAYOUT_H

#include "Document.h"

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace quill {

class FontMetrics {
public:
	virtual					~FontMetrics() {}
	// Advance widths in points of the count characters of the UTF-8 text.
	virtual	void			Widths(const CharFormat& format, float size,
								const char* text, int32_t bytes,
								int32_t count, float* widths) = 0;
	virtual	void			Heights(const CharFormat& format, float size,
								float* ascent, float* descent,
								float* leading) = 0;
};

struct LayoutOptions {
	bool		showHidden = false;
	int32_t		pageNumber = 1;
	int32_t		pageCount = 1;
	std::string	date;
	std::string	time;
	const std::map<int32_t, int32_t>* footnoteNumbers = nullptr;
	// Outline view: headings indented by level, body text below them,
	// collapsed headings hide what is subordinate to them.
	bool		outline = false;
	int32_t		outlineShowLevel = 0;	// 0: all levels and body text
	bool		outlineFirstLine = false;	// body text: first line only
	bool		outlineFormatting = true;
};

struct OutlineInfo {
	int32_t	level = 0;			// 1-9 headings, 0 body text
	int32_t	indent = 0;			// outline indent steps
	bool	visible = true;
	bool	hasChildren = false;	// heading with subordinate paragraphs
};

enum GlyphFlags : uint8_t {
	kGlyphHidden	= 1 << 0,	// not shown, zero width
	kGlyphSmall		= 1 << 1,	// small caps: lowercase drawn as capitals
	kGlyphHyphen	= 1 << 2,	// optional hyphen shown at a line end
	kGlyphTab		= 1 << 3,
	kGlyphField		= 1 << 4,	// drawn from ParaLayout::fieldText
	kGlyphPicture	= 1 << 5,
	kGlyphBreak		= 1 << 6,	// line, page or section break
	kGlyphSpace		= 1 << 7
};

struct LineBox {
	int32_t	startCp = 0;
	int32_t	endCp = 0;
	float	top = 0;		// text top relative to the paragraph top
	float	height = 0;
	float	baseline = 0;	// relative to the line top
	float	ascent = 0;
	float	descent = 0;
	float	left = 0;		// x where the line starts (indent)
	float	endX = 0;		// x after the last character, trailing spaces too
	float	contentRight = 0;
	bool	forced = false;	// ends with a line, page or section break
	bool	pageBreak = false;
	bool	sectionBreak = false;
	bool	last = false;
};

struct ParaLayout {
	uint64_t	revision = 0;
	float		width = 0;
	std::string	key;			// options the layout depends on
	std::vector<int32_t>	cpByte;	// byte offset of each character, + end
	std::vector<uint32_t>	cps;
	std::vector<float>		x;		// x of each character
	std::vector<float>		w;		// displayed advance of each character
	std::vector<int32_t>	runOf;	// run index of each character
	std::vector<uint8_t>	flags;
	std::map<int32_t, std::string> fieldText;
	std::map<int32_t, uint8_t>	tabLeader;
	std::vector<float>		barTabs;
	std::vector<LineBox>	lines;
	float		height = 0;
	float		spaceBefore = 0;
	float		spaceAfter = 0;
	float		padTop = 0;
	float		padBottom = 0;
	bool		borderTop = false;		// draw the top border line
	bool		borderBottom = false;
	bool		borderBetween = false;	// draw the between line on top
	float		textLeft = 0;			// leftmost indent
	float		textRight = 0;			// right indent edge
	bool		invisible = false;		// collapsed away in Outline view

	int32_t		Count() const { return (int32_t)cps.size(); }
	int32_t		CpForByte(int32_t byte) const;
	int32_t		LineForCp(int32_t cp, bool lineEnd) const;
	float		CaretX(int32_t cp, int32_t line) const;
	bool		LineHasFootnotes(int32_t line) const;
};

void LayoutParagraph(const Paragraph& paragraph, const Document& document,
	FontMetrics& metrics, const LayoutOptions& options, float width,
	bool borderTop, bool borderBottom, bool borderBetween, ParaLayout& out);

float BorderLineWidth(uint8_t line);

struct PlacedLine {
	int32_t	para = 0;
	int32_t	line = 0;
	int32_t	page = 0;
	int32_t	column = 0;
	float	normalTop = 0;	// Normal view, continuous
	float	normalX = 0;	// frame origin in Normal view (cell offset)
	float	pageTop = 0;	// relative to the page's top edge
	float	pageX = 0;		// frame origin relative to the page's left edge
	float	height = 0;
};

struct Block {
	int32_t	firstPara = 0;
	int32_t	lastPara = 0;	// row end paragraph for table rows
	bool	row = false;
	float	normalTop = 0;
	float	normalBottom = 0;
	int32_t	page = 0;
	float	pageTop = 0;
	float	pageBottom = 0;
	float	pageX = 0;		// frame origin of the row/paragraph on the page
};

struct FootnotePlace {
	int32_t	id = 0;
	int32_t	page = 0;
	int32_t	column = 0;
	float	pageTop = 0;
	float	pageX = 0;
	bool	separator = false;	// draw the separator line above
};

struct PageInfo {
	int32_t	firstLine = 0;	// index into PlacedLine array
	int32_t	endLine = 0;
	int32_t	number = 1;		// page number as printed
};

enum {
	kGeomNormal = 0,
	kGeomPage = 1
};

struct CaretInfo {
	int32_t	page = 0;
	float	x = 0;
	float	top = 0;
	float	bottom = 0;
	float	baseline = 0;
};

struct SelectionRect {
	int32_t	page;
	float	left;
	float	top;
	float	right;
	float	bottom;
};

class DocLayout {
public:
							DocLayout(FontMetrics* metrics);

			void			SetStory(Document* document, Story* story,
								bool paginate);
			void			SetOptions(const LayoutOptions& options);
			const LayoutOptions& Options() const { return fOptions; }
			void			SetWidth(float width);	// 0: from the document
			void			Invalidate();
			bool			Update();	// returns true if anything changed

			Document*		GetDocument() const { return fDocument; }
			Story*			GetStory() const { return fStory; }
			int32_t			ParaCount() const
								{ return (int32_t)fParas.size(); }
			const ParaLayout& Para(int32_t index) const
								{ return *fParas[index]; }
			const std::vector<PlacedLine>& Lines() const { return fLines; }
			const std::vector<Block>& Blocks() const { return fBlocks; }
			const std::vector<PageInfo>& Pages() const { return fPages; }
			const std::vector<FootnotePlace>& FootnotePlaces() const
								{ return fFootnotePlaces; }
			const std::vector<ParaLayout>* FootnoteLayout(int32_t id) const;
			const OutlineInfo* Outline(int32_t para) const
								{ return para >= 0
									&& para < (int32_t)fOutline.size()
									? &fOutline[para] : nullptr; }
			bool			IsOutline() const { return fOptions.outline; }
			int32_t			FirstLineOf(int32_t para) const
								{ return fParaFirstLine[para]; }
			const PlacedLine& Placed(int32_t para, int32_t line) const
								{ return fLines[fParaFirstLine[para] + line]; }
			float			NormalHeight() const { return fNormalHeight; }
			int32_t			PageCount() const
								{ return (int32_t)fPages.size(); }
			float			LayoutWidth() const { return fWidth; }
			int32_t			PageOfPos(Pos pos) const;
			int32_t			LineCount() const
								{ return (int32_t)fLines.size(); }

	// Geometry. Normal coordinates: x from the text column's left edge,
	// y from the top of the story. Page coordinates: from the page's
	// top left corner.
			bool			Caret(Pos pos, int geometry, bool lineEnd,
								CaretInfo* info) const;
			Pos				HitTest(int geometry, int32_t page, float x,
								float y, bool* lineEnd) const;
			void			SelectionRects(Pos start, Pos end, int geometry,
								float markWidth,
								std::vector<SelectionRect>& rects) const;
			Pos				LineStart(Pos pos, bool lineEnd) const;
			Pos				LineEnd(Pos pos, bool lineEnd,
								bool* endAffinity) const;
			Pos				MoveVertical(Pos pos, bool lineEnd,
								int geometry, float goalX, int lines,
								bool* newLineEnd) const;
			int32_t			LineIndexOf(Pos pos, bool lineEnd) const;
			float			PageTextLeft(int32_t page) const;
			int32_t			FindBlock(int geometry, int32_t page,
								float y) const;

private:
			const ParaLayout* _LayoutPara(int32_t index, float width,
								bool top, bool bottom, bool between,
								bool* changed);
			void			_Place();
			void			_LayoutFootnotes(bool* changed);
			float			_CellWidth(const RowFormat& row, int index) const;
			std::string		_KeyFor(const Paragraph& paragraph) const;

			FontMetrics*	fMetrics;
			Document*		fDocument;
			Story*			fStory;
			bool			fPaginate;
			LayoutOptions	fOptions;
			float			fRequestedWidth;
			float			fWidth;
			uint64_t		fGeneration;

			std::unordered_map<uint64_t, ParaLayout> fCache;
			std::vector<const ParaLayout*> fParas;
			std::vector<PlacedLine>	fLines;
			std::vector<int32_t>	fParaFirstLine;
			std::vector<Block>		fBlocks;
			std::vector<PageInfo>	fPages;
			std::vector<FootnotePlace> fFootnotePlaces;
			std::map<int32_t, std::vector<ParaLayout> > fFootnotes;
			std::map<int32_t, uint64_t> fFootnoteRevisions;
			std::map<int32_t, int32_t> fFootnoteNumbersStorage;
			std::vector<OutlineInfo> fOutline;
			bool			fPaginating;
			float			fNormalHeight;
			std::string		fLastSettingsKey;
};

}	// namespace quill

#endif	// QUILL_LAYOUT_H
