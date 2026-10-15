/*
 * Quill - drawing laid out text.
 */
#include "Painter.h"

#include "Fonts.h"
#include "Text.h"

#include <DataIO.h>
#include <TranslationUtils.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace quill;

static const rgb_color kBlack = { 0, 0, 0, 255 };
static const rgb_color kWhite = { 255, 255, 255, 255 };
static const rgb_color kMarkColor = { 70, 70, 70, 255 };
static const rgb_color kGridColor = { 170, 170, 170, 255 };


rgb_color
ColorFor(uint32 color)
{
	if (color == kColorAuto)
		return kBlack;
	rgb_color result = { (uint8)((color >> 16) & 0xFF),
		(uint8)((color >> 8) & 0xFF), (uint8)(color & 0xFF), 255 };
	return result;
}


PictureCache::~PictureCache()
{
	Clear();
}


void
PictureCache::Clear()
{
	for (auto& entry : fBitmaps)
		delete entry.second;
	fBitmaps.clear();
}


BBitmap*
PictureCache::Decode(const Picture* picture)
{
	if (picture == nullptr || picture->data.empty())
		return nullptr;
	BMemoryIO input(picture->data.data(), picture->data.size());
	return BTranslationUtils::GetBitmap(&input);
}


BBitmap*
PictureCache::Get(const Picture* picture)
{
	auto it = fBitmaps.find(picture);
	if (it != fBitmaps.end())
		return it->second;
	BBitmap* bitmap = Decode(picture);
	fBitmaps[picture] = bitmap;
	return bitmap;
}


void
StrokeDotted(BView* view, BPoint from, BPoint to, float step)
{
	step = std::max(2.0f, step);
	if (fabs(to.y - from.y) < 0.5f) {
		float y = floorf(from.y);
		for (float x = std::min(from.x, to.x); x <= std::max(from.x, to.x);
				x += step)
			view->FillRect(BRect(floorf(x), y, floorf(x), y));
	} else {
		float x = floorf(from.x);
		for (float y = std::min(from.y, to.y); y <= std::max(from.y, to.y);
				y += step)
			view->FillRect(BRect(x, floorf(y), x, floorf(y)));
	}
}


static void
StrokeBorderLine(BView* view, uint8 line, BPoint from, BPoint to,
	float zoom, bool vertical)
{
	float width = std::max(1.0f, roundf(BorderLineWidth(line) * zoom));
	view->SetHighColor(kBlack);
	switch (line) {
		case kBorderNone:
			return;
		case kBorderDotted:
			StrokeDotted(view, from, to, 2);
			return;
		case kBorderDouble: {
			float gap = std::max(2.0f, roundf(zoom * 1.5f));
			float half = gap / 2 + 0.5f;
			if (vertical) {
				view->StrokeLine(from - BPoint(half, 0), to - BPoint(half, 0));
				view->StrokeLine(from + BPoint(half, 0), to + BPoint(half, 0));
			} else {
				view->StrokeLine(from - BPoint(0, half), to - BPoint(0, half));
				view->StrokeLine(from + BPoint(0, half), to + BPoint(0, half));
			}
			return;
		}
		case kBorderHairline:
			view->SetHighColor(80, 80, 80);
			view->StrokeLine(from, to);
			return;
		default:
			view->SetPenSize(width);
			view->StrokeLine(from, to);
			view->SetPenSize(1);
			return;
	}
}


namespace {

struct Segment {
	std::string			text;
	std::vector<BPoint>	locations;
};


std::string
ShownText(uint32 c, const CharFormat& format, bool small)
{
	uint32 shown = c;
	if (format.allCaps || small)
		shown = ToUpper(c);
	if (shown == kNbspChar)
		shown = ' ';
	else if (shown == kNbHyphenChar)
		shown = '-';
	return Utf8(shown);
}


void
DrawTextEffects(BView* view, const CharFormat& format, const Segment& segment,
	float zoom, rgb_color color)
{
	const char* text = segment.text.c_str();
	int32 length = (int32)segment.text.size();
	int32 count = (int32)segment.locations.size();
	if (count == 0)
		return;
	if (format.outline || format.shadow) {
		float step = std::max(1.0f, roundf(zoom * format.size / 24));
		std::vector<BPoint> moved(segment.locations);
		if (format.shadow) {
			for (int32 i = 0; i < count; i++)
				moved[i] = segment.locations[i] + BPoint(step * 2, step * 2);
			view->SetHighColor(color);
			view->DrawString(text, length, moved.data(), count);
		}
		static const float kOffsets[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 },
			{ 0, 1 } };
		view->SetHighColor(color);
		for (const auto& offset : kOffsets) {
			for (int32 i = 0; i < count; i++) {
				moved[i] = segment.locations[i]
					+ BPoint(offset[0] * step, offset[1] * step);
			}
			view->DrawString(text, length, moved.data(), count);
		}
		view->SetHighColor(kWhite);
		view->DrawString(text, length, segment.locations.data(), count);
		return;
	}
	view->SetHighColor(color);
	view->DrawString(text, length, segment.locations.data(), count);
}

}	// namespace


void
PaintLine(PaintContext& context, const Paragraph& paragraph,
	const ParaLayout& layout, int32 lineIndex, BPoint origin)
{
	BView* view = context.view;
	const float zoom = context.zoom;
	const LineBox& line = layout.lines[lineIndex];
	const ParaFormat& pf = paragraph.format;
	const ParaBorder& border = pf.border;
	float lineTop = origin.y;
	float lineBottom = origin.y + line.height * zoom;

	// Paragraph shading and borders belong to the line's slot.
	if (border.Any() && !paragraph.IsTablePart()) {
		float slotTop = lineTop;
		float slotBottom = lineBottom;
		if (lineIndex == 0)
			slotTop -= layout.padTop * zoom;
		if (line.last)
			slotBottom += layout.padBottom * zoom;
		float lineWidth = BorderLineWidth(std::max(border.left, border.right));
		float left = origin.x + (layout.textLeft - border.space - lineWidth / 2)
			* zoom;
		float right = origin.x + (layout.textRight + border.space
			+ lineWidth / 2) * zoom;
		if (border.shading > 0) {
			uint8 gray = (uint8)(255 - border.shading * 255 / 100);
			view->SetHighColor(gray, gray, gray);
			view->FillRect(BRect(left, slotTop, right, slotBottom));
		}
		if (lineIndex == 0) {
			float y = slotTop + BorderLineWidth(layout.borderBetween
				? border.between : border.top) * zoom / 2;
			if (layout.borderTop && border.top)
				StrokeBorderLine(view, border.top, BPoint(left, y),
					BPoint(right, y), zoom, false);
			else if (layout.borderBetween && border.between)
				StrokeBorderLine(view, border.between, BPoint(left, y),
					BPoint(right, y), zoom, false);
		}
		if (line.last && layout.borderBottom && border.bottom) {
			float y = slotBottom - (BorderLineWidth(border.bottom)
				+ (border.shadow ? 2 : 0)) * zoom / 2;
			StrokeBorderLine(view, border.bottom, BPoint(left, y),
				BPoint(right, y), zoom, false);
			if (border.shadow) {
				view->SetHighColor(kBlack);
				view->FillRect(BRect(left + 2 * zoom, y + 1,
					right + 2 * zoom, y + 2 * zoom));
			}
		}
		if (border.left)
			StrokeBorderLine(view, border.left, BPoint(left, slotTop),
				BPoint(left, slotBottom), zoom, true);
		if (border.right) {
			StrokeBorderLine(view, border.right, BPoint(right, slotTop),
				BPoint(right, slotBottom), zoom, true);
			if (border.shadow) {
				view->SetHighColor(kBlack);
				view->FillRect(BRect(right + 1, slotTop + 2 * zoom,
					right + 2 * zoom, slotBottom));
			}
		}
	}

	// Bar tabs are vertical lines through every line of the paragraph.
	if (!layout.barTabs.empty()) {
		view->SetHighColor(kBlack);
		for (float bar : layout.barTabs) {
			float x = origin.x + bar * zoom;
			view->StrokeLine(BPoint(x, lineTop), BPoint(x, lineBottom - 1));
		}
	}

	view->SetDrawingMode(B_OP_OVER);
	float baseline = origin.y + line.baseline * zoom;
	int32 k = line.startCp;
	while (k < line.endCp) {
		uint8 flags = layout.flags[k];
		uint32 c = layout.cps[k];
		if (flags & kGlyphHidden) {
			k++;
			continue;
		}
		const CharFormat& format = paragraph.runs[layout.runOf[k]].format;
		float x = origin.x + layout.x[k] * zoom;
		float y = baseline - format.position * zoom;
		rgb_color color = ColorFor(format.color);
		BFont font = FontCache::Get().FontFor(format, format.size * zoom);

		if (flags & kGlyphTab) {
			float width = layout.w[k] * zoom;
			auto leader = layout.tabLeader.find(k);
			if (leader != layout.tabLeader.end() && width > 4) {
				view->SetHighColor(color);
				if (leader->second == kLeaderLine) {
					view->StrokeLine(BPoint(x, y + 1), BPoint(x + width - 2,
						y + 1));
				} else {
					const char* mark = leader->second == kLeaderDot ? "." : "-";
					view->SetFont(&font);
					float step = std::max(2.0f, font.StringWidth(mark) * 1.6f);
					// align leader characters to a grid like Word does
					float start = ceilf((x + 2) / step) * step;
					for (float dot = start; dot + step <= x + width;
							dot += step)
						view->DrawString(mark, BPoint(dot, y));
				}
			}
			if (context.showAll && !context.printing) {
				BFont markFont(font);
				markFont.SetFace(B_REGULAR_FACE);
				view->SetFont(&markFont);
				view->SetHighColor(kMarkColor);
				float arrowWidth = markFont.StringWidth("\xE2\x86\x92");
				float ax = x + std::max(0.0f, (width - arrowWidth) / 2);
				view->DrawString("\xE2\x86\x92", BPoint(ax, baseline));
			}
			k++;
			continue;
		}
		if (flags & kGlyphBreak) {
			if (context.showAll && !context.printing && c == kLineBreakChar) {
				view->SetFont(&font);
				view->SetHighColor(kMarkColor);
				view->DrawString("\xE2\x86\xB5", BPoint(x + zoom, baseline));
			}
			k++;
			continue;
		}
		if (flags & kGlyphPicture) {
			const Picture* picture = context.document->PictureFor(
				format.picture);
			float width = layout.w[k] * zoom;
			float height = (picture != nullptr ? picture->height : 36) * zoom;
			BRect frame(x, baseline - height, x + width - 1, baseline - 1);
			BBitmap* bitmap = context.pictures != nullptr
				? context.pictures->Get(picture) : nullptr;
			if (bitmap != nullptr) {
				view->SetDrawingMode(B_OP_ALPHA);
				view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
				view->DrawBitmap(bitmap, bitmap->Bounds(), frame,
					B_FILTER_BITMAP_BILINEAR);
				view->SetDrawingMode(B_OP_OVER);
			} else {
				view->SetHighColor(230, 230, 230);
				view->FillRect(frame);
				view->SetHighColor(kBlack);
				view->StrokeRect(frame);
				view->StrokeLine(frame.LeftTop(), frame.RightBottom());
				view->StrokeLine(frame.LeftBottom(), frame.RightTop());
			}
			k++;
			continue;
		}
		if (flags & kGlyphField) {
			auto field = layout.fieldText.find(k);
			if (field != layout.fieldText.end()) {
				Segment segment;
				segment.text = field->second;
				std::vector<float> widths(CountChars(field->second));
				FontCache::Get().Widths(format, format.size,
					field->second.c_str(), (int32)field->second.size(),
					(int32)widths.size(), widths.data());
				float fx = x;
				for (float width : widths) {
					segment.locations.push_back(BPoint(fx, y));
					fx += (width + format.spacing) * zoom;
				}
				view->SetFont(&font);
				DrawTextEffects(view, format, segment, zoom, color);
				if (format.underline != kULNone) {
					view->SetHighColor(color);
					float uy = y + std::max(1.0f, roundf(format.size * zoom
						* 0.12f));
					view->StrokeLine(BPoint(x, uy), BPoint(fx - 1, uy));
				}
			}
			k++;
			continue;
		}
		if (c == kSoftHyphenChar) {
			if (flags & kGlyphHyphen) {
				view->SetFont(&font);
				view->SetHighColor(color);
				view->DrawString("-", BPoint(x, y));
			} else if (context.showAll && !context.printing) {
				view->SetFont(&font);
				view->SetHighColor(kMarkColor);
				view->DrawString("\xC2\xAC", BPoint(x, y));
			}
			k++;
			continue;
		}

		// A run of plain characters with the same format.
		bool small = (flags & kGlyphSmall) != 0;
		int32 run = layout.runOf[k];
		int32 end = k;
		Segment segment;
		while (end < line.endCp && layout.runOf[end] == run) {
			uint8 f = layout.flags[end];
			if (f & (kGlyphHidden | kGlyphTab | kGlyphBreak | kGlyphPicture
					| kGlyphField))
				break;
			if (layout.cps[end] == kSoftHyphenChar)
				break;
			if (((f & kGlyphSmall) != 0) != small)
				break;
			segment.text += ShownText(layout.cps[end], format, small);
			segment.locations.push_back(BPoint(origin.x
				+ layout.x[end] * zoom, y));
			end++;
		}
		if (small) {
			BFont smallFont = FontCache::Get().FontFor(format,
				format.size * 0.8f * zoom);
			view->SetFont(&smallFont);
		} else
			view->SetFont(&font);
		DrawTextEffects(view, format, segment, zoom, color);

		// Underline and strikethrough
		bool hiddenShown = format.hidden;
		if (format.underline != kULNone || format.strike || hiddenShown) {
			font_height height;
			font.GetHeight(&height);
			float thickness = std::max(1.0f, roundf(format.size * zoom / 16));
			float uy = roundf(y + std::max(1.0f, height.descent * 0.35f));
			// trailing spaces at the end of a line are not underlined
			int32 last = end - 1;
			if (end == line.endCp || layout.cps[end - 1] == ' ') {
				while (last >= k && (layout.cps[last] == ' '
						|| layout.cps[last] == kNbspChar)
					&& (end == line.endCp))
					last--;
			}
			view->SetHighColor(color);
			view->SetPenSize(thickness);
			if (last >= k && format.underline != kULNone) {
				float x0 = origin.x + layout.x[k] * zoom;
				float x1 = origin.x + (layout.x[last] + layout.w[last]) * zoom;
				switch (format.underline) {
					case kULWord: {
						int32 i = k;
						while (i <= last) {
							while (i <= last && (layout.cps[i] == ' '
									|| layout.cps[i] == kNbspChar))
								i++;
							int32 j = i;
							while (j <= last && layout.cps[j] != ' '
								&& layout.cps[j] != kNbspChar)
								j++;
							if (j > i) {
								view->StrokeLine(BPoint(origin.x
									+ layout.x[i] * zoom, uy),
									BPoint(origin.x + (layout.x[j - 1]
										+ layout.w[j - 1]) * zoom - 1, uy));
							}
							i = j;
						}
						break;
					}
					case kULDouble:
						view->StrokeLine(BPoint(x0, uy), BPoint(x1 - 1, uy));
						view->StrokeLine(BPoint(x0, uy + thickness + 1),
							BPoint(x1 - 1, uy + thickness + 1));
						break;
					case kULDotted:
						view->SetPenSize(1);
						for (float t = 0; t < thickness; t++) {
							StrokeDotted(view, BPoint(x0, uy + t),
								BPoint(x1 - 1, uy + t), 2);
						}
						break;
					default:
						view->StrokeLine(BPoint(x0, uy), BPoint(x1 - 1, uy));
						break;
				}
			}
			if (format.strike && end > k) {
				float sy = roundf(y - height.ascent * 0.3f);
				view->StrokeLine(BPoint(origin.x + layout.x[k] * zoom, sy),
					BPoint(origin.x + (layout.x[end - 1] + layout.w[end - 1])
						* zoom - 1, sy));
			}
			if (hiddenShown && !context.printing && end > k) {
				view->SetPenSize(1);
				view->SetHighColor(kMarkColor);
				StrokeDotted(view, BPoint(origin.x + layout.x[k] * zoom,
					uy + thickness + 1), BPoint(origin.x + (layout.x[end - 1]
					+ layout.w[end - 1]) * zoom - 1, uy + thickness + 1), 2);
			}
			view->SetPenSize(1);
		}

		if (context.showAll && !context.printing) {
			BFont markFont(font);
			markFont.SetFace(B_REGULAR_FACE);
			view->SetFont(&markFont);
			view->SetHighColor(kMarkColor);
			float dotWidth = markFont.StringWidth("\xC2\xB7");
			for (int32 i = k; i < end; i++) {
				uint32 ci = layout.cps[i];
				if (ci != ' ' && ci != kNbspChar)
					continue;
				float cx = origin.x + (layout.x[i] + layout.w[i] / 2) * zoom
					- dotWidth / 2;
				view->DrawString(ci == ' ' ? "\xC2\xB7" : "\xC2\xB0",
					BPoint(cx, baseline));
			}
		}
		k = end;
	}

	if (line.last && context.showAll && !context.printing) {
		const CharFormat& mark = paragraph.MarkFormat();
		CharFormat plain = mark;
		plain.bold = plain.italic = false;
		BFont font = FontCache::Get().FontFor(plain, mark.size * zoom);
		view->SetFont(&font);
		view->SetHighColor(kMarkColor);
		const char* symbol = paragraph.IsTablePart() ? "\xC2\xA4" : "\xC2\xB6";
		view->DrawString(symbol, BPoint(origin.x + line.endX * zoom + 1,
			baseline));
	}
	view->SetDrawingMode(B_OP_COPY);
}


void
PaintRowGrid(PaintContext& context, const RowFormat& row, float left,
	float top, float height, float layoutWidth, bool gridlines)
{
	BView* view = context.view;
	float zoom = context.zoom;
	if (row.cells.empty())
		return;
	float rowWidth = row.cells.back().right;
	float alignOffset = 0;
	if (row.align == kAlignCenter)
		alignOffset = (layoutWidth - rowWidth) / 2 - row.leftEdge;
	else if (row.align == kAlignRight)
		alignOffset = layoutWidth - rowWidth - row.leftEdge;
	float rowLeft = left + (row.leftEdge + std::max(0.0f, alignOffset)) * zoom;
	float bottom = top + height - 1;
	for (size_t i = 0; i < row.cells.size(); i++) {
		const CellFormat& cell = row.cells[i];
		float x0 = rowLeft + row.CellLeft((int)i) * zoom;
		float x1 = rowLeft + cell.right * zoom;
		BRect frame(x0, top, x1, bottom);
		if (cell.shading > 0) {
			uint8 gray = (uint8)(255 - cell.shading * 255 / 100);
			view->SetHighColor(gray, gray, gray);
			view->FillRect(frame);
		}
		bool any = cell.top || cell.bottom || cell.left || cell.rightLine;
		if (gridlines && !context.printing && !any) {
			view->SetHighColor(kGridColor);
			StrokeDotted(view, frame.LeftTop(), frame.RightTop(), 2);
			StrokeDotted(view, frame.LeftBottom(), frame.RightBottom(), 2);
			StrokeDotted(view, frame.LeftTop(), frame.LeftBottom(), 2);
			StrokeDotted(view, frame.RightTop(), frame.RightBottom(), 2);
		}
		if (cell.top)
			StrokeBorderLine(view, cell.top, frame.LeftTop(), frame.RightTop(),
				zoom, false);
		if (cell.bottom)
			StrokeBorderLine(view, cell.bottom, frame.LeftBottom(),
				frame.RightBottom(), zoom, false);
		if (cell.left)
			StrokeBorderLine(view, cell.left, frame.LeftTop(),
				frame.LeftBottom(), zoom, true);
		if (cell.rightLine)
			StrokeBorderLine(view, cell.rightLine, frame.RightTop(),
				frame.RightBottom(), zoom, true);
	}
}
