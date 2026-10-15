/*
 * Quill - document model.
 */
#include "Document.h"
#include "Text.h"

#include <algorithm>
#include <atomic>

namespace quill {

std::string gDefaultSerif = "Noto Serif";
std::string gDefaultSans = "Noto Sans";

static std::atomic<uint64_t> sRevision(0);


Paragraph::Paragraph()
{
	CharFormat format;
	format.font = gDefaultSerif;
	runs.push_back(Run{0, format});
	Touch();
}


Paragraph::Paragraph(const CharFormat& format)
{
	runs.push_back(Run{0, format});
	Touch();
}


void
Paragraph::Touch()
{
	revision = ++sRevision;
}


int32_t
Paragraph::RunIndexAt(int32_t offset) const
{
	int32_t low = 0;
	int32_t high = (int32_t)runs.size() - 1;
	while (low < high) {
		int32_t middle = (low + high + 1) / 2;
		if (runs[middle].start <= offset)
			low = middle;
		else
			high = middle - 1;
	}
	return low;
}


int32_t
Paragraph::RunEnd(int32_t index) const
{
	if (index + 1 < (int32_t)runs.size())
		return runs[index + 1].start;
	return Length();
}


const CharFormat&
Paragraph::FormatAt(int32_t offset) const
{
	if (offset >= Length())
		return runs.back().format;
	return runs[RunIndexAt(offset)].format;
}


void
Paragraph::Normalize()
{
	int32_t length = Length();
	if (runs.empty())
		runs.push_back(Run{0, CharFormat()});
	if (length > 0) {
		while (runs.size() > 1 && runs.back().start >= length)
			runs.pop_back();
	} else if (runs.size() > 1) {
		Run keep = runs.back();
		runs.clear();
		runs.push_back(keep);
	}
	runs[0].start = 0;

	std::vector<Run> merged;
	merged.reserve(runs.size());
	for (const Run& run : runs) {
		if (!merged.empty() && merged.back().start == run.start) {
			merged.back() = run;		// zero length run: the later wins
			continue;
		}
		if (!merged.empty() && merged.back().format == run.format)
			continue;
		merged.push_back(run);
	}
	// Merging after replacing can leave equal neighbours; one more pass.
	runs.clear();
	for (const Run& run : merged) {
		if (!runs.empty() && runs.back().format == run.format)
			continue;
		runs.push_back(run);
	}
}


void
Paragraph::Insert(int32_t offset, const std::string& insert,
	const CharFormat& format)
{
	if (insert.empty())
		return;
	int32_t length = Length();
	offset = std::max<int32_t>(0, std::min(offset, length));
	int32_t count = (int32_t)insert.size();
	if (length == 0) {
		text = insert;
		runs.clear();
		runs.push_back(Run{0, format});
		Touch();
		return;
	}

	std::vector<Run> result;
	result.reserve(runs.size() + 2);
	for (const Run& run : runs) {
		if (run.start < offset)
			result.push_back(run);
	}
	result.push_back(Run{offset, format});
	if (offset < length) {
		int32_t containing = RunIndexAt(offset);
		if (runs[containing].start < offset)
			result.push_back(Run{offset + count, runs[containing].format});
		for (const Run& run : runs) {
			if (run.start >= offset)
				result.push_back(Run{run.start + count, run.format});
		}
	}
	text.insert(offset, insert);
	runs.swap(result);
	Normalize();
	Touch();
}


void
Paragraph::Erase(int32_t start, int32_t end)
{
	int32_t length = Length();
	start = std::max<int32_t>(0, start);
	end = std::min(end, length);
	if (start >= end)
		return;
	int32_t count = end - start;
	std::vector<Run> result;
	result.reserve(runs.size());
	for (const Run& run : runs) {
		int32_t newStart;
		if (run.start < start)
			newStart = run.start;
		else if (run.start < end)
			newStart = start;
		else
			newStart = run.start - count;
		if (!result.empty() && result.back().start == newStart)
			result.back() = Run{newStart, run.format};
		else
			result.push_back(Run{newStart, run.format});
	}
	text.erase(start, count);
	runs.swap(result);
	Normalize();
	Touch();
}


void
Paragraph::ApplyFormat(int32_t start, int32_t end,
	const std::function<void(CharFormat&)>& modify)
{
	int32_t length = Length();
	if (length == 0) {
		modify(runs[0].format);
		Touch();
		return;
	}
	start = std::max<int32_t>(0, start);
	end = std::min(end, length);
	if (start >= end)
		return;

	// Make sure runs begin at start and at end.
	for (int32_t split : { start, end }) {
		if (split <= 0 || split >= length)
			continue;
		int32_t index = RunIndexAt(split);
		if (runs[index].start != split) {
			runs.insert(runs.begin() + index + 1,
				Run{split, runs[index].format});
		}
	}
	for (Run& run : runs) {
		if (run.start >= start && run.start < end)
			modify(run.format);
	}
	Normalize();
	Touch();
}


void
Paragraph::SetMarkFormat(const CharFormat& format)
{
	if (Length() == 0) {
		runs.clear();
		runs.push_back(Run{0, format});
		Touch();
	}
}


Paragraph
Paragraph::SplitAt(int32_t offset)
{
	int32_t length = Length();
	offset = std::max<int32_t>(0, std::min(offset, length));
	Paragraph tail(FormatAt(offset));
	tail.format = format;
	tail.kind = kind;
	tail.row = row;
	if (offset < length) {
		tail.text = text.substr(offset);
		tail.runs.clear();
		for (int32_t i = 0; i < (int32_t)runs.size(); i++) {
			if (RunEnd(i) > offset) {
				tail.runs.push_back(Run{std::max<int32_t>(0,
					runs[i].start - offset), runs[i].format});
			}
		}
		tail.Normalize();
	}

	if (offset == 0) {
		CharFormat first = runs[0].format;
		runs.clear();
		runs.push_back(Run{0, first});
	} else {
		std::vector<Run> head;
		for (const Run& run : runs) {
			if (run.start < offset)
				head.push_back(run);
		}
		runs.swap(head);
	}
	text.resize(offset);
	Normalize();
	Touch();
	tail.Touch();
	return tail;
}


void
Paragraph::Append(const Paragraph& other)
{
	if (other.text.empty()) {
		Touch();
		return;
	}
	if (text.empty()) {
		text = other.text;
		runs = other.runs;
		Touch();
		return;
	}
	int32_t base = Length();
	text += other.text;
	for (const Run& run : other.runs)
		runs.push_back(Run{run.start + base, run.format});
	Normalize();
	Touch();
}


Paragraph
Paragraph::Slice(int32_t start, int32_t end) const
{
	int32_t length = Length();
	start = std::max<int32_t>(0, std::min(start, length));
	end = std::max(start, std::min(end, length));
	Paragraph slice(FormatAt(start));
	slice.format = format;
	slice.kind = kind;
	slice.row = row;
	if (start < end) {
		slice.text = text.substr(start, end - start);
		slice.runs.clear();
		for (int32_t i = 0; i < (int32_t)runs.size(); i++) {
			int32_t runEnd = RunEnd(i);
			if (runs[i].start < end && runEnd > start) {
				slice.runs.push_back(Run{std::max(runs[i].start, start) - start,
					runs[i].format});
			}
		}
		slice.Normalize();
	}
	slice.Touch();
	return slice;
}


// #pragma mark - Document


Document::Document()
{
	Reset(gDefaultSerif, gDefaultSans);
}


void
Document::Reset(const std::string& serif, const std::string& sans)
{
	styles = StyleSheet::Default(serif, sans);
	settings = DocSettings();
	info = DocInfo();
	body = NewStory(0);
	int32_t headerStyle = std::max<int32_t>(0, styles.Find("header"));
	int32_t footerStyle = std::max<int32_t>(0, styles.Find("footer"));
	header = NewStory(headerStyle);
	footer = NewStory(footerStyle);
	firstHeader = NewStory(headerStyle);
	firstFooter = NewStory(footerStyle);
	footnotes.clear();
	nextFootnoteId = 1;
	pictures.clear();
	nextPictureId = 1;
}


Story*
Document::StoryFor(int kind, int32_t footnoteId)
{
	switch (kind) {
		case kStoryHeader: return &header;
		case kStoryFooter: return &footer;
		case kStoryFirstHeader: return &firstHeader;
		case kStoryFirstFooter: return &firstFooter;
		case kStoryFootnote: {
			auto it = footnotes.find(footnoteId);
			return it == footnotes.end() ? nullptr : &it->second;
		}
		default: return &body;
	}
}


bool
Document::StoryEmpty(const Story& story)
{
	for (const Paragraph& paragraph : story) {
		if (!paragraph.text.empty())
			return false;
	}
	return true;
}


CharFormat
Document::StyleCharFormat(int32_t style) const
{
	CharFormat format = styles.Get(style).cf;
	if (format.font.empty())
		format.font = gDefaultSerif;
	return format;
}


ParaFormat
Document::StyleParaFormat(int32_t style) const
{
	ParaFormat format = styles.Get(style).pf;
	format.style = style >= 0 && style < styles.Count() ? style : 0;
	return format;
}


Paragraph
Document::NewParagraph(int32_t style) const
{
	Paragraph paragraph(StyleCharFormat(style));
	paragraph.format = StyleParaFormat(style);
	return paragraph;
}


Story
Document::NewStory(int32_t style) const
{
	Story story;
	story.push_back(NewParagraph(style));
	return story;
}


int32_t
Document::AddPicture(std::shared_ptr<Picture> picture)
{
	int32_t id = nextPictureId++;
	pictures[id] = picture;
	return id;
}


const Picture*
Document::PictureFor(int32_t id) const
{
	auto it = pictures.find(id);
	return it == pictures.end() ? nullptr : it->second.get();
}


std::vector<int32_t>
Document::FootnoteOrder() const
{
	std::vector<int32_t> order;
	for (const Paragraph& paragraph : body) {
		for (int32_t i = 0; i < (int32_t)paragraph.runs.size(); i++) {
			const Run& run = paragraph.runs[i];
			if (run.format.footnote == 0)
				continue;
			int32_t end = paragraph.RunEnd(i);
			for (int32_t offset = run.start; offset < end;
					offset = NextOffset(paragraph.text, offset)) {
				if (CharAt(paragraph.text, offset) == kFootnoteChar)
					order.push_back(run.format.footnote);
			}
		}
	}
	return order;
}


std::map<int32_t, int32_t>
Document::FootnoteNumbers() const
{
	std::map<int32_t, int32_t> numbers;
	int32_t number = settings.footnoteStart;
	for (int32_t id : FootnoteOrder()) {
		if (numbers.find(id) == numbers.end())
			numbers[id] = number++;
	}
	return numbers;
}


std::string
Document::PlainText(const Story& story, Pos start, Pos end)
{
	std::string text;
	if (story.empty() || end <= start)
		return text;
	for (int32_t p = start.para; p <= end.para && p < (int32_t)story.size();
			p++) {
		const std::string& paragraph = story[p].text;
		int32_t from = p == start.para ? start.offset : 0;
		int32_t to = p == end.para ? end.offset : (int32_t)paragraph.size();
		from = std::min<int32_t>(from, paragraph.size());
		to = std::min<int32_t>(to, paragraph.size());
		if (to > from)
			text.append(paragraph, from, to - from);
		if (p != end.para)
			text += '\n';
	}
	return text;
}


std::string
Document::PlainText(const Story& story)
{
	return PlainText(story, Pos(0, 0), EndOf(story));
}


Pos
Document::EndOf(const Story& story)
{
	if (story.empty())
		return Pos(0, 0);
	return Pos((int32_t)story.size() - 1, story.back().Length());
}

}	// namespace quill
