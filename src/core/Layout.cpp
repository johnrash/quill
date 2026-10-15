/*
 * Quill - line layout and pagination.
 */
#include "Layout.h"
#include "Text.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace quill {

static const float kFootnoteSeparator = 12;


float
BorderLineWidth(uint8_t line)
{
	switch (line) {
		case kBorderSingle: return 1;
		case kBorderThick: return 2;
		case kBorderDouble: return 3;
		case kBorderDotted: return 1;
		case kBorderHairline: return 0.5f;
		default: return 0;
	}
}


int32_t
ParaLayout::CpForByte(int32_t byte) const
{
	auto it = std::lower_bound(cpByte.begin(), cpByte.end(), byte);
	if (it == cpByte.end())
		return Count();
	return (int32_t)(it - cpByte.begin());
}


int32_t
ParaLayout::LineForCp(int32_t cp, bool lineEnd) const
{
	int32_t low = 0;
	int32_t high = (int32_t)lines.size() - 1;
	while (low < high) {
		int32_t middle = (low + high + 1) / 2;
		if (lines[middle].startCp <= cp)
			low = middle;
		else
			high = middle - 1;
	}
	if (lineEnd && low > 0 && cp == lines[low].startCp
		&& !lines[low - 1].forced)
		low--;
	return low;
}


float
ParaLayout::CaretX(int32_t cp, int32_t line) const
{
	const LineBox& box = lines[line];
	if (cp < box.endCp && cp < Count())
		return x[cp];
	return box.endX;
}


bool
ParaLayout::LineHasFootnotes(int32_t line) const
{
	const LineBox& box = lines[line];
	for (int32_t i = box.startCp; i < box.endCp; i++) {
		if (cps[i] == kFootnoteChar)
			return true;
	}
	return false;
}


// #pragma mark - paragraph layout


namespace {

struct RunMetrics {
	float	ascent = 0;
	float	descent = 0;
	float	leading = 0;
	float	hyphen = -1;
};


bool
IsPlain(uint32_t c)
{
	return c != kTabChar && !IsSpecialChar(c) && c != kSoftHyphenChar;
}


bool
BreaksAfter(uint32_t c)
{
	return c == '-' || c == 0x2013 || c == 0x2014 || c == kSoftHyphenChar
		|| (c >= 0x2E80 && c < 0xA000) || (c >= 0xAC00 && c < 0xD7B0);
}


std::string
FieldText(uint32_t c, const CharFormat& format, const Document& document,
	const LayoutOptions& options)
{
	switch (c) {
		case kFieldPageChar:
			return FormatPageNumber(options.pageNumber,
				document.settings.pageNumberFormat);
		case kFieldPagesChar:
			return std::to_string(options.pageCount);
		case kFieldDateChar:
			return options.date.empty() ? std::string("1/1/92") : options.date;
		case kFieldTimeChar:
			return options.time.empty() ? std::string("12:00 PM")
				: options.time;
		case kFootnoteChar: {
			if (options.footnoteNumbers != nullptr) {
				auto it = options.footnoteNumbers->find(format.footnote);
				if (it != options.footnoteNumbers->end())
					return std::to_string(it->second);
			}
			return "*";
		}
	}
	return std::string();
}

}	// namespace


void
LayoutParagraph(const Paragraph& paragraph, const Document& document,
	FontMetrics& metrics, const LayoutOptions& options, float width,
	bool borderTop, bool borderBottom, bool borderBetween, ParaLayout& out)
{
	out = ParaLayout();
	out.revision = paragraph.revision;
	out.width = width;
	const ParaFormat& pf = paragraph.format;
	const std::string& text = paragraph.text;
	const std::vector<Run>& runs = paragraph.runs;

	int32_t length = (int32_t)text.size();
	for (int32_t offset = 0; offset < length;) {
		int32_t consumed;
		uint32_t c = DecodeUtf8(text.c_str() + offset, length - offset,
			&consumed);
		out.cpByte.push_back(offset);
		out.cps.push_back(c);
		offset += std::max<int32_t>(1, consumed);
	}
	out.cpByte.push_back(length);
	int32_t n = (int32_t)out.cps.size();
	out.x.assign(n, 0);
	out.w.assign(n, 0);
	out.runOf.assign(n, 0);
	out.flags.assign(n, 0);

	int32_t run = 0;
	for (int32_t i = 0; i < n; i++) {
		while (run + 1 < (int32_t)runs.size()
			&& runs[run + 1].start <= out.cpByte[i])
			run++;
		out.runOf[i] = run;
	}

	std::vector<RunMetrics> runMetrics(runs.size());
	for (size_t r = 0; r < runs.size(); r++) {
		const CharFormat& format = runs[r].format;
		RunMetrics& m = runMetrics[r];
		metrics.Heights(format, format.size, &m.ascent, &m.descent,
			&m.leading);
		if (format.position > 0)
			m.ascent += format.position;
		else if (format.position < 0)
			m.descent -= format.position;
	}

	// Character widths
	std::string buffer;
	for (int32_t i = 0; i < n;) {
		int32_t r = out.runOf[i];
		const CharFormat& format = runs[r].format;
		uint32_t c = out.cps[i];
		if (format.hidden && !options.showHidden) {
			out.flags[i] |= kGlyphHidden;
			i++;
			continue;
		}
		if (c == kTabChar) {
			out.flags[i] |= kGlyphTab;
			i++;
			continue;
		}
		if (c == kLineBreakChar || c == kPageBreakChar
			|| c == kSectionBreakChar) {
			out.flags[i] |= kGlyphBreak;
			i++;
			continue;
		}
		if (c == kSoftHyphenChar) {
			i++;
			continue;
		}
		if (c == kPictureChar) {
			const Picture* picture = document.PictureFor(format.picture);
			out.flags[i] |= kGlyphPicture;
			out.w[i] = picture != nullptr ? picture->width : 36;
			i++;
			continue;
		}
		if ((c >= kFieldPageChar && c <= kFootnoteChar)) {
			std::string field = FieldText(c, format, document, options);
			out.fieldText[i] = field;
			out.flags[i] |= kGlyphField;
			int32_t count = CountChars(field);
			if (count > 0) {
				std::vector<float> widths(count);
				metrics.Widths(format, format.size, field.c_str(),
					(int32_t)field.size(), count, widths.data());
				float sum = 0;
				for (float value : widths)
					sum += value + format.spacing;
				out.w[i] = std::max(0.0f, sum);
			}
			i++;
			continue;
		}

		// A batch of plain characters of the same run and case class.
		bool small = format.smallCaps && IsLower(c);
		int32_t j = i;
		buffer.clear();
		while (j < n && out.runOf[j] == r && IsPlain(out.cps[j])) {
			uint32_t cj = out.cps[j];
			bool jSmall = format.smallCaps && IsLower(cj);
			if (jSmall != small)
				break;
			uint32_t shown = cj;
			if (format.allCaps || jSmall)
				shown = ToUpper(cj);
			if (shown == kNbspChar)
				shown = ' ';
			else if (shown == kNbHyphenChar)
				shown = '-';
			AppendUtf8(buffer, shown);
			j++;
		}
		float size = small ? format.size * 0.8f : format.size;
		metrics.Widths(format, size, buffer.c_str(), (int32_t)buffer.size(),
			j - i, out.w.data() + i);
		for (int32_t k = i; k < j; k++) {
			if (small)
				out.flags[k] |= kGlyphSmall;
			if (out.cps[k] == ' ' || out.cps[k] == kNbspChar)
				out.flags[k] |= kGlyphSpace;
			if (out.w[k] > 0)
				out.w[k] = std::max(0.0f, out.w[k] + format.spacing);
		}
		i = j;
	}

	for (const TabStop& tab : pf.tabs) {
		if (tab.kind == kTabBar)
			out.barTabs.push_back(tab.pos);
	}

	float defaultTab = std::max(1.0f, document.settings.defaultTab);
	float right = width - pf.rightIndent;

	auto tabAdvance = [&](int32_t index, float x, bool firstLine) -> float {
		TabStop implicit;
		const TabStop* stop = nullptr;
		for (const TabStop& tab : pf.tabs) {
			if (tab.kind == kTabBar)
				continue;
			if (tab.pos > x + 0.01f) {
				stop = &tab;
				break;
			}
		}
		if (firstLine && pf.firstIndent < 0 && pf.leftIndent > x + 0.01f
			&& (stop == nullptr || stop->pos > pf.leftIndent)) {
			implicit.pos = pf.leftIndent;
			stop = &implicit;
		}
		if (stop == nullptr) {
			implicit.pos = (std::floor(x / defaultTab + 0.001f) + 1)
				* defaultTab;
			stop = &implicit;
		}
		if (stop->leader != kLeaderNone)
			out.tabLeader[index] = stop->leader;
		float pos = stop->pos;
		if (stop->kind == kTabLeft)
			return std::max(0.0f, pos - x);

		float segment = 0;
		float toDot = -1;
		for (int32_t k = index + 1; k < n; k++) {
			if (out.flags[k] & (kGlyphTab | kGlyphBreak))
				break;
			if (out.flags[k] & kGlyphHidden)
				continue;
			if (toDot < 0 && out.cps[k] == '.')
				toDot = segment;
			segment += out.w[k];
		}
		switch (stop->kind) {
			case kTabRight:
				return std::max(0.0f, pos - x - segment);
			case kTabCenter:
				return std::max(0.0f, pos - x - segment / 2);
			case kTabDecimal:
				return std::max(0.0f, pos - x - (toDot >= 0 ? toDot : segment));
		}
		return std::max(0.0f, pos - x);
	};

	auto hyphenWidth = [&](int32_t index) -> float {
		int32_t r = out.runOf[index];
		RunMetrics& m = runMetrics[r];
		if (m.hyphen < 0) {
			float value = 0;
			metrics.Widths(runs[r].format, runs[r].format.size, "-", 1, 1,
				&value);
			m.hyphen = value;
		}
		return m.hyphen;
	};

	// Lines
	int32_t cp = 0;
	bool firstLine = true;
	while (true) {
		LineBox line;
		line.startCp = cp;
		float left = pf.leftIndent + (firstLine ? pf.firstIndent : 0);
		line.left = left;
		float x = left;
		int32_t breakAfter = -1;
		int32_t end = -1;
		bool forced = false;
		int32_t i = cp;
		while (i < n) {
			uint32_t c = out.cps[i];
			uint8_t flags = out.flags[i];
			if (flags & kGlyphHidden) {
				out.x[i] = x;
				i++;
				continue;
			}
			if (flags & kGlyphBreak) {
				out.x[i] = x;
				out.w[i] = 0;
				i++;
				forced = true;
				line.pageBreak = c == kPageBreakChar || c == kSectionBreakChar;
				line.sectionBreak = c == kSectionBreakChar;
				break;
			}
			float advance = out.w[i];
			if (flags & kGlyphTab) {
				advance = tabAdvance(i, x, firstLine);
				out.w[i] = advance;
			}
			if (c == ' ') {
				out.x[i] = x;
				x += advance;
				breakAfter = i + 1;
				i++;
				continue;
			}
			if (x + advance > right + 0.05f && i > cp) {
				end = breakAfter > cp ? breakAfter : i;
				break;
			}
			out.x[i] = x;
			x += advance;
			if (BreaksAfter(c) || (flags & kGlyphTab))
				breakAfter = i + 1;
			i++;
		}
		if (end < 0)
			end = i;
		else
			forced = false;
		if (end <= cp && cp < n)
			end = cp + 1;
		line.endCp = end;
		line.forced = forced;
		if (!forced) {
			line.pageBreak = false;
			line.sectionBreak = false;
		}

		// An optional hyphen at the end of a wrapped line is shown.
		if (!forced && end < n && end > cp
			&& out.cps[end - 1] == kSoftHyphenChar) {
			out.flags[end - 1] |= kGlyphHyphen;
			out.w[end - 1] = hyphenWidth(end - 1);
		}

		float endX = left;
		float contentRight = left;
		int32_t lastContent = -1;
		for (int32_t k = cp; k < end; k++) {
			if (out.flags[k] & kGlyphHidden)
				continue;
			endX = out.x[k] + out.w[k];
			if (out.cps[k] != ' ' && !(out.flags[k] & kGlyphBreak)) {
				contentRight = endX;
				lastContent = k;
			}
		}

		bool lastOfParagraph = end >= n
			&& !(forced && n > 0 && out.cps[n - 1] == kLineBreakChar);
		float shift = 0;
		if (pf.align == kAlignCenter)
			shift = std::max(0.0f, (right - contentRight) / 2);
		else if (pf.align == kAlignRight)
			shift = std::max(0.0f, right - contentRight);
		else if (pf.align == kAlignJustify && !forced && !lastOfParagraph
			&& lastContent >= 0) {
			int32_t from = cp;
			for (int32_t k = cp; k < end; k++) {
				if (out.flags[k] & kGlyphTab)
					from = k + 1;
			}
			int32_t spaces = 0;
			for (int32_t k = from; k < lastContent; k++) {
				if (out.cps[k] == ' ' && !(out.flags[k] & kGlyphHidden))
					spaces++;
			}
			float extra = right - contentRight;
			if (spaces > 0 && extra > 0) {
				float each = extra / spaces;
				float added = 0;
				for (int32_t k = from; k < end; k++) {
					out.x[k] += added;
					if (k < lastContent && out.cps[k] == ' '
						&& !(out.flags[k] & kGlyphHidden)) {
						out.w[k] += each;
						added += each;
					}
				}
				contentRight += added;
				endX += added;
			}
		}
		if (shift != 0) {
			for (int32_t k = cp; k < end; k++)
				out.x[k] += shift;
			contentRight += shift;
			endX += shift;
		}
		line.endX = endX;
		line.contentRight = contentRight;

		// Height
		float ascent = 0;
		float descent = 0;
		float leading = 0;
		bool any = false;
		for (int32_t k = cp; k < end; k++) {
			if (out.flags[k] & kGlyphHidden)
				continue;
			const RunMetrics& m = runMetrics[out.runOf[k]];
			if (out.flags[k] & kGlyphPicture) {
				const Picture* picture = document.PictureFor(
					runs[out.runOf[k]].format.picture);
				ascent = std::max(ascent, picture != nullptr
					? picture->height : 36.0f);
				descent = std::max(descent, m.descent);
			} else {
				ascent = std::max(ascent, m.ascent);
				descent = std::max(descent, m.descent);
				leading = std::max(leading, m.leading);
			}
			any = true;
		}
		if (!any) {
			const RunMetrics& m = cp < n ? runMetrics[out.runOf[cp]]
				: runMetrics.back();
			ascent = m.ascent;
			descent = m.descent;
			leading = m.leading;
		}
		float natural = ascent + descent + leading;
		float height;
		switch (pf.lineRule) {
			case kLineAtLeast:
				height = std::max(natural, pf.lineValue);
				break;
			case kLineExactly:
				height = pf.lineValue > 0 ? pf.lineValue : natural;
				break;
			default:
				height = natural * std::max(0.5f, pf.lineValue);
				break;
		}
		line.height = height;
		line.baseline = height - descent - leading;
		line.ascent = ascent;
		line.descent = descent;
		out.lines.push_back(line);

		cp = end;
		firstLine = false;
		if (cp >= n) {
			if (forced && n > 0 && out.cps[n - 1] == kLineBreakChar
				&& out.lines.back().startCp < n)
				continue;	// one more, empty line after a final line break
			break;
		}
	}
	out.lines.back().last = true;

	// Vertical structure
	const ParaBorder& border = pf.border;
	out.spaceBefore = pf.spaceBefore;
	out.spaceAfter = pf.spaceAfter;
	out.borderTop = borderTop;
	out.borderBottom = borderBottom;
	out.borderBetween = borderBetween;
	if (borderTop && border.top)
		out.padTop = border.space + BorderLineWidth(border.top);
	else if (borderBetween && border.between)
		out.padTop = border.space + BorderLineWidth(border.between);
	else if (borderTop && border.Any())
		out.padTop = border.space;
	if (borderBottom && border.bottom)
		out.padBottom = border.space + BorderLineWidth(border.bottom)
			+ (border.shadow ? 2 : 0);
	else if (borderBottom && border.Any())
		out.padBottom = border.space;

	float y = out.spaceBefore + out.padTop;
	for (LineBox& line : out.lines) {
		line.top = y;
		y += line.height;
	}
	out.height = y + out.padBottom + out.spaceAfter;
	out.textLeft = std::min(pf.leftIndent, pf.leftIndent + pf.firstIndent);
	out.textRight = width - pf.rightIndent;
}


// #pragma mark - DocLayout


DocLayout::DocLayout(FontMetrics* metrics)
	:
	fMetrics(metrics),
	fDocument(nullptr),
	fStory(nullptr),
	fPaginate(true),
	fRequestedWidth(0),
	fWidth(0),
	fGeneration(0),
	fPaginating(true),
	fNormalHeight(0)
{
}


void
DocLayout::SetStory(Document* document, Story* story, bool paginate)
{
	fDocument = document;
	fStory = story;
	fPaginate = paginate;
	Invalidate();
}


void
DocLayout::SetOptions(const LayoutOptions& options)
{
	fOptions = options;
}


void
DocLayout::SetWidth(float width)
{
	fRequestedWidth = width;
}


void
DocLayout::Invalidate()
{
	fCache.clear();
	fFootnotes.clear();
	fFootnoteRevisions.clear();
	fParas.clear();
}


const std::vector<ParaLayout>*
DocLayout::FootnoteLayout(int32_t id) const
{
	auto it = fFootnotes.find(id);
	return it == fFootnotes.end() ? nullptr : &it->second;
}


std::string
DocLayout::_KeyFor(const Paragraph& paragraph) const
{
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%c%g", fOptions.showHidden ? 'H' : '-',
		fDocument->settings.defaultTab);
	std::string key = buffer;
	if (paragraph.text.find("\xEE\x80") != std::string::npos) {
		// fields and footnote references depend on the context
		for (size_t r = 0; r < paragraph.runs.size(); r++) {
			int32_t id = paragraph.runs[r].format.footnote;
			if (id != 0 && fOptions.footnoteNumbers != nullptr) {
				auto it = fOptions.footnoteNumbers->find(id);
				key += "f" + std::to_string(it == fOptions.footnoteNumbers->end()
					? 0 : it->second);
			}
		}
		if (paragraph.text.find("\xEE\x80\x80") != std::string::npos
			|| paragraph.text.find("\xEE\x80\x83") != std::string::npos) {
			key += "p" + std::to_string(fOptions.pageNumber) + "/"
				+ std::to_string(fOptions.pageCount) + "/"
				+ std::to_string(fDocument->settings.pageNumberFormat);
		}
		if (paragraph.text.find("\xEE\x80\x81") != std::string::npos
			|| paragraph.text.find("\xEE\x80\x82") != std::string::npos)
			key += "d" + fOptions.date + fOptions.time;
	}
	if (paragraph.text.find("\xEF\xBF\xBC") != std::string::npos) {
		for (const Run& run : paragraph.runs) {
			if (run.format.picture != 0) {
				const Picture* picture = fDocument->PictureFor(
					run.format.picture);
				if (picture != nullptr) {
					snprintf(buffer, sizeof(buffer), "i%gx%g", picture->width,
						picture->height);
					key += buffer;
				}
			}
		}
	}
	return key;
}


const ParaLayout*
DocLayout::_LayoutPara(int32_t index, float width, bool top, bool bottom,
	bool between, bool* changed)
{
	const Paragraph& paragraph = (*fStory)[index];
	std::string key = _KeyFor(paragraph);
	key += top ? 'T' : 't';
	key += bottom ? 'B' : 'b';
	key += between ? 'W' : 'w';
	const OutlineInfo* outline = fOptions.outline
		&& index < (int32_t)fOutline.size() ? &fOutline[index] : nullptr;
	if (outline != nullptr) {
		char buffer[48];
		snprintf(buffer, sizeof(buffer), "O%d%c%c%c", (int)outline->indent,
			outline->visible ? 'v' : 'h',
			fOptions.outlineFirstLine ? 'f' : '-',
			fOptions.outlineFormatting ? 'F' : '-');
		key += buffer;
	}
	auto it = fCache.find(paragraph.revision);
	if (it != fCache.end() && it->second.width == width
		&& it->second.key == key)
		return &it->second;
	ParaLayout& layout = fCache[paragraph.revision];
	if (outline == nullptr) {
		LayoutParagraph(paragraph, *fDocument, *fMetrics, fOptions, width, top,
			bottom, between, layout);
	} else {
		// Outline view ignores paragraph formatting.
		Paragraph shown = paragraph;
		if (!paragraph.IsTablePart()) {
			ParaFormat& format = shown.format;
			format.leftIndent = outline->indent * 36.0f + 18;
			format.firstIndent = 0;
			format.rightIndent = 0;
			format.align = kAlignLeft;
			format.spaceBefore = 0;
			format.spaceAfter = 0;
			format.lineRule = kLineAuto;
			format.lineValue = 1;
			format.border = ParaBorder();
			format.pageBreakBefore = false;
		}
		if (!fOptions.outlineFormatting) {
			CharFormat plain = fDocument->StyleCharFormat(0);
			for (Run& run : shown.runs) {
				CharFormat format = plain;
				format.picture = run.format.picture;
				format.footnote = run.format.footnote;
				format.hidden = run.format.hidden;
				run.format = format;
			}
		}
		LayoutParagraph(shown, *fDocument, *fMetrics, fOptions, width, false,
			false, false, layout);
		bool firstOnly = fOptions.outlineFirstLine && outline->level == 0;
		if (!outline->visible || firstOnly) {
			float y = 0;
			for (size_t l = 0; l < layout.lines.size(); l++) {
				LineBox& line = layout.lines[l];
				if (!outline->visible || l > 0)
					line.height = 0;
				line.top = y;
				y += line.height;
			}
			layout.height = y;
			layout.spaceBefore = layout.spaceAfter = 0;
			layout.invisible = !outline->visible;
		}
	}
	layout.key = key;
	*changed = true;
	return &layout;
}


float
DocLayout::_CellWidth(const RowFormat& row, int index) const
{
	if (index < 0 || index >= (int)row.cells.size())
		return 72;
	float width = row.cells[index].right - row.CellLeft(index) - 2 * row.gap;
	return std::max(width, 6.0f);
}


static bool
SameBorderGroup(const Paragraph& a, const Paragraph& b)
{
	return a.format.border.Any() && a.format.border == b.format.border
		&& a.format.leftIndent == b.format.leftIndent
		&& a.format.rightIndent == b.format.rightIndent
		&& !a.IsTablePart() && !b.IsTablePart();
}


bool
DocLayout::Update()
{
	if (fStory == nullptr || fDocument == nullptr)
		return false;
	Story& story = *fStory;
	const DocSettings& settings = fDocument->settings;
	fPaginating = fPaginate && !fOptions.outline;
	float width = fRequestedWidth > 0 ? fRequestedWidth
		: (fPaginating ? settings.ColumnWidth() : settings.TextWidth());
	fWidth = width;
	bool changed = false;

	fOutline.clear();
	if (fOptions.outline) {
		int32_t count = (int32_t)story.size();
		fOutline.resize(count);
		int32_t heading = 0;
		int32_t hideLevel = 0;		// 0: nothing hidden
		for (int32_t i = 0; i < count; i++) {
			OutlineInfo& info = fOutline[i];
			const Paragraph& paragraph = story[i];
			info.level = paragraph.IsTablePart() ? 0
				: fDocument->styles.OutlineLevel(paragraph.format.style);
			if (info.level > 0) {
				if (hideLevel > 0 && info.level <= hideLevel)
					hideLevel = 0;
				info.visible = hideLevel == 0
					&& (fOptions.outlineShowLevel == 0
						|| info.level <= fOptions.outlineShowLevel);
				info.indent = info.level - 1;
				heading = info.level;
				if (hideLevel == 0 && paragraph.collapsed)
					hideLevel = info.level;
			} else {
				info.visible = hideLevel == 0 && fOptions.outlineShowLevel == 0;
				info.indent = heading;
			}
		}
		for (int32_t i = 0; i + 1 < count; i++) {
			OutlineInfo& info = fOutline[i];
			if (info.level == 0)
				continue;
			int32_t next = fOutline[i + 1].level;
			info.hasChildren = next == 0 || next > info.level;
		}
	}

	fFootnoteNumbersStorage = fDocument->FootnoteNumbers();
	fOptions.footnoteNumbers = &fFootnoteNumbersStorage;

	int32_t count = (int32_t)story.size();
	fParas.assign(count, nullptr);
	for (int32_t i = 0; i < count; i++) {
		const Paragraph& paragraph = story[i];
		float paraWidth = width;
		if (paragraph.kind == kParaCell) {
			int32_t rowStart = i;
			while (rowStart > 0 && story[rowStart - 1].kind == kParaCell)
				rowStart--;
			int32_t rowEnd = i;
			while (rowEnd < count && story[rowEnd].kind == kParaCell)
				rowEnd++;
			if (rowEnd < count && story[rowEnd].kind == kParaRowEnd)
				paraWidth = _CellWidth(story[rowEnd].row, i - rowStart);
		} else if (paragraph.kind == kParaRowEnd)
			paraWidth = 12;
		bool hasBorder = paragraph.format.border.Any()
			&& !paragraph.IsTablePart();
		bool samePrevious = i > 0 && SameBorderGroup(story[i - 1], paragraph);
		bool sameNext = i + 1 < count
			&& SameBorderGroup(paragraph, story[i + 1]);
		fParas[i] = _LayoutPara(i, paraWidth, hasBorder && !samePrevious,
			hasBorder && !sameNext, hasBorder && samePrevious, &changed);
	}

	_LayoutFootnotes(&changed);
	_Place();

	// Forget layouts of paragraphs that are gone.
	if (fCache.size() > (size_t)count + 64) {
		std::unordered_set<uint64_t> used;
		for (const Paragraph& paragraph : story)
			used.insert(paragraph.revision);
		for (auto it = fCache.begin(); it != fCache.end();) {
			if (used.find(it->first) == used.end())
				it = fCache.erase(it);
			else
				++it;
		}
	}
	return changed;
}


void
DocLayout::_LayoutFootnotes(bool* changed)
{
	if (!fPaginating || fStory != &fDocument->body
		|| fDocument->footnotes.empty()) {
		if (!fFootnotes.empty()) {
			fFootnotes.clear();
			fFootnoteRevisions.clear();
			*changed = true;
		}
		return;
	}
	std::set<int32_t> seen;
	for (int32_t id : fDocument->FootnoteOrder()) {
		if (!seen.insert(id).second)
			continue;
		const Story* story = fDocument->StoryFor(kStoryFootnote, id);
		if (story == nullptr)
			continue;
		uint64_t signature = (uint64_t)(fWidth * 64);
		for (const Paragraph& paragraph : *story)
			signature = signature * 1000003u + paragraph.revision;
		signature += fOptions.showHidden ? 7 : 0;
		auto it = fFootnoteRevisions.find(id);
		if (it != fFootnoteRevisions.end() && it->second == signature)
			continue;
		std::vector<ParaLayout>& layouts = fFootnotes[id];
		layouts.assign(story->size(), ParaLayout());
		for (size_t i = 0; i < story->size(); i++) {
			LayoutParagraph((*story)[i], *fDocument, *fMetrics, fOptions,
				fWidth, false, false, false, layouts[i]);
		}
		fFootnoteRevisions[id] = signature;
		*changed = true;
	}
}


float
DocLayout::PageTextLeft(int32_t page) const
{
	if (!fPaginating || fDocument == nullptr)
		return 0;
	const DocSettings& settings = fDocument->settings;
	bool even = (page % 2) == 1;
	if (settings.mirrorMargins && even)
		return settings.marginRight;
	return settings.marginLeft + settings.gutter;
}


void
DocLayout::_Place()
{
	fLines.clear();
	fBlocks.clear();
	fPages.clear();
	fFootnotePlaces.clear();
	Story& story = *fStory;
	int32_t count = (int32_t)story.size();
	fParaFirstLine.assign(count, 0);
	const DocSettings& settings = fDocument->settings;

	int32_t columns = fPaginating ? std::max<int32_t>(1, settings.columns) : 1;
	float columnGap = settings.columnSpacing;
	float columnHeight = fPaginating
		? std::max(36.0f, settings.TextHeight()) : 1e30f;
	float marginTop = fPaginating ? settings.marginTop : 0;
	bool bottomNotes = fPaginating && fStory == &fDocument->body
		&& settings.footnotePos != kFootnoteEndOfDocument
		&& !fFootnotes.empty();

	int32_t page = 0;
	int32_t column = 0;
	float y = 0;
	float normalY = 0;
	float footReserved = 0;
	std::vector<int32_t> columnNotes;

	auto columnX = [&](int32_t onPage, int32_t inColumn) {
		return PageTextLeft(onPage) + inColumn * (fWidth + columnGap);
	};
	auto noteHeight = [&](int32_t id) {
		float height = 0;
		auto it = fFootnotes.find(id);
		if (it != fFootnotes.end()) {
			for (const ParaLayout& layout : it->second)
				height += layout.height;
		}
		return height;
	};
	auto flushNotes = [&]() {
		if (columnNotes.empty())
			return;
		float noteY = columnHeight - footReserved;
		FootnotePlace separator;
		separator.page = page;
		separator.column = column;
		separator.pageTop = marginTop + noteY;
		separator.pageX = columnX(page, column);
		separator.separator = true;
		fFootnotePlaces.push_back(separator);
		noteY += kFootnoteSeparator;
		for (int32_t id : columnNotes) {
			FootnotePlace place;
			place.id = id;
			place.page = page;
			place.column = column;
			place.pageTop = marginTop + noteY;
			place.pageX = columnX(page, column);
			fFootnotePlaces.push_back(place);
			noteY += noteHeight(id);
		}
		columnNotes.clear();
	};
	auto newColumn = [&](bool newPage) {
		flushNotes();
		if (!newPage && column + 1 < columns)
			column++;
		else {
			page++;
			column = 0;
		}
		y = 0;
		footReserved = 0;
	};

	std::set<int32_t> placedNotes;
	int32_t i = 0;
	while (i < count) {
		const Paragraph& paragraph = story[i];
		if (paragraph.kind == kParaCell || paragraph.kind == kParaRowEnd) {
			int32_t j = i;
			while (j < count && story[j].kind == kParaCell)
				j++;
			if (j < count && story[j].kind == kParaRowEnd) {
				const RowFormat& row = story[j].row;
				float rowHeight = 0;
				for (int32_t c = i; c < j; c++)
					rowHeight = std::max(rowHeight, fParas[c]->height);
				if (rowHeight <= 0)
					rowHeight = fParas[j]->height;
				if (row.height > 0)
					rowHeight = std::max(rowHeight, row.height);
				else if (row.height < 0)
					rowHeight = -row.height;
				bool split = fPaginating && rowHeight > columnHeight;
				if (fPaginating && y > 0
					&& y + rowHeight > columnHeight - footReserved)
					newColumn(false);

				Block block;
				block.firstPara = i;
				block.lastPara = j;
				block.row = true;
				block.normalTop = normalY;
				block.normalBottom = normalY + rowHeight;
				block.page = page;
				block.pageTop = marginTop + y;
				block.pageBottom = marginTop + std::min(y + rowHeight,
					columnHeight);
				block.pageX = columnX(page, column);
				fBlocks.push_back(block);

				float rowAlignOffset = 0;
				float rowWidth = row.cells.empty() ? 0 : row.cells.back().right;
				if (row.align == kAlignCenter)
					rowAlignOffset = (fWidth - rowWidth) / 2 - row.leftEdge;
				else if (row.align == kAlignRight)
					rowAlignOffset = fWidth - rowWidth - row.leftEdge;
				float rowLeft = row.leftEdge + std::max(0.0f, rowAlignOffset);
				// A row taller than a page lets the lines of each cell
				// continue on the next pages; the row ends where the
				// longest cell ends.
				int32_t startSlot = page * columns + column;
				int32_t endSlot = startSlot;
				float endY = 0;
				for (int32_t c = i; c < j; c++) {
					int32_t cell = c - i;
					float cellLeft = rowLeft + (cell < (int32_t)row.cells.size()
						? row.CellLeft(cell) : cell * 72.0f) + row.gap;
					fParaFirstLine[c] = (int32_t)fLines.size();
					const ParaLayout& layout = *fParas[c];
					int32_t slot = startSlot;
					float shift = 0;	// height moved to later pages
					for (int32_t l = 0; l < (int32_t)layout.lines.size(); l++) {
						const LineBox& box = layout.lines[l];
						float top = y + box.top - shift;
						if (split && top + box.height > columnHeight
							&& top > 0) {
							shift += top;
							top = 0;
							slot++;
						}
						int32_t onPage = slot / columns;
						int32_t inColumn = slot % columns;
						PlacedLine placed;
						placed.para = c;
						placed.line = l;
						placed.page = onPage;
						placed.column = inColumn;
						placed.normalTop = normalY + box.top;
						placed.normalX = cellLeft;
						placed.pageTop = marginTop + top;
						placed.pageX = columnX(onPage, inColumn) + cellLeft;
						placed.height = box.height;
						fLines.push_back(placed);
						if (slot > endSlot || (slot == endSlot
								&& top + box.height > endY)) {
							endSlot = slot;
							endY = top + box.height;
						}
					}
				}
				fParaFirstLine[j] = (int32_t)fLines.size();
				const ParaLayout& endLayout = *fParas[j];
				for (int32_t l = 0; l < (int32_t)endLayout.lines.size(); l++) {
					PlacedLine placed;
					placed.para = j;
					placed.line = l;
					placed.page = page;
					placed.column = column;
					placed.normalTop = normalY;
					placed.normalX = rowLeft + rowWidth + 3;
					placed.pageTop = marginTop + y;
					placed.pageX = columnX(page, column) + rowLeft + rowWidth + 3;
					placed.height = std::min(rowHeight,
						endLayout.lines[l].height);
					fLines.push_back(placed);
				}
				if (split && endSlot > startSlot) {
					page = endSlot / columns;
					column = endSlot % columns;
					y = endY;
					footReserved = 0;
				} else
					y += rowHeight;
				normalY += rowHeight;
				i = j + 1;
				continue;
			}
			// A malformed row is laid out like normal paragraphs.
		}

		const ParaLayout& layout = *fParas[i];
		const ParaFormat& format = paragraph.format;
		if (fPaginating) {
			if (format.pageBreakBefore && (y > 0 || column > 0))
				newColumn(true);
			float available = columnHeight - footReserved;
			if (format.keepTogether && y > 0 && y + layout.height > available
				&& layout.height <= columnHeight)
				newColumn(false);
			else if (format.keepWithNext && y > 0 && i + 1 < count) {
				float next = 0;
				if (story[i + 1].IsTablePart())
					next = fParas[i + 1]->height;
				else {
					const ParaLayout& nextLayout = *fParas[i + 1];
					next = nextLayout.lines.size() > 1
						? nextLayout.lines[1].top : nextLayout.height;
				}
				float need = layout.height + next;
				if (y + need > available && need <= columnHeight)
					newColumn(false);
			}
		}

		Block block;
		block.firstPara = i;
		block.lastPara = i;
		block.normalTop = normalY;
		block.normalBottom = normalY + layout.height;
		block.page = page;
		block.pageTop = marginTop + y;
		block.pageX = columnX(page, column);

		fParaFirstLine[i] = (int32_t)fLines.size();
		float paraNormalTop = normalY;
		int32_t lineCount = (int32_t)layout.lines.size();
		for (int32_t k = 0; k < lineCount; k++) {
			const LineBox& box = layout.lines[k];
			float slotTop = k == 0 ? 0 : box.top;
			float slotBottom = k + 1 < lineCount ? layout.lines[k + 1].top
				: layout.height;
			float slotHeight = slotBottom - slotTop;
			float fitHeight = k + 1 < lineCount ? slotHeight
				: slotHeight - layout.spaceAfter;

			float noteNeed = 0;
			std::vector<int32_t> notes;
			if (bottomNotes && layout.LineHasFootnotes(k)) {
				for (int32_t c = box.startCp; c < box.endCp; c++) {
					if (layout.cps[c] != kFootnoteChar)
						continue;
					int32_t id = paragraph.runs[layout.runOf[c]].format.footnote;
					if (placedNotes.count(id) || fFootnotes.count(id) == 0)
						continue;
					notes.push_back(id);
					noteNeed += noteHeight(id);
				}
				if (!notes.empty() && columnNotes.empty())
					noteNeed += kFootnoteSeparator;
			}

			if (fPaginating && y > 0
				&& y + fitHeight + footReserved + noteNeed > columnHeight) {
				int32_t moveBack = 0;
				if (settings.widowControl && lineCount > 1
					&& (k == 1 || (k == lineCount - 1 && k >= 2)))
					moveBack = 1;
				std::vector<PlacedLine> moved;
				for (int32_t m = 0; m < moveBack; m++) {
					if (!fLines.empty() && fLines.back().para == i
						&& fLines.back().page == page
						&& fLines.back().column == column) {
						moved.insert(moved.begin(), fLines.back());
						fLines.pop_back();
					}
				}
				newColumn(false);
				for (PlacedLine& line : moved) {
					const LineBox& movedBox = layout.lines[line.line];
					float movedTop = line.line == 0 ? 0 : movedBox.top;
					float movedBottom = layout.lines[line.line + 1].top;
					line.page = page;
					line.column = column;
					line.pageTop = marginTop + y + (movedBox.top - movedTop);
					line.pageX = columnX(page, column);
					y += movedBottom - movedTop;
					fLines.push_back(line);
				}
				if (k == 0 || (moved.size() > 0 && moved[0].line == 0)) {
					block.page = page;
					block.pageTop = marginTop;
					block.pageX = columnX(page, column);
				}
			}

			PlacedLine placed;
			placed.para = i;
			placed.line = k;
			placed.page = page;
			placed.column = column;
			placed.normalTop = paraNormalTop + box.top;
			placed.normalX = 0;
			placed.pageTop = marginTop + y + (box.top - slotTop);
			placed.pageX = columnX(page, column);
			placed.height = box.height;
			fLines.push_back(placed);
			y += slotHeight;

			if (!notes.empty()) {
				for (int32_t id : notes) {
					columnNotes.push_back(id);
					placedNotes.insert(id);
				}
				footReserved += noteNeed;
			}
			if (fPaginating && box.pageBreak)
				newColumn(true);
		}
		normalY = paraNormalTop + layout.height;
		block.pageBottom = marginTop + y;
		fBlocks.push_back(block);
		i++;
	}
	flushNotes();

	// Footnotes at the end of the document
	if (fPaginating && fStory == &fDocument->body && !fFootnotes.empty()
		&& settings.footnotePos == kFootnoteEndOfDocument) {
		bool first = true;
		std::set<int32_t> done;
		for (int32_t id : fDocument->FootnoteOrder()) {
			if (!done.insert(id).second || fFootnotes.count(id) == 0)
				continue;
			float height = noteHeight(id) + (first ? kFootnoteSeparator : 0);
			if (y > 0 && y + height > columnHeight)
				newColumn(false);
			if (first) {
				FootnotePlace separator;
				separator.page = page;
				separator.column = column;
				separator.pageTop = marginTop + y;
				separator.pageX = columnX(page, column);
				separator.separator = true;
				fFootnotePlaces.push_back(separator);
				y += kFootnoteSeparator;
				first = false;
			}
			FootnotePlace place;
			place.id = id;
			place.page = page;
			place.column = column;
			place.pageTop = marginTop + y;
			place.pageX = columnX(page, column);
			fFootnotePlaces.push_back(place);
			y += noteHeight(id);
		}
	}

	fNormalHeight = normalY;

	// Pages
	int32_t pageCount = page + 1;
	fPages.assign(pageCount, PageInfo());
	for (int32_t p = 0; p < pageCount; p++) {
		fPages[p].firstLine = -1;
		fPages[p].endLine = -1;
		fPages[p].number = settings.pageNumberStart + p;
	}
	for (int32_t l = 0; l < (int32_t)fLines.size(); l++) {
		int32_t onPage = std::min(fLines[l].page, pageCount - 1);
		if (fPages[onPage].firstLine < 0)
			fPages[onPage].firstLine = l;
		fPages[onPage].endLine = l + 1;
	}
	int32_t previousEnd = 0;
	for (PageInfo& info : fPages) {
		if (info.firstLine < 0) {
			info.firstLine = previousEnd;
			info.endLine = previousEnd;
		}
		previousEnd = info.endLine;
	}
}


// #pragma mark - geometry


int32_t
DocLayout::LineIndexOf(Pos pos, bool lineEnd) const
{
	if (pos.para < 0 || pos.para >= ParaCount())
		return 0;
	const ParaLayout& layout = Para(pos.para);
	return layout.LineForCp(layout.CpForByte(pos.offset), lineEnd);
}


int32_t
DocLayout::PageOfPos(Pos pos) const
{
	if (pos.para < 0 || pos.para >= ParaCount())
		return 0;
	return Placed(pos.para, LineIndexOf(pos, false)).page;
}


bool
DocLayout::Caret(Pos pos, int geometry, bool lineEnd, CaretInfo* info) const
{
	if (pos.para < 0 || pos.para >= ParaCount() || fLines.empty())
		return false;
	const ParaLayout& layout = Para(pos.para);
	int32_t cp = layout.CpForByte(pos.offset);
	int32_t line = layout.LineForCp(cp, lineEnd);
	const LineBox& box = layout.lines[line];
	float x = layout.CaretX(cp, line);
	const PlacedLine& placed = Placed(pos.para, line);
	if (geometry == kGeomPage) {
		info->page = placed.page;
		info->x = placed.pageX + x;
		info->top = placed.pageTop;
	} else {
		info->page = 0;
		info->x = placed.normalX + x;
		info->top = placed.normalTop;
	}
	info->bottom = info->top + box.height;
	info->baseline = info->top + box.baseline;
	return true;
}


int32_t
DocLayout::FindBlock(int geometry, int32_t page, float y) const
{
	if (fBlocks.empty())
		return -1;
	if (geometry == kGeomNormal) {
		int32_t low = 0;
		int32_t high = (int32_t)fBlocks.size() - 1;
		while (low < high) {
			int32_t middle = (low + high) / 2;
			if (fBlocks[middle].normalBottom <= y)
				low = middle + 1;
			else
				high = middle;
		}
		return low;
	}
	for (int32_t b = 0; b < (int32_t)fBlocks.size(); b++) {
		if (fBlocks[b].page == page && fBlocks[b].pageBottom > y)
			return b;
	}
	return (int32_t)fBlocks.size() - 1;
}


Pos
DocLayout::HitTest(int geometry, int32_t page, float x, float y,
	bool* lineEnd) const
{
	if (lineEnd)
		*lineEnd = false;
	if (fLines.empty())
		return Pos();

	int32_t firstCandidate = 0;
	int32_t endCandidate = (int32_t)fLines.size();
	if (geometry == kGeomNormal) {
		int32_t block = FindBlock(geometry, 0, y);
		if (block >= 0) {
			firstCandidate = fParaFirstLine[fBlocks[block].firstPara];
			int32_t last = fBlocks[block].lastPara;
			endCandidate = last + 1 < ParaCount()
				? fParaFirstLine[last + 1] : (int32_t)fLines.size();
		}
	} else {
		page = std::max<int32_t>(0, std::min<int32_t>(page, PageCount() - 1));
		firstCandidate = fPages[page].firstLine;
		endCandidate = fPages[page].endLine;
		if (firstCandidate >= endCandidate) {
			// empty page: use the first line of the next non-empty page
			for (int32_t p = page; p < PageCount(); p++) {
				if (fPages[p].firstLine < fPages[p].endLine) {
					firstCandidate = fPages[p].firstLine;
					endCandidate = firstCandidate + 1;
					break;
				}
			}
			if (firstCandidate >= endCandidate) {
				firstCandidate = (int32_t)fLines.size() - 1;
				endCandidate = (int32_t)fLines.size();
			}
		}
	}

	int32_t best = firstCandidate;
	float bestScore = 1e30f;
	for (int pass = 0; pass < 2 && bestScore >= 1e30f; pass++) {
	if (pass == 1) {
		// everything near was hidden (Outline view): look at all lines
		firstCandidate = 0;
		endCandidate = (int32_t)fLines.size();
	}
	for (int32_t l = firstCandidate; l < endCandidate; l++) {
		const PlacedLine& placed = fLines[l];
		if (placed.height <= 0)
			continue;
		const ParaLayout& layout = Para(placed.para);
		float top = geometry == kGeomPage ? placed.pageTop : placed.normalTop;
		float left = geometry == kGeomPage ? placed.pageX : placed.normalX;
		float dy = y < top ? top - y : (y >= top + placed.height
			? y - top - placed.height + 0.001f : 0);
		float frameLeft = left + std::min(0.0f, layout.textLeft);
		float frameRight = left + std::max(layout.width,
			layout.lines[placed.line].endX);
		if ((*fStory)[placed.para].kind == kParaRowEnd)
			frameRight = left + 12;
		float dx = x < frameLeft ? frameLeft - x
			: (x > frameRight ? x - frameRight : 0);
		float score = dy * 4 + dx;
		if (score < bestScore) {
			bestScore = score;
			best = l;
		}
	}
	}

	const PlacedLine& placed = fLines[best];
	const ParaLayout& layout = Para(placed.para);
	const LineBox& box = layout.lines[placed.line];
	float localX = x - (geometry == kGeomPage ? placed.pageX : placed.normalX);
	int32_t limit = box.forced ? box.endCp - 1 : box.endCp;
	int32_t result = -1;
	for (int32_t k = box.startCp; k < limit; k++) {
		if (layout.flags[k] & kGlyphHidden)
			continue;
		if (localX < layout.x[k] + layout.w[k] / 2) {
			result = k;
			break;
		}
	}
	if (result < 0) {
		result = std::max(limit, box.startCp);
		if (lineEnd && !box.forced && !box.last && result == box.endCp)
			*lineEnd = true;
	}
	return Pos(placed.para, layout.cpByte[result]);
}


void
DocLayout::SelectionRects(Pos start, Pos end, int geometry, float markWidth,
	std::vector<SelectionRect>& rects) const
{
	rects.clear();
	if (fLines.empty() || !(start < end))
		return;
	int32_t lastPara = std::min(end.para, ParaCount() - 1);
	for (int32_t p = std::max<int32_t>(0, start.para); p <= lastPara; p++) {
		const ParaLayout& layout = Para(p);
		int32_t a = p == start.para ? layout.CpForByte(start.offset) : 0;
		int32_t b = p == end.para ? layout.CpForByte(end.offset)
			: layout.Count();
		bool includesMark = p < end.para;
		for (int32_t l = 0; l < (int32_t)layout.lines.size(); l++) {
			const LineBox& box = layout.lines[l];
			int32_t s = std::max(a, box.startCp);
			int32_t e = std::min(b, box.endCp);
			bool continues = b > box.endCp || (includesMark && box.last);
			if (s > e || (s == e && !continues))
				continue;
			if (s >= box.endCp && !(includesMark && box.last)
				&& box.endCp > box.startCp)
				continue;
			float x0 = s < box.endCp && s < layout.Count()
				? layout.x[s] : box.endX;
			float x1 = e > s ? layout.x[e - 1] + layout.w[e - 1] : x0;
			if (continues) {
				x1 = std::max(x1, box.endX);
				if (box.last && includesMark)
					x1 += markWidth;
			}
			if (x1 <= x0)
				continue;
			const PlacedLine& placed = Placed(p, l);
			SelectionRect rect;
			rect.page = geometry == kGeomPage ? placed.page : 0;
			float left = geometry == kGeomPage ? placed.pageX : placed.normalX;
			float top = geometry == kGeomPage ? placed.pageTop
				: placed.normalTop;
			rect.left = left + x0;
			rect.right = left + x1;
			rect.top = top;
			rect.bottom = top + box.height;
			rects.push_back(rect);
		}
	}
}


Pos
DocLayout::LineStart(Pos pos, bool lineEnd) const
{
	if (pos.para < 0 || pos.para >= ParaCount())
		return pos;
	const ParaLayout& layout = Para(pos.para);
	int32_t line = layout.LineForCp(layout.CpForByte(pos.offset), lineEnd);
	return Pos(pos.para, layout.cpByte[layout.lines[line].startCp]);
}


Pos
DocLayout::LineEnd(Pos pos, bool lineEnd, bool* endAffinity) const
{
	*endAffinity = false;
	if (pos.para < 0 || pos.para >= ParaCount())
		return pos;
	const ParaLayout& layout = Para(pos.para);
	int32_t line = layout.LineForCp(layout.CpForByte(pos.offset), lineEnd);
	const LineBox& box = layout.lines[line];
	if (box.forced)
		return Pos(pos.para, layout.cpByte[std::max(box.startCp,
			box.endCp - 1)]);
	if (!box.last)
		*endAffinity = true;
	return Pos(pos.para, layout.cpByte[box.endCp]);
}


Pos
DocLayout::MoveVertical(Pos pos, bool lineEnd, int geometry, float goalX,
	int lines, bool* newLineEnd) const
{
	*newLineEnd = false;
	if (fLines.empty() || pos.para < 0 || pos.para >= ParaCount())
		return pos;
	int32_t index = FirstLineOf(pos.para) + LineIndexOf(pos, lineEnd);
	int32_t target = std::max<int32_t>(0, std::min<int32_t>(
		index + lines, (int32_t)fLines.size() - 1));
	// hidden lines (Outline view) do not count
	int32_t direction = lines > 0 ? 1 : -1;
	while (target > 0 && target < (int32_t)fLines.size() - 1
		&& fLines[target].height <= 0)
		target += direction;
	if (fLines[target].height <= 0)
		target = index;
	// skip table row end markers
	while (target > 0 && target < (int32_t)fLines.size()
		&& (*fStory)[fLines[target].para].kind == kParaRowEnd
		&& target != index) {
		target += lines > 0 ? 1 : -1;
		target = std::max<int32_t>(0, std::min<int32_t>(target,
			(int32_t)fLines.size() - 1));
		if (target == 0 || target == (int32_t)fLines.size() - 1)
			break;
	}
	const PlacedLine& placed = fLines[target];
	const ParaLayout& layout = Para(placed.para);
	const LineBox& box = layout.lines[placed.line];
	float localX = goalX - (geometry == kGeomPage ? placed.pageX
		: placed.normalX);
	int32_t limit = box.forced ? box.endCp - 1 : box.endCp;
	for (int32_t k = box.startCp; k < limit; k++) {
		if (layout.flags[k] & kGlyphHidden)
			continue;
		if (localX < layout.x[k] + layout.w[k] / 2)
			return Pos(placed.para, layout.cpByte[k]);
	}
	int32_t result = std::max(limit, box.startCp);
	if (!box.forced && !box.last && result == box.endCp)
		*newLineEnd = true;
	return Pos(placed.para, layout.cpByte[result]);
}

}	// namespace quill
