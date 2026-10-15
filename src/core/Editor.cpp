/*
 * Quill - editing operations with undo.
 */
#include "Editor.h"
#include "Text.h"

#include <algorithm>
#include <cstring>

namespace quill {

static const size_t kMaxUndo = 200;


Editor::Editor(Document* document, Story* story)
	:
	fDocument(document),
	fStory(story),
	fTypingSet(false),
	fCoalesce(false),
	fPendingCount(0),
	fPendingCoalesced(false),
	fBulkDepth(0),
	fChangeCount(0)
{
	fPendingFirst = 0;
	fPendingNewLast = 0;
}


// #pragma mark - selection


Pos
Editor::Clamp(Pos pos) const
{
	const Story& story = *fStory;
	if (story.empty())
		return Pos();
	pos.para = std::max<int32_t>(0, std::min<int32_t>(pos.para,
		(int32_t)story.size() - 1));
	const std::string& text = story[pos.para].text;
	pos.offset = std::max<int32_t>(0, std::min<int32_t>(pos.offset,
		(int32_t)text.size()));
	while (pos.offset > 0 && pos.offset < (int32_t)text.size()
		&& ((unsigned char)text[pos.offset] & 0xC0) == 0x80)
		pos.offset--;
	return pos;
}


Pos
Editor::End() const
{
	return Document::EndOf(*fStory);
}


void
Editor::Select(Pos anchor, Pos caret)
{
	fAnchor = Clamp(anchor);
	fCaret = Clamp(caret);
	fTypingSet = false;
	fCoalesce = false;
}


void
Editor::SelectAll()
{
	Select(Pos(0, 0), End());
}


int32_t
Editor::FirstSelectedPara() const
{
	return SelStart().para;
}


int32_t
Editor::LastSelectedPara() const
{
	Pos start = SelStart();
	Pos end = SelEnd();
	if (end.para > start.para && end.offset == 0)
		return end.para - 1;
	return end.para;
}


// #pragma mark - formats


CharFormat
Editor::InsertionFormat() const
{
	if (fTypingSet)
		return fTypingFormat;
	const Story& story = *fStory;
	Pos start = SelStart();
	const Paragraph& paragraph = story[start.para];
	CharFormat format;
	if (HasSelection() || start.offset == 0) {
		format = paragraph.FormatAt(start.offset);
	} else {
		int32_t previous = PrevOffset(paragraph.text, start.offset);
		uint32_t c = CharAt(paragraph.text, previous);
		if (c == kFootnoteChar || c == kPictureChar) {
			if (start.offset < paragraph.Length())
				format = paragraph.FormatAt(start.offset);
			else
				format = fDocument->StyleCharFormat(paragraph.format.style);
		} else
			format = paragraph.FormatAt(previous);
	}
	format.picture = 0;
	format.footnote = 0;
	return format;
}


void
Editor::SetTypingFormat(const CharFormat& format)
{
	fTypingFormat = format;
	fTypingFormat.picture = 0;
	fTypingFormat.footnote = 0;
	fTypingSet = true;
}


CharFormat
Editor::SelectionCharFormat(uint32_t* sameMask) const
{
	if (!HasSelection()) {
		if (sameMask)
			*sameMask = kCFAll;
		return InsertionFormat();
	}
	const Story& story = *fStory;
	Pos start = SelStart();
	Pos end = SelEnd();
	CharFormat first;
	bool have = false;
	uint32_t same = kCFAll;
	for (int32_t p = start.para; p <= end.para; p++) {
		const Paragraph& paragraph = story[p];
		int32_t from = p == start.para ? start.offset : 0;
		int32_t to = p == end.para ? end.offset : paragraph.Length();
		if (from >= to)
			continue;
		for (int32_t i = paragraph.RunIndexAt(from);
				i < (int32_t)paragraph.runs.size()
					&& paragraph.runs[i].start < to; i++) {
			if (!have) {
				first = paragraph.runs[i].format;
				have = true;
			} else
				same &= SameCharFields(first, paragraph.runs[i].format);
		}
	}
	if (!have)
		first = InsertionFormat();
	if (sameMask)
		*sameMask = same;
	return first;
}


ParaFormat
Editor::SelectionParaFormat(uint32_t* sameMask) const
{
	const Story& story = *fStory;
	int32_t first = FirstSelectedPara();
	int32_t last = LastSelectedPara();
	ParaFormat format = story[first].format;
	uint32_t same = kPFAll;
	for (int32_t p = first + 1; p <= last; p++)
		same &= SameParaFields(format, story[p].format);
	if (sameMask)
		*sameMask = same;
	return format;
}


// #pragma mark - undo machinery


void
Editor::MarkChanged()
{
	fChangeCount++;
	fDocument->changes++;
}


void
Editor::_BeginStep(const char* name, int kind, int32_t first, int32_t last)
{
	fPendingFirst = first;
	fPendingCount = last - first + 1;
	fPendingCoalesced = false;
	if (_InBulk())
		return;
	fRedo.clear();
	if (kind == kUndoTyping && fCoalesce && !fUndo.empty()) {
		UndoStep& top = fUndo.back();
		if (top.kind == kUndoTyping && first >= top.first
			&& last < top.first + top.count) {
			fPendingCoalesced = true;
			return;
		}
	}
	if (kind == kUndoTyping)
		fLastTyped.clear();

	UndoStep step;
	step.name = name;
	step.kind = kind;
	step.first = first;
	step.count = last - first + 1;
	step.saved.assign(fStory->begin() + first, fStory->begin() + last + 1);
	step.beforeAnchor = fAnchor;
	step.beforeCaret = fCaret;
	fUndo.push_back(std::move(step));
	if (fUndo.size() > kMaxUndo)
		fUndo.erase(fUndo.begin());
}


void
Editor::_EndStep(int32_t newLast)
{
	MarkChanged();
	if (_InBulk() || fUndo.empty())
		return;
	UndoStep& top = fUndo.back();
	top.count += (newLast - fPendingFirst + 1) - fPendingCount;
	top.afterAnchor = fAnchor;
	top.afterCaret = fCaret;
	fCoalesce = top.kind == kUndoTyping;
}


void
Editor::_Swap(UndoStep& step)
{
	Story& story = *fStory;
	int32_t first = std::min<int32_t>(step.first, (int32_t)story.size());
	int32_t count = std::min<int32_t>(step.count,
		(int32_t)story.size() - first);
	std::vector<Paragraph> current(story.begin() + first,
		story.begin() + first + count);
	story.erase(story.begin() + first, story.begin() + first + count);
	story.insert(story.begin() + first, step.saved.begin(), step.saved.end());
	step.count = (int32_t)step.saved.size();
	step.saved.swap(current);
	if (story.empty())
		story.push_back(fDocument->NewParagraph(0));
	if (step.hasDocument) {
		std::swap(fDocument->settings, step.settings);
		std::swap(fDocument->styles, step.styles);
		std::swap(fDocument->info, step.info);
	}
}


std::string
Editor::UndoName() const
{
	return fUndo.empty() ? std::string() : fUndo.back().name;
}


std::string
Editor::RedoName() const
{
	return fRedo.empty() ? std::string() : fRedo.back().name;
}


void
Editor::Undo()
{
	if (fUndo.empty())
		return;
	UndoStep step = std::move(fUndo.back());
	fUndo.pop_back();
	_Swap(step);
	fAnchor = Clamp(step.beforeAnchor);
	fCaret = Clamp(step.beforeCaret);
	fRedo.push_back(std::move(step));
	fCoalesce = false;
	fTypingSet = false;
	MarkChanged();
}


void
Editor::Redo()
{
	if (fRedo.empty())
		return;
	UndoStep step = std::move(fRedo.back());
	fRedo.pop_back();
	_Swap(step);
	fAnchor = Clamp(step.afterAnchor);
	fCaret = Clamp(step.afterCaret);
	fUndo.push_back(std::move(step));
	fCoalesce = false;
	fTypingSet = false;
	MarkChanged();
}


void
Editor::ClearUndo()
{
	fUndo.clear();
	fRedo.clear();
	fCoalesce = false;
}


void
Editor::BeginBulk(const char* undoName, bool includeDocument)
{
	if (fBulkDepth++ > 0)
		return;
	fRedo.clear();
	UndoStep step;
	step.name = undoName;
	step.first = 0;
	step.count = (int32_t)fStory->size();
	step.saved = *fStory;
	step.beforeAnchor = fAnchor;
	step.beforeCaret = fCaret;
	step.hasDocument = includeDocument;
	if (includeDocument) {
		step.settings = fDocument->settings;
		step.styles = fDocument->styles;
		step.info = fDocument->info;
	}
	fUndo.push_back(std::move(step));
	if (fUndo.size() > kMaxUndo)
		fUndo.erase(fUndo.begin());
}


void
Editor::EndBulk()
{
	if (fBulkDepth == 0 || --fBulkDepth > 0)
		return;
	fAnchor = Clamp(fAnchor);
	fCaret = Clamp(fCaret);
	if (!fUndo.empty()) {
		UndoStep& top = fUndo.back();
		top.count = (int32_t)fStory->size();
		top.afterAnchor = fAnchor;
		top.afterCaret = fCaret;
	}
	fCoalesce = false;
	MarkChanged();
}


void
Editor::DocumentChanged(const char* undoName, const DocSettings& oldSettings)
{
	if (!_InBulk()) {
		fRedo.clear();
		UndoStep step;
		step.name = undoName;
		step.first = 0;
		step.count = 0;
		step.beforeAnchor = step.afterAnchor = fAnchor;
		step.beforeCaret = step.afterCaret = fCaret;
		step.hasDocument = true;
		step.settings = oldSettings;
		step.styles = fDocument->styles;
		step.info = fDocument->info;
		fUndo.push_back(std::move(step));
	}
	fCoalesce = false;
	MarkChanged();
}


// #pragma mark - replacing text


Story
Editor::_Sanitize(const Story& fragment, Pos at) const
{
	Story result = fragment;
	if (result.empty())
		result.push_back(Paragraph(InsertionFormat()));

	bool hasTable = false;
	for (const Paragraph& paragraph : result) {
		if (paragraph.IsTablePart() || paragraph.format.inTable)
			hasTable = true;
	}
	if (!hasTable)
		return result;

	// Whole rows may be inserted at the start of a normal paragraph.
	const Paragraph& target = (*fStory)[at.para];
	bool wholeRows = at.offset == 0 && !target.IsTablePart()
		&& result.size() >= 3 && result.front().kind == kParaCell
		&& result.back().kind == kParaNormal && result.back().text.empty()
		&& result[result.size() - 2].kind == kParaRowEnd;
	if (wholeRows)
		return result;

	Story flat;
	for (Paragraph& paragraph : result) {
		if (paragraph.kind == kParaRowEnd)
			continue;
		paragraph.kind = kParaNormal;
		paragraph.format.inTable = false;
		paragraph.Touch();
		flat.push_back(paragraph);
	}
	if (flat.empty())
		flat.push_back(Paragraph(InsertionFormat()));
	return flat;
}


Pos
Editor::_ReplaceInTable(Pos start, Pos end, const Story& fragment)
{
	Story& story = *fStory;
	for (int32_t p = start.para; p <= end.para; p++) {
		Paragraph& paragraph = story[p];
		int32_t from = p == start.para ? start.offset : 0;
		int32_t to = p == end.para ? end.offset : paragraph.Length();
		paragraph.Erase(from, to);
	}

	Paragraph joined = fragment[0];
	for (size_t i = 1; i < fragment.size(); i++) {
		joined.Insert(joined.Length(), Utf8(kLineBreakChar),
			joined.FormatAt(joined.Length()));
		joined.Append(fragment[i]);
	}
	Paragraph& target = story[start.para];
	if (target.kind == kParaRowEnd || joined.text.empty())
		return start;

	Paragraph merged = target.Slice(0, start.offset);
	merged.Append(joined);
	int32_t caret = merged.Length();
	merged.Append(target.Slice(start.offset, target.Length()));
	merged.format = target.format;
	merged.kind = target.kind;
	merged.row = target.row;
	merged.Touch();
	target = merged;
	return Pos(start.para, caret);
}


Pos
Editor::_ReplaceRaw(Pos start, Pos end, const Story& source)
{
	Story& story = *fStory;
	Story fragment = _Sanitize(source, start);

	bool table = false;
	for (int32_t p = start.para; p <= end.para; p++) {
		if (story[p].IsTablePart())
			table = true;
	}
	if (table && (start.para != end.para || fragment.size() > 1)) {
		fPendingNewLast = end.para;
		return _ReplaceInTable(start, end, fragment);
	}

	const Paragraph& first = story[start.para];
	const Paragraph& last = story[end.para];
	Paragraph head = first.Slice(0, start.offset);
	Paragraph tail = last.Slice(end.offset, last.Length());
	bool useFirst = start.para != end.para && tail.text.empty();
	const Paragraph& markOwner = useFirst ? first : last;

	std::vector<Paragraph> result;
	Pos caret;
	size_t count = fragment.size();
	if (count == 1) {
		Paragraph merged = head;
		merged.Append(fragment[0]);
		int32_t offset = merged.Length();
		merged.Append(tail);
		merged.format = markOwner.format;
		merged.kind = markOwner.kind;
		merged.row = markOwner.row;
		merged.Touch();
		result.push_back(merged);
		caret = Pos(start.para, offset);
	} else {
		Paragraph front = head;
		front.Append(fragment[0]);
		front.format = fragment[0].format;
		front.kind = fragment[0].kind;
		front.row = fragment[0].row;
		if (head.text.empty() && fragment[0].text.empty())
			front.SetMarkFormat(fragment[0].MarkFormat());
		front.Touch();
		result.push_back(front);
		for (size_t i = 1; i + 1 < count; i++)
			result.push_back(fragment[i]);
		Paragraph back = fragment[count - 1];
		int32_t offset = back.Length();
		back.Append(tail);
		back.format = last.format;
		back.kind = last.kind;
		back.row = last.row;
		back.Touch();
		result.push_back(back);
		caret = Pos(start.para + (int32_t)count - 1, offset);
	}

	story.erase(story.begin() + start.para, story.begin() + end.para + 1);
	story.insert(story.begin() + start.para, result.begin(), result.end());
	fPendingNewLast = start.para + (int32_t)count - 1;
	return caret;
}


Pos
Editor::ReplaceRange(Pos start, Pos end, const Story& fragment,
	const char* undoName, bool selectResult)
{
	start = Clamp(start);
	end = Clamp(end);
	if (end < start)
		std::swap(start, end);
	_BeginStep(undoName, kUndoOther, start.para, end.para);
	Pos result = _ReplaceRaw(start, end, fragment);
	fAnchor = selectResult ? start : result;
	fCaret = result;
	fTypingSet = false;
	_EndStep(fPendingNewLast);
	fCoalesce = false;
	return result;
}


void
Editor::ReplaceSelection(const Story& fragment, const char* undoName,
	bool selectResult)
{
	ReplaceRange(SelStart(), SelEnd(), fragment, undoName, selectResult);
}


Story
Editor::CopyRange(Pos start, Pos end) const
{
	start = Clamp(start);
	end = Clamp(end);
	if (end < start)
		std::swap(start, end);
	Story fragment;
	const Story& story = *fStory;
	for (int32_t p = start.para; p <= end.para; p++) {
		const Paragraph& paragraph = story[p];
		int32_t from = p == start.para ? start.offset : 0;
		int32_t to = p == end.para ? end.offset : paragraph.Length();
		fragment.push_back(paragraph.Slice(from, to));
	}
	return fragment;
}


Story
Editor::CopySelection() const
{
	return CopyRange(SelStart(), SelEnd());
}


std::string
Editor::SelectedText() const
{
	return Document::PlainText(*fStory, SelStart(), SelEnd());
}


// #pragma mark - typing


void
Editor::InsertText(const std::string& text)
{
	if (text.empty())
		return;
	if (text.find_first_of("\r\n") != std::string::npos) {
		CharFormat format = InsertionFormat();
		Story fragment;
		size_t begin = 0;
		while (true) {
			size_t stop = text.find_first_of("\r\n", begin);
			Paragraph paragraph(format);
			paragraph.format = (*fStory)[SelStart().para].format;
			paragraph.kind = kParaNormal;
			paragraph.format.inTable = false;
			std::string line = text.substr(begin,
				stop == std::string::npos ? std::string::npos : stop - begin);
			paragraph.Insert(0, line, format);
			fragment.push_back(paragraph);
			if (stop == std::string::npos)
				break;
			begin = stop + 1;
			if (text[stop] == '\r' && begin < text.size() && text[begin] == '\n')
				begin++;
		}
		ReplaceSelection(fragment, "Typing");
		fLastTyped = text;
		return;
	}

	Story& story = *fStory;
	CharFormat format = InsertionFormat();
	if (!HasSelection() && story[fCaret.para].kind == kParaRowEnd
		&& fCaret.para > 0) {
		fCaret = fAnchor = Pos(fCaret.para - 1,
			story[fCaret.para - 1].Length());
	}
	if (HasSelection()) {
		Pos start = SelStart();
		Pos end = SelEnd();
		_BeginStep("Typing", kUndoTyping, start.para, end.para);
		Paragraph inserted(format);
		inserted.Insert(0, text, format);
		Story fragment;
		fragment.push_back(inserted);
		Pos result = _ReplaceRaw(start, end, fragment);
		fAnchor = fCaret = result;
		fTypingSet = false;
		_EndStep(fPendingNewLast);
	} else {
		_BeginStep("Typing", kUndoTyping, fCaret.para, fCaret.para);
		story[fCaret.para].Insert(fCaret.offset, text, format);
		fCaret.offset += (int32_t)text.size();
		fAnchor = fCaret;
		fTypingSet = false;
		_EndStep(fCaret.para);
	}
	fLastTyped += text;
}


void
Editor::InsertSpecial(uint32_t c, const CharFormat* format,
	const char* undoName)
{
	Story& story = *fStory;
	CharFormat insertFormat = format ? *format : InsertionFormat();
	Pos start = SelStart();
	Pos end = SelEnd();
	int kind = strcmp(undoName, "Typing") == 0 ? kUndoTyping : kUndoOther;
	_BeginStep(undoName, kind, start.para, end.para);
	Pos at = start;
	if (HasSelection()) {
		Story empty;
		empty.push_back(Paragraph(insertFormat));
		at = _ReplaceRaw(start, end, empty);
	} else
		fPendingNewLast = start.para;
	std::string text = Utf8(c);
	story[at.para].Insert(at.offset, text, insertFormat);
	fCaret = fAnchor = Pos(at.para, at.offset + (int32_t)text.size());
	fTypingSet = false;
	_EndStep(fPendingNewLast);
	if (kind != kUndoTyping)
		fCoalesce = false;
}


void
Editor::InsertParagraphBreak(bool keepStyle)
{
	Story& story = *fStory;
	if (story[SelStart().para].IsTablePart()) {
		InsertSpecial(kLineBreakChar);
		return;
	}
	CharFormat format = InsertionFormat();
	Pos start = SelStart();
	Pos end = SelEnd();
	_BeginStep("Typing", kUndoTyping, start.para, end.para);
	if (HasSelection()) {
		Story empty;
		empty.push_back(Paragraph(format));
		start = _ReplaceRaw(start, end, empty);
	}
	Paragraph& paragraph = story[start.para];
	bool atEnd = start.offset >= paragraph.Length();
	Paragraph tail = paragraph.SplitAt(start.offset);
	int32_t style = paragraph.format.style;
	int32_t next = fDocument->styles.NextStyle(style);
	if (atEnd && !keepStyle && next != style) {
		tail.format = fDocument->StyleParaFormat(next);
		tail.SetMarkFormat(fDocument->StyleCharFormat(next));
	} else if (atEnd)
		tail.SetMarkFormat(format);
	tail.format.pageBreakBefore = false;
	tail.Touch();
	story.insert(story.begin() + start.para + 1, tail);
	fCaret = fAnchor = Pos(start.para + 1, 0);
	fTypingSet = false;
	_EndStep(start.para + 1);
	fLastTyped += "\n";
}


void
Editor::Backspace(bool word)
{
	Story& story = *fStory;
	if (HasSelection()) {
		Pos start = SelStart();
		Pos end = SelEnd();
		_BeginStep("Typing", kUndoTyping, start.para, end.para);
		Story empty;
		empty.push_back(Paragraph(InsertionFormat()));
		Pos result = _ReplaceRaw(start, end, empty);
		fAnchor = fCaret = result;
		_EndStep(fPendingNewLast);
		return;
	}
	Pos caret = fCaret;
	if (caret.para == 0 && caret.offset == 0)
		return;
	if (caret.offset == 0 && (story[caret.para].IsTablePart()
			|| story[caret.para - 1].IsTablePart()))
		return;
	Pos from = word ? PrevWord(caret) : PrevChar(caret);
	_BeginStep("Typing", kUndoTyping, from.para, caret.para);
	Story empty;
	empty.push_back(Paragraph(InsertionFormat()));
	Pos result = _ReplaceRaw(from, caret, empty);
	fAnchor = fCaret = result;
	_EndStep(fPendingNewLast);
	if (!fLastTyped.empty())
		fLastTyped.erase(PrevOffset(fLastTyped, (int32_t)fLastTyped.size()));
}


void
Editor::DeleteForward(bool word)
{
	Story& story = *fStory;
	if (HasSelection()) {
		Backspace(false);
		return;
	}
	Pos caret = fCaret;
	if (caret == End())
		return;
	const Paragraph& paragraph = story[caret.para];
	if (caret.offset >= paragraph.Length() && (paragraph.IsTablePart()
			|| story[caret.para + 1].IsTablePart()))
		return;
	Pos to = word ? NextWord(caret) : NextChar(caret);
	_BeginStep("Typing", kUndoTyping, caret.para, to.para);
	Story empty;
	empty.push_back(Paragraph(InsertionFormat()));
	Pos result = _ReplaceRaw(caret, to, empty);
	fAnchor = fCaret = result;
	_EndStep(fPendingNewLast);
}


void
Editor::DeleteSelection(const char* undoName)
{
	if (!HasSelection())
		return;
	Story empty;
	empty.push_back(Paragraph(InsertionFormat()));
	ReplaceRange(SelStart(), SelEnd(), empty, undoName);
}


// #pragma mark - formatting


void
Editor::ApplyCharFormat(const std::function<void(CharFormat&)>& modify,
	const char* undoName)
{
	Story& story = *fStory;
	if (!HasSelection()) {
		CharFormat format = InsertionFormat();
		modify(format);
		Paragraph& paragraph = story[fCaret.para];
		if (paragraph.Length() == 0) {
			_BeginStep(undoName, kUndoOther, fCaret.para, fCaret.para);
			paragraph.ApplyFormat(0, 0, modify);
			_EndStep(fCaret.para);
			fCoalesce = false;
		} else
			MarkChanged();
		SetTypingFormat(format);
		return;
	}
	Pos start = SelStart();
	Pos end = SelEnd();
	_BeginStep(undoName, kUndoOther, start.para, end.para);
	for (int32_t p = start.para; p <= end.para; p++) {
		Paragraph& paragraph = story[p];
		int32_t from = p == start.para ? start.offset : 0;
		int32_t to = p == end.para ? end.offset : paragraph.Length();
		if (paragraph.Length() == 0) {
			if (p < end.para || start.para == end.para)
				paragraph.ApplyFormat(0, 0, modify);
		} else if (from < to)
			paragraph.ApplyFormat(from, to, modify);
	}
	_EndStep(end.para);
	fCoalesce = false;
}


void
Editor::ApplyCharFields(const CharFormat& values, uint32_t mask,
	const char* undoName)
{
	if (mask == 0)
		return;
	ApplyCharFormat([&values, mask](CharFormat& format) {
		CopyCharFields(format, values, mask);
	}, undoName);
}


void
Editor::ToggleCharField(uint32_t field)
{
	CharFormat current = SelectionCharFormat(nullptr);
	CharFormat values = current;
	switch (field) {
		case kCFBold: values.bold = !current.bold; break;
		case kCFItalic: values.italic = !current.italic; break;
		case kCFStrike: values.strike = !current.strike; break;
		case kCFOutline: values.outline = !current.outline; break;
		case kCFShadow: values.shadow = !current.shadow; break;
		case kCFSmallCaps: values.smallCaps = !current.smallCaps; break;
		case kCFAllCaps: values.allCaps = !current.allCaps; break;
		case kCFHidden: values.hidden = !current.hidden; break;
		case kCFUnderline:
			values.underline = current.underline == kULSingle
				? kULNone : kULSingle;
			break;
		default:
			return;
	}
	ApplyCharFields(values, field);
}


void
Editor::ApplyParaFormat(const std::function<void(ParaFormat&)>& modify,
	const char* undoName)
{
	Story& story = *fStory;
	int32_t first = FirstSelectedPara();
	int32_t last = LastSelectedPara();
	_BeginStep(undoName, kUndoOther, first, SelEnd().para);
	for (int32_t p = first; p <= last; p++) {
		modify(story[p].format);
		story[p].Touch();
	}
	_EndStep(SelEnd().para);
	fCoalesce = false;
}


void
Editor::_ApplyStyleToParagraph(Paragraph& paragraph, int32_t style)
{
	int32_t old = paragraph.format.style;
	CharFormat oldFormat = fDocument->StyleCharFormat(old);
	CharFormat newFormat = fDocument->StyleCharFormat(style);
	bool inTable = paragraph.format.inTable;
	paragraph.format = fDocument->StyleParaFormat(style);
	paragraph.format.inTable = inTable;
	paragraph.ApplyFormat(0, std::max<int32_t>(paragraph.Length(), 1),
		[&](CharFormat& format) {
			uint32_t same = SameCharFields(format, oldFormat);
			CopyCharFields(format, newFormat, same);
		});
	paragraph.Touch();
}


void
Editor::ApplyStyle(int32_t style)
{
	if (style < 0 || style >= fDocument->styles.Count())
		return;
	Story& story = *fStory;
	int32_t first = FirstSelectedPara();
	int32_t last = LastSelectedPara();
	_BeginStep("Style", kUndoOther, first, SelEnd().para);
	for (int32_t p = first; p <= last; p++)
		_ApplyStyleToParagraph(story[p], style);
	_EndStep(SelEnd().para);
	fTypingSet = false;
	fCoalesce = false;
}


void
Editor::RevertToStyle()
{
	Story& story = *fStory;
	int32_t first = FirstSelectedPara();
	int32_t last = LastSelectedPara();
	Pos start = SelStart();
	Pos end = SelEnd();
	bool whole = !HasSelection();
	_BeginStep("Formatting", kUndoOther, first, end.para);
	for (int32_t p = first; p <= last; p++) {
		Paragraph& paragraph = story[p];
		int32_t style = paragraph.format.style;
		CharFormat format = fDocument->StyleCharFormat(style);
		int32_t from = whole || p != start.para ? 0 : start.offset;
		int32_t to = whole || p != end.para ? paragraph.Length() : end.offset;
		paragraph.ApplyFormat(from, std::max(to, from + 1),
			[&format](CharFormat& f) { CopyCharFields(f, format, kCFAll); });
		bool inTable = paragraph.format.inTable;
		paragraph.format = fDocument->StyleParaFormat(style);
		paragraph.format.inTable = inTable;
		paragraph.Touch();
	}
	_EndStep(end.para);
	fTypingSet = false;
	fCoalesce = false;
}


void
Editor::PlainText()
{
	const uint32_t mask = kCFBold | kCFItalic | kCFUnderline | kCFStrike
		| kCFOutline | kCFShadow | kCFSmallCaps | kCFAllCaps | kCFHidden
		| kCFPosition | kCFSpacing;
	Story& story = *fStory;
	if (!HasSelection()) {
		CharFormat format = InsertionFormat();
		CopyCharFields(format, fDocument->StyleCharFormat(
			story[fCaret.para].format.style), mask);
		SetTypingFormat(format);
		MarkChanged();
		return;
	}
	Pos start = SelStart();
	Pos end = SelEnd();
	_BeginStep("Formatting", kUndoOther, start.para, end.para);
	for (int32_t p = start.para; p <= end.para; p++) {
		Paragraph& paragraph = story[p];
		CharFormat style = fDocument->StyleCharFormat(paragraph.format.style);
		int32_t from = p == start.para ? start.offset : 0;
		int32_t to = p == end.para ? end.offset : paragraph.Length();
		if (paragraph.Length() == 0 || from < to) {
			paragraph.ApplyFormat(from, to, [&style, mask](CharFormat& f) {
				CopyCharFields(f, style, mask);
			});
		}
	}
	_EndStep(end.para);
	fCoalesce = false;
}


void
Editor::ChangeCase(int mode)
{
	if (!HasSelection()) {
		Pos start, end;
		WordAt(fCaret, &start, &end);
		Select(start, end);
		if (!HasSelection())
			return;
	}
	Story& story = *fStory;
	Pos start = SelStart();
	Pos end = SelEnd();
	_BeginStep("Change Case", kUndoOther, start.para, end.para);

	// Context: is the character before the selection the end of a word
	// or sentence?
	bool wordStart = true;
	bool sentenceStart = true;
	{
		const std::string& text = story[start.para].text;
		int32_t offset = start.offset;
		if (offset > 0) {
			uint32_t previous = CharAt(text, PrevOffset(text, offset));
			wordStart = !IsWordChar(previous);
			int32_t scan = offset;
			sentenceStart = true;
			while (scan > 0) {
				scan = PrevOffset(text, scan);
				uint32_t c = CharAt(text, scan);
				if (IsSpace(c) || c == '\t' || c == '"' || c == 0x201C)
					continue;
				sentenceStart = IsSentenceEnd(c);
				break;
			}
		}
	}

	for (int32_t p = start.para; p <= end.para; p++) {
		Paragraph& paragraph = story[p];
		int32_t from = p == start.para ? start.offset : 0;
		int32_t to = p == end.para ? end.offset : paragraph.Length();
		if (p != start.para) {
			wordStart = true;
			sentenceStart = true;
		}
		if (from >= to)
			continue;
		Paragraph rebuilt = paragraph.Slice(0, from);
		int32_t offset = from;
		while (offset < to) {
			int32_t runIndex = paragraph.RunIndexAt(offset);
			int32_t runEnd = std::min(paragraph.RunEnd(runIndex), to);
			const CharFormat& format = paragraph.runs[runIndex].format;
			std::string piece;
			while (offset < runEnd) {
				uint32_t c = CharAt(paragraph.text, offset);
				uint32_t mapped = c;
				switch (mode) {
					case kCaseUpper: mapped = ToUpper(c); break;
					case kCaseLower: mapped = ToLower(c); break;
					case kCaseToggle:
						mapped = IsUpper(c) ? ToLower(c) : ToUpper(c);
						break;
					case kCaseTitle:
						mapped = wordStart ? ToUpper(c) : ToLower(c);
						break;
					case kCaseSentence:
						mapped = sentenceStart && IsLetter(c)
							? ToUpper(c) : ToLower(c);
						break;
				}
				if (IsLetter(c) || IsDigit(c))
					sentenceStart = false;
				else if (IsSentenceEnd(c))
					sentenceStart = true;
				wordStart = !(IsWordChar(c) || c == '\'' || c == 0x2019);
				AppendUtf8(piece, mapped);
				offset = NextOffset(paragraph.text, offset);
			}
			rebuilt.Insert(rebuilt.Length(), piece, format);
		}
		if (p == end.para)
			fCaret = fAnchor = Pos(p, rebuilt.Length());
		rebuilt.Append(paragraph.Slice(to, paragraph.Length()));
		rebuilt.format = paragraph.format;
		rebuilt.kind = paragraph.kind;
		rebuilt.row = paragraph.row;
		rebuilt.Touch();
		paragraph = rebuilt;
	}
	fAnchor = start;
	fCaret = Clamp(fCaret);
	_EndStep(end.para);
	fCoalesce = false;
}


// #pragma mark - styles


void
Editor::_UpdateStyleUsers(Story& story, int32_t index, const Style& oldStyle,
	const Style& newStyle)
{
	CharFormat oldCF = oldStyle.cf;
	CharFormat newCF = newStyle.cf;
	for (Paragraph& paragraph : story) {
		if (paragraph.format.style != index)
			continue;
		uint32_t same = SameParaFields(paragraph.format, oldStyle.pf)
			& ~kPFStyle;
		CopyParaFields(paragraph.format, newStyle.pf, same);
		paragraph.ApplyFormat(0, std::max<int32_t>(paragraph.Length(), 1),
			[&](CharFormat& format) {
				CopyCharFields(format, newCF, SameCharFields(format, oldCF));
			});
		paragraph.Touch();
	}
}


void
Editor::RedefineStyle(int32_t index, const Style& style, const char* undoName)
{
	StyleSheet& sheet = fDocument->styles;
	if (index < 0 || index >= sheet.Count())
		return;
	BeginBulk(undoName, true);
	std::vector<Story*> stories = { &fDocument->body, &fDocument->header,
		&fDocument->footer, &fDocument->firstHeader, &fDocument->firstFooter };
	for (auto& entry : fDocument->footnotes)
		stories.push_back(&entry.second);

	std::function<void(int32_t, const Style&, const Style&, int)> update;
	update = [&](int32_t changed, const Style& before, const Style& after,
			int depth) {
		for (Story* story : stories)
			_UpdateStyleUsers(*story, changed, before, after);
		if (depth > 8)
			return;
		for (int32_t i = 0; i < sheet.Count(); i++) {
			if (sheet.styles[i].basedOn != changed || i == changed)
				continue;
			Style derivedBefore = sheet.styles[i];
			Style& derived = sheet.styles[i];
			CopyCharFields(derived.cf, after.cf,
				SameCharFields(derived.cf, before.cf));
			CopyParaFields(derived.pf, after.pf,
				SameParaFields(derived.pf, before.pf) & ~kPFStyle);
			update(i, derivedBefore, derived, depth + 1);
		}
	};

	Style before = sheet.styles[index];
	sheet.styles[index] = style;
	update(index, before, style, 0);
	EndBulk();
}


int32_t
Editor::AddStyle(const Style& style)
{
	fDocument->styles.styles.push_back(style);
	MarkChanged();
	return fDocument->styles.Count() - 1;
}


void
Editor::DeleteStyle(int32_t index)
{
	StyleSheet& sheet = fDocument->styles;
	if (index <= 0 || index >= sheet.Count() || sheet.styles[index].automatic)
		return;
	BeginBulk("Delete Style", true);
	std::vector<Story*> stories = { &fDocument->body, &fDocument->header,
		&fDocument->footer, &fDocument->firstHeader, &fDocument->firstFooter };
	for (auto& entry : fDocument->footnotes)
		stories.push_back(&entry.second);
	for (Story* story : stories) {
		for (Paragraph& paragraph : *story) {
			int32_t& style = paragraph.format.style;
			if (style == index)
				style = 0;
			else if (style > index)
				style--;
			paragraph.Touch();
		}
	}
	sheet.styles.erase(sheet.styles.begin() + index);
	for (Style& style : sheet.styles) {
		if (style.basedOn == index)
			style.basedOn = 0;
		else if (style.basedOn > index)
			style.basedOn--;
		if (style.next == index)
			style.next = -1;
		else if (style.next > index)
			style.next--;
		if (style.pf.style == index)
			style.pf.style = 0;
		else if (style.pf.style > index)
			style.pf.style--;
	}
	EndBulk();
}


// #pragma mark - navigation


static bool
IsWordCharAt(const std::string& text, int32_t offset)
{
	if (offset < 0 || offset >= (int32_t)text.size())
		return false;
	uint32_t c = CharAt(text, offset);
	if (IsWordChar(c))
		return true;
	if (c == '\'' || c == 0x2019) {
		if (offset == 0)
			return false;
		int32_t next = NextOffset(text, offset);
		return IsWordChar(CharAt(text, PrevOffset(text, offset)))
			&& next < (int32_t)text.size() && IsWordChar(CharAt(text, next));
	}
	return false;
}


static bool
IsBlank(uint32_t c)
{
	return IsSpace(c) || c == kNbspChar;
}


Pos
Editor::NextChar(Pos pos) const
{
	const Story& story = *fStory;
	if (pos.offset < story[pos.para].Length())
		return Pos(pos.para, NextOffset(story[pos.para].text, pos.offset));
	if (pos.para + 1 < (int32_t)story.size())
		return Pos(pos.para + 1, 0);
	return pos;
}


Pos
Editor::PrevChar(Pos pos) const
{
	const Story& story = *fStory;
	if (pos.offset > 0)
		return Pos(pos.para, PrevOffset(story[pos.para].text, pos.offset));
	if (pos.para > 0)
		return Pos(pos.para - 1, story[pos.para - 1].Length());
	return pos;
}


Pos
Editor::NextWord(Pos pos) const
{
	const Story& story = *fStory;
	const std::string& text = story[pos.para].text;
	int32_t length = (int32_t)text.size();
	if (pos.offset >= length) {
		if (pos.para + 1 < (int32_t)story.size())
			return Pos(pos.para + 1, 0);
		return pos;
	}
	int32_t offset = pos.offset;
	if (IsWordCharAt(text, offset)) {
		while (offset < length && IsWordCharAt(text, offset))
			offset = NextOffset(text, offset);
	} else if (!IsBlank(CharAt(text, offset)))
		offset = NextOffset(text, offset);
	while (offset < length && IsBlank(CharAt(text, offset)))
		offset = NextOffset(text, offset);
	return Pos(pos.para, offset);
}


Pos
Editor::PrevWord(Pos pos) const
{
	const Story& story = *fStory;
	if (pos.offset == 0) {
		if (pos.para > 0)
			return Pos(pos.para - 1, story[pos.para - 1].Length());
		return pos;
	}
	const std::string& text = story[pos.para].text;
	int32_t offset = pos.offset;
	while (offset > 0 && IsBlank(CharAt(text, PrevOffset(text, offset))))
		offset = PrevOffset(text, offset);
	if (offset == 0)
		return Pos(pos.para, 0);
	if (IsWordCharAt(text, PrevOffset(text, offset))) {
		while (offset > 0 && IsWordCharAt(text, PrevOffset(text, offset)))
			offset = PrevOffset(text, offset);
	} else
		offset = PrevOffset(text, offset);
	return Pos(pos.para, offset);
}


Pos
Editor::NextParagraph(Pos pos) const
{
	if (pos.para + 1 < (int32_t)fStory->size())
		return Pos(pos.para + 1, 0);
	return End();
}


Pos
Editor::PrevParagraph(Pos pos) const
{
	if (pos.offset > 0)
		return Pos(pos.para, 0);
	if (pos.para > 0)
		return Pos(pos.para - 1, 0);
	return pos;
}


void
Editor::WordAt(Pos pos, Pos* start, Pos* end) const
{
	pos = Clamp(pos);
	const std::string& text = (*fStory)[pos.para].text;
	int32_t length = (int32_t)text.size();
	*start = *end = pos;
	if (length == 0)
		return;
	int32_t offset = pos.offset;
	if (offset >= length)
		offset = PrevOffset(text, length);
	int32_t from = offset;
	int32_t to = offset;
	if (IsWordCharAt(text, offset)) {
		while (from > 0 && IsWordCharAt(text, PrevOffset(text, from)))
			from = PrevOffset(text, from);
		while (to < length && IsWordCharAt(text, to))
			to = NextOffset(text, to);
	} else if (IsBlank(CharAt(text, offset))) {
		while (from > 0 && IsBlank(CharAt(text, PrevOffset(text, from))))
			from = PrevOffset(text, from);
		while (to < length && IsBlank(CharAt(text, to)))
			to = NextOffset(text, to);
	} else
		to = NextOffset(text, to);
	while (to < length && CharAt(text, to) == ' ')
		to = NextOffset(text, to);
	*start = Pos(pos.para, from);
	*end = Pos(pos.para, to);
}


void
Editor::SentenceAt(Pos pos, Pos* start, Pos* end) const
{
	pos = Clamp(pos);
	const std::string& text = (*fStory)[pos.para].text;
	int32_t length = (int32_t)text.size();
	// start: after the previous sentence end followed by white space
	int32_t from = pos.offset;
	while (from > 0) {
		int32_t previous = PrevOffset(text, from);
		uint32_t c = CharAt(text, previous);
		if (IsBlank(c) || c == '\t') {
			int32_t scan = previous;
			while (scan > 0 && IsBlank(CharAt(text, PrevOffset(text, scan))))
				scan = PrevOffset(text, scan);
			if (scan > 0) {
				uint32_t before = CharAt(text, PrevOffset(text, scan));
				if (IsSentenceEnd(before) || before == '"' || before == 0x201D
					|| before == ')') {
					break;
				}
			}
		}
		from = previous;
	}
	int32_t to = pos.offset;
	while (to < length) {
		uint32_t c = CharAt(text, to);
		to = NextOffset(text, to);
		if (IsSentenceEnd(c)) {
			while (to < length && !IsBlank(CharAt(text, to)))
				to = NextOffset(text, to);
			while (to < length && IsBlank(CharAt(text, to)))
				to = NextOffset(text, to);
			break;
		}
	}
	*start = Pos(pos.para, from);
	*end = Pos(pos.para, to);
}


Pos
Editor::NextSentence(Pos pos) const
{
	Pos start, end;
	SentenceAt(pos, &start, &end);
	if (end == pos)
		return NextChar(pos);
	return end;
}


Pos
Editor::PrevSentence(Pos pos) const
{
	Pos start, end;
	SentenceAt(pos, &start, &end);
	if (start == pos && pos != Pos(0, 0)) {
		Pos previous = PrevChar(pos);
		SentenceAt(previous, &start, &end);
	}
	return start;
}


void
Editor::ParagraphAt(Pos pos, Pos* start, Pos* end) const
{
	pos = Clamp(pos);
	*start = Pos(pos.para, 0);
	if (pos.para + 1 < (int32_t)fStory->size())
		*end = Pos(pos.para + 1, 0);
	else
		*end = Pos(pos.para, (*fStory)[pos.para].Length());
}

}	// namespace quill
