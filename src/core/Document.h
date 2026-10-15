/*
 * Quill - document model.
 *
 * A story is a list of paragraphs. A paragraph is UTF-8 text without a
 * terminator, a list of character format runs and a paragraph format.
 * Table rows are represented like Word does internally: one paragraph per
 * cell (ParaFormat::inTable, kind kCell) followed by a row end paragraph
 * (kind kRowEnd) that carries the RowFormat.
 */
#ifndef QUILL_DOCUMENT_H
#define QUILL_DOCUMENT_H

#include "Format.h"

#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace quill {

struct Run {
	int32_t		start;
	CharFormat	format;
};

enum ParaKind : uint8_t {
	kParaNormal = 0,
	kParaCell,
	kParaRowEnd
};

struct Paragraph {
	std::string			text;
	std::vector<Run>	runs;		// never empty; runs[0].start == 0
	ParaFormat			format;
	uint8_t				kind = kParaNormal;
	RowFormat			row;		// for kParaRowEnd
	uint64_t			revision;	// changes whenever the content does
	bool				collapsed = false;	// Outline view state, not saved

	Paragraph();
	explicit Paragraph(const CharFormat& format);

	int32_t Length() const { return (int32_t)text.size(); }
	bool IsTablePart() const { return kind != kParaNormal; }

	// Format of the character at offset; at or past the end, the format
	// of the paragraph mark (the last run).
	const CharFormat& FormatAt(int32_t offset) const;
	const CharFormat& MarkFormat() const { return runs.back().format; }
	int32_t RunIndexAt(int32_t offset) const;
	int32_t RunEnd(int32_t index) const;

	void Insert(int32_t offset, const std::string& text,
		const CharFormat& format);
	void Erase(int32_t start, int32_t end);
	void ApplyFormat(int32_t start, int32_t end,
		const std::function<void(CharFormat&)>& modify);
	void SetMarkFormat(const CharFormat& format);
	Paragraph SplitAt(int32_t offset);		// keeps [0, offset)
	void Append(const Paragraph& other);
	Paragraph Slice(int32_t start, int32_t end) const;

	void Touch();
	void Normalize();
};

typedef std::vector<Paragraph> Story;

struct Pos {
	int32_t	para = 0;
	int32_t	offset = 0;

	Pos() {}
	Pos(int32_t p, int32_t o) : para(p), offset(o) {}
	bool operator==(const Pos& o) const
		{ return para == o.para && offset == o.offset; }
	bool operator!=(const Pos& o) const { return !(*this == o); }
	bool operator<(const Pos& o) const
		{ return para < o.para || (para == o.para && offset < o.offset); }
	bool operator<=(const Pos& o) const { return !(o < *this); }
	bool operator>(const Pos& o) const { return o < *this; }
	bool operator>=(const Pos& o) const { return !(*this < o); }
};

struct Picture {
	std::string	type;			// "png", "jpeg", "bmp" ... (data format)
	std::string	data;			// encoded image
	int32_t		pixelWidth = 0;
	int32_t		pixelHeight = 0;
	float		width = 0;		// displayed size in points
	float		height = 0;
};

struct DocInfo {
	std::string	title;
	std::string	subject;
	std::string	author;
	std::string	version;
	std::string	keywords;
	std::string	comments;
	time_t		created = 0;
	time_t		revised = 0;
	int32_t		revisions = 0;
};

enum StoryKind {
	kStoryBody = 0,
	kStoryHeader,
	kStoryFooter,
	kStoryFirstHeader,
	kStoryFirstFooter,
	kStoryFootnote
};

struct Document {
	Story		body;
	Story		header;
	Story		footer;
	Story		firstHeader;
	Story		firstFooter;
	std::map<int32_t, Story> footnotes;
	int32_t		nextFootnoteId = 1;
	std::map<int32_t, std::shared_ptr<Picture> > pictures;
	int32_t		nextPictureId = 1;
	StyleSheet	styles;
	DocSettings	settings;
	DocInfo		info;
	uint64_t	changes = 0;		// incremented by every edit

	Document();
	void Reset(const std::string& serif, const std::string& sans);

	Story* StoryFor(int kind, int32_t footnoteId = 0);
	static bool StoryEmpty(const Story& story);

	CharFormat StyleCharFormat(int32_t style) const;
	ParaFormat StyleParaFormat(int32_t style) const;
	Paragraph NewParagraph(int32_t style) const;
	Story NewStory(int32_t style) const;

	int32_t AddPicture(std::shared_ptr<Picture> picture);
	const Picture* PictureFor(int32_t id) const;

	// Footnote numbers in the order of the references in the body.
	std::map<int32_t, int32_t> FootnoteNumbers() const;
	std::vector<int32_t> FootnoteOrder() const;

	// Plain text of a range (paragraphs separated by "\n").
	static std::string PlainText(const Story& story, Pos start, Pos end);
	static std::string PlainText(const Story& story);
	static Pos EndOf(const Story& story);
};

// Font used by documents when nothing else is specified; set by the
// application to an installed family.
extern std::string gDefaultSerif;
extern std::string gDefaultSans;

}	// namespace quill

#endif	// QUILL_DOCUMENT_H
