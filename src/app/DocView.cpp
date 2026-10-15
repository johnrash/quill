/*
 * Quill - the document view.
 */
#include "DocView.h"

#include "Constants.h"
#include "Fonts.h"
#include "Prefs.h"
#include "RTF.h"
#include "Text.h"
#include "TextIO.h"
#include "Tools.h"

#include <Application.h>
#include <Bitmap.h>
#include <Clipboard.h>
#include <Cursor.h>
#include <MessageRunner.h>
#include <NodeInfo.h>
#include <Window.h>
#include <Region.h>
#include <ScrollBar.h>
#include <Window.h>

#include <algorithm>
#include <cmath>
#include <ctime>

using namespace quill;

float DocView::kNormalLeft = 24;
static const float kNormalTop = 6;
static const float kPageGap = 18;

enum {
	kCursorDefault = 0,
	kCursorBeam,
	kCursorBar
};


static BCursor*
SelectionBarCursor()
{
	// An arrow pointing up and to the right, like Word's selection bar.
	static const char* kShape[16] = {
		"..........#.....",
		".........##.....",
		"........#o#.....",
		".......#oo#.....",
		"......#ooo#.....",
		".....#oooo#.....",
		"....#ooooo#.....",
		"...#oooooo#.....",
		"..#ooooooo#.....",
		".#####oooo#.....",
		"......#ooo#.....",
		".......#oo#.....",
		"........#o#.....",
		".........##.....",
		"..........#.....",
		"................"
	};
	static BCursor* sCursor = nullptr;
	if (sCursor == nullptr) {
		uint8 data[68];
		memset(data, 0, sizeof(data));
		data[0] = 16;
		data[1] = 1;
		data[2] = 0;	// hot spot y
		data[3] = 10;	// hot spot x
		for (int row = 0; row < 16; row++) {
			for (int column = 0; column < 16; column++) {
				char c = kShape[row][column];
				int byte = row * 2 + column / 8;
				int bit = 7 - (column % 8);
				if (c == '#')
					data[4 + byte] |= 1 << bit;
				if (c == '#' || c == 'o')
					data[36 + byte] |= 1 << bit;
			}
		}
		sCursor = new BCursor(data);
	}
	return sCursor;
}


DocView::DocView(const char* name, Document* document, Story* story,
	bool paginate, PictureCache* pictures)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS | B_PULSE_NEEDED
		| B_NAVIGABLE),
	fListener(nullptr),
	fDocument(document),
	fStory(story),
	fEditor(document, story),
	fLayout(&gMetrics),
	fPictures(pictures),
	fPaginate(paginate),
	fMode(kModeNormal),
	fZoom(1.0f),
	fShowAll(false),
	fFieldPage(1),
	fOffscreen(nullptr),
	fOffscreenView(nullptr),
	fHorizontal(nullptr),
	fVertical(nullptr),
	fLineEnd(false),
	fGoalX(0),
	fGoalValid(false),
	fCaretOn(true),
	fActive(true),
	fTracking(false),
	fUnit(kUnitChar),
	fAutoScroll(nullptr),
	fDragPending(false),
	fDropShown(false),
	fCursorKind(-1)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	fLayout.SetStory(document, story, paginate);
	fRenderer = new PageRenderer(document, pictures);
	LayoutOptions options;
	options.showHidden = gPrefs.showHidden;
	fLayout.SetOptions(options);
	fLayout.Update();
}


DocView::~DocView()
{
	delete fAutoScroll;
	delete fOffscreen;
	delete fRenderer;
}


void
DocView::AttachedToWindow()
{
	BView::AttachedToWindow();
	SetViewColor(B_TRANSPARENT_COLOR);
	Window()->SetPulseRate(500000);
	fLayout.Update();
	_UpdateScrollBars();
}


void
DocView::SetScrollBars(BScrollBar* horizontal, BScrollBar* vertical)
{
	fHorizontal = horizontal;
	fVertical = vertical;
	_UpdateScrollBars();
}


void
DocView::_Notify(bool content)
{
	if (fListener != nullptr)
		fListener->DocViewChanged(this, content);
}


// #pragma mark - modes


void
DocView::SetMode(int mode)
{
	if (!fPaginate)
		mode = kModeNormal;
	if (mode == fMode)
		return;
	bool outline = mode == kModeOutline;
	if (outline != (fMode == kModeOutline)) {
		LayoutOptions options = fLayout.Options();
		options.outline = outline;
		fLayout.SetOptions(options);
		fLayout.Update();
	}
	fMode = mode;
	ScrollTo(BPoint(0, 0));
	_UpdateScrollBars();
	Invalidate();
	ScrollToCaret();
	_Notify(false);
	if (fListener != nullptr)
		fListener->DocViewScrolled(this);
}


void
DocView::SetOutlineOptions(int32 showLevel, bool firstLine, bool formatting)
{
	LayoutOptions options = fLayout.Options();
	options.outlineShowLevel = showLevel;
	options.outlineFirstLine = firstLine;
	options.outlineFormatting = formatting;
	fLayout.SetOptions(options);
	fLayout.Update();
	// keep the insertion point on something visible
	Pos caret = fEditor.Caret();
	const OutlineInfo* info = fLayout.Outline(caret.para);
	if (info != nullptr && !info->visible) {
		int32 para = caret.para;
		while (para > 0 && fLayout.Outline(para) != nullptr
			&& !fLayout.Outline(para)->visible)
			para--;
		fEditor.Select(Pos(para, 0), Pos(para, 0));
	}
	Changed(true);
}


std::vector<bool>
DocView::OutlineVisibility()
{
	std::vector<bool> visible;
	for (int32 p = 0; p < (int32)fStory->size(); p++) {
		const OutlineInfo* info = fLayout.Outline(p);
		visible.push_back(info == nullptr || info->visible);
	}
	return visible;
}


void
DocView::SelectOutlineFamily(int32 para)
{
	const Story& story = *fStory;
	int32 level = fDocument->styles.OutlineLevel(story[para].format.style);
	int32 end = para + 1;
	if (level > 0) {
		while (end < (int32)story.size()) {
			int32 next = fDocument->styles.OutlineLevel(
				story[end].format.style);
			if (next > 0 && next <= level)
				break;
			end++;
		}
	}
	Pos last = end < (int32)story.size() ? Pos(end, 0)
		: Pos(end - 1, story[end - 1].Length());
	Select(Pos(para, 0), last);
}


void
DocView::SetZoom(float zoom)
{
	zoom = std::max(0.25f, std::min(zoom, 4.0f));
	if (zoom == fZoom)
		return;
	fZoom = zoom;
	_UpdateScrollBars();
	Invalidate();
	ScrollToCaret();
	if (fListener != nullptr)
		fListener->DocViewScrolled(this);
}


void
DocView::SetShowAll(bool show)
{
	if (show == fShowAll)
		return;
	fShowAll = show;
	Relayout();
}


void
DocView::Relayout()
{
	LayoutOptions options = fLayout.Options();
	options.showHidden = gPrefs.showHidden || fShowAll;
	options.pageNumber = fFieldPage;
	fLayout.SetOptions(options);
	fLayout.Invalidate();
	fRenderer->Invalidate();
	Changed(false);
}


void
DocView::Changed(bool scroll)
{
	fLayout.Update();
	_UpdateScrollBars();
	Invalidate();
	fCaretOn = true;
	if (scroll)
		ScrollToCaret();
	_Notify(true);
}


void
DocView::Select(Pos anchor, Pos caret)
{
	fEditor.Select(anchor, caret);
	fLineEnd = false;
	fGoalValid = false;
	fCaretOn = true;
	Invalidate();
	ScrollToCaret();
	_Notify(false);
}


// #pragma mark - geometry


BRect
DocView::PageViewRect(int32 page)
{
	const DocSettings& settings = fDocument->settings;
	float width = settings.PaperWidth() * fZoom;
	float height = settings.PaperHeight() * fZoom;
	float left = PageViewLeft(page);
	float top = kPageGap + page * (height + kPageGap);
	return BRect(left, top, left + width, top + height);
}


float
DocView::PageViewLeft(int32 page)
{
	(void)page;
	float width = fDocument->settings.PaperWidth() * fZoom;
	return std::max(kPageGap, floorf((Bounds().Width() - width) / 2));
}


BPoint
DocView::_ToView(int32 page, float x, float y)
{
	if (fMode == kModePage) {
		BRect rect = PageViewRect(page);
		return BPoint(rect.left + x * fZoom, rect.top + y * fZoom);
	}
	return BPoint(kNormalLeft + x * fZoom, kNormalTop + y * fZoom);
}


int32
DocView::PageAtViewY(float y)
{
	float height = fDocument->settings.PaperHeight() * fZoom + kPageGap;
	int32 page = (int32)floorf((y - kPageGap / 2) / height);
	return std::max<int32>(0, std::min<int32>(page, fLayout.PageCount() - 1));
}


void
DocView::_FromView(BPoint where, int32* page, float* x, float* y)
{
	if (fMode == kModePage) {
		*page = PageAtViewY(where.y);
		BRect rect = PageViewRect(*page);
		*x = (where.x - rect.left) / fZoom;
		*y = (where.y - rect.top) / fZoom;
		return;
	}
	*page = 0;
	*x = (where.x - kNormalLeft) / fZoom;
	*y = (where.y - kNormalTop) / fZoom;
}


Pos
DocView::_HitTest(BPoint where, bool* lineEnd)
{
	int32 page;
	float x, y;
	_FromView(where, &page, &x, &y);
	return fLayout.HitTest(fMode == kModePage ? kGeomPage : kGeomNormal,
		page, x, y, lineEnd);
}


bool
DocView::_InSelectionBar(BPoint where)
{
	if (fMode == kModePage) {
		int32 page;
		float x, y;
		_FromView(where, &page, &x, &y);
		return x >= 0 && x < fLayout.PageTextLeft(page) - 4;
	}
	return where.x - Bounds().left < kNormalLeft - 3;
}


BSize
DocView::_DocumentSize()
{
	const DocSettings& settings = fDocument->settings;
	if (fMode == kModePage) {
		float width = settings.PaperWidth() * fZoom + 2 * kPageGap;
		float height = kPageGap + fLayout.PageCount()
			* (settings.PaperHeight() * fZoom + kPageGap);
		return BSize(width, height);
	}
	float width = kNormalLeft + (fLayout.LayoutWidth() + 48) * fZoom;
	float height = kNormalTop + fLayout.NormalHeight() * fZoom + 40;
	return BSize(width, height);
}


void
DocView::_UpdateScrollBars()
{
	if (Window() == nullptr)
		return;
	BRect bounds = Bounds();
	BSize size = _DocumentSize();
	if (fHorizontal != nullptr) {
		float range = std::max(0.0f, size.width - bounds.Width());
		fHorizontal->SetRange(0, range);
		fHorizontal->SetProportion(std::min(1.0f,
			bounds.Width() / std::max(1.0f, size.width)));
		fHorizontal->SetSteps(16, std::max(16.0f, bounds.Width() - 32));
	}
	if (fVertical != nullptr) {
		float range = std::max(0.0f, size.height - bounds.Height());
		fVertical->SetRange(0, range);
		fVertical->SetProportion(std::min(1.0f,
			bounds.Height() / std::max(1.0f, size.height)));
		fVertical->SetSteps(16 * fZoom, std::max(16.0f, bounds.Height() - 24));
	}
}


void
DocView::ScrollTo(BPoint where)
{
	BSize size = _DocumentSize();
	BRect bounds = Bounds();
	where.x = std::max(0.0f, std::min(where.x, size.width - bounds.Width()));
	where.y = std::max(0.0f, std::min(where.y, size.height - bounds.Height()));
	where.x = roundf(where.x);
	where.y = roundf(where.y);
	BView::ScrollTo(where);
	if (fListener != nullptr)
		fListener->DocViewScrolled(this);
}


BRect
DocView::_CaretRect()
{
	CaretInfo info;
	int geometry = fMode == kModePage ? kGeomPage : kGeomNormal;
	Pos caret = fEditor.Caret();
	if (!fLayout.Caret(caret, geometry, fLineEnd, &info))
		return BRect();
	const ParaLayout& layout = fLayout.Para(caret.para);
	const LineBox& line = layout.lines[fLayout.LineIndexOf(caret, fLineEnd)];
	float top = std::max(info.top, info.baseline - line.ascent);
	float bottom = std::min(info.bottom, info.baseline + line.descent);
	BPoint a = _ToView(info.page, info.x, top);
	BPoint b = _ToView(info.page, info.x, bottom);
	float width = fZoom >= 2 ? 2 : 1;
	float x = floorf(a.x);
	return BRect(x, floorf(a.y), x + width - 1, ceilf(b.y));
}


void
DocView::_InvalidateCaret()
{
	BRect rect = _CaretRect();
	if (rect.IsValid())
		Invalidate(rect.InsetByCopy(-2, -2));
}


void
DocView::ScrollToCaret()
{
	if (Window() == nullptr)
		return;
	BRect caret = _CaretRect();
	if (!caret.IsValid())
		return;
	BRect bounds = Bounds();
	BPoint target = bounds.LeftTop();
	float vertical = std::min(40.0f, bounds.Height() / 4);
	float horizontal = std::min(48.0f, bounds.Width() / 4);
	if (caret.top < bounds.top + 2)
		target.y = caret.top - vertical;
	else if (caret.bottom > bounds.bottom - 2)
		target.y = caret.bottom - bounds.Height() + vertical;
	if (caret.left < bounds.left + (fMode == kModePage ? 0 : kNormalLeft))
		target.x = caret.left - horizontal - (fMode == kModePage
			? 0 : kNormalLeft);
	else if (caret.right > bounds.right - 4)
		target.x = caret.right - bounds.Width() + horizontal;
	if (target != bounds.LeftTop())
		ScrollTo(target);
}


float
DocView::ColumnOriginX()
{
	Pos caret = fEditor.Caret();
	float left = Bounds().left;
	if (fLayout.ParaCount() == 0 || caret.para >= fLayout.ParaCount())
		return kNormalLeft - left;
	const PlacedLine& placed = fLayout.Placed(caret.para,
		fLayout.LineIndexOf(caret, fLineEnd));
	if (fMode == kModePage) {
		return PageViewRect(placed.page).left + placed.pageX * fZoom - left;
	}
	return kNormalLeft + placed.normalX * fZoom - left;
}


int32
DocView::CaretPage()
{
	return fLayout.PageOfPos(fEditor.Caret());
}


int32
DocView::TopPage()
{
	BRect bounds = Bounds();
	if (fMode == kModePage)
		return PageAtViewY(bounds.top + 20);
	float y = (bounds.top - kNormalTop) / fZoom;
	int32 block = fLayout.FindBlock(kGeomNormal, 0, y);
	if (block < 0)
		return 0;
	int32 para = fLayout.Blocks()[block].firstPara;
	return fLayout.Placed(para, 0).page;
}


void
DocView::GoToPage(int32 page)
{
	page = std::max<int32>(0, std::min<int32>(page, fLayout.PageCount() - 1));
	const PageInfo& info = fLayout.Pages()[page];
	if (info.firstLine < (int32)fLayout.Lines().size()) {
		const PlacedLine& line = fLayout.Lines()[info.firstLine];
		const ParaLayout& layout = fLayout.Para(line.para);
		Pos pos(line.para, layout.cpByte[layout.lines[line.line].startCp]);
		fEditor.Select(pos, pos);
		fLineEnd = false;
	}
	if (fMode == kModePage)
		ScrollTo(BPoint(Bounds().left, PageViewRect(page).top - kPageGap / 2));
	else
		ScrollToCaret();
	Invalidate();
	_Notify(false);
}


// #pragma mark - drawing


bool
DocView::_EnsureOffscreen()
{
	BRect bounds = Bounds();
	int32 width = (int32)ceilf(bounds.Width()) + 1;
	int32 height = (int32)ceilf(bounds.Height()) + 1;
	if (fOffscreen != nullptr) {
		BRect have = fOffscreen->Bounds();
		if (have.Width() + 1 >= width && have.Height() + 1 >= height)
			return true;
		delete fOffscreen;
		fOffscreen = nullptr;
		fOffscreenView = nullptr;
	}
	// grow in steps so that resizing does not reallocate all the time
	width = (width + 127) & ~127;
	height = (height + 127) & ~127;
	BRect rect(0, 0, width - 1, height - 1);
	fOffscreen = new BBitmap(rect, B_BITMAP_ACCEPTS_VIEWS, B_RGB32);
	if (fOffscreen->InitCheck() != B_OK) {
		delete fOffscreen;
		fOffscreen = nullptr;
		return false;
	}
	fOffscreenView = new BView(rect, "offscreen", B_FOLLOW_NONE, B_WILL_DRAW);
	fOffscreen->AddChild(fOffscreenView);
	return true;
}


void
DocView::Draw(BRect update)
{
	if (!_EnsureOffscreen()) {
		_Render(this, update);
		_DrawCaret(this);
		return;
	}
	BRect bounds = Bounds();
	fOffscreen->Lock();
	BView* view = fOffscreenView;
	view->PushState();
	view->SetOrigin(-bounds.left, -bounds.top);
	BRegion clip(update);
	view->ConstrainClippingRegion(&clip);
	_Render(view, update);
	view->PopState();
	view->Sync();
	fOffscreen->Unlock();
	DrawBitmap(fOffscreen, update.OffsetByCopy(-bounds.left, -bounds.top),
		update);
	_DrawCaret(this);
	if (fDropShown) {
		CaretInfo info;
		if (fLayout.Caret(fDropPos, fMode == kModePage ? kGeomPage
				: kGeomNormal, false, &info)) {
			BPoint a = _ToView(info.page, info.x, info.top);
			BPoint b = _ToView(info.page, info.x, info.bottom);
			SetHighColor(0, 0, 0);
			StrokeDotted(this, BPoint(floorf(a.x), a.y), BPoint(floorf(a.x),
				b.y), 2);
		}
	}
}


void
DocView::_Render(BView* view, BRect update)
{
	view->SetDrawingMode(B_OP_COPY);
	view->SetPenSize(1);
	if (fMode == kModePage)
		_RenderPages(view, update);
	else
		_RenderNormal(view, update);
	_RenderSelection(view, update);
}


void
DocView::_PaintRows(PaintContext& context, BView* view, BRect update,
	int32 page)
{
	const Story& story = *fStory;
	for (const Block& block : fLayout.Blocks()) {
		if (!block.row || block.lastPara >= (int32)story.size())
			continue;
		const RowFormat& row = story[block.lastPara].row;
		float left, top, height;
		if (fMode == kModePage) {
			if (block.page != page)
				continue;
			BRect rect = PageViewRect(page);
			left = rect.left + block.pageX * fZoom;
			top = rect.top + block.pageTop * fZoom;
			height = (block.pageBottom - block.pageTop) * fZoom;
		} else {
			left = kNormalLeft;
			top = kNormalTop + block.normalTop * fZoom;
			height = (block.normalBottom - block.normalTop) * fZoom;
		}
		if (top > update.bottom || top + height < update.top)
			continue;
		PaintRowGrid(context, row, left, top, height, fLayout.LayoutWidth(),
			gPrefs.tableGridlines);
	}
	(void)view;
}


void
DocView::_RenderNormal(BView* view, BRect update)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(update);

	PaintContext context;
	context.view = view;
	context.document = fDocument;
	context.zoom = fZoom;
	context.showAll = fShowAll;
	context.pictures = fPictures;

	const Story& story = *fStory;
	const std::vector<Block>& blocks = fLayout.Blocks();
	if (blocks.empty())
		return;
	_PaintRows(context, view, update, 0);

	float docTop = (update.top - kNormalTop) / fZoom - 24;
	float docBottom = (update.bottom - kNormalTop) / fZoom + 24;
	int32 first = std::max<int32>(0, fLayout.FindBlock(kGeomNormal, 0, docTop));
	for (int32 b = first; b < (int32)blocks.size(); b++) {
		const Block& block = blocks[b];
		if (block.normalTop > docBottom)
			break;
		for (int32 p = block.firstPara; p <= block.lastPara; p++) {
			if (story[p].kind == kParaRowEnd && !fShowAll)
				continue;
			const ParaLayout& layout = fLayout.Para(p);
			for (int32 l = 0; l < (int32)layout.lines.size(); l++) {
				if (layout.lines[l].height <= 0)
					continue;
				const PlacedLine& placed = fLayout.Placed(p, l);
				float top = kNormalTop + placed.normalTop * fZoom;
				float height = layout.lines[l].height * fZoom;
				if (top - 24 > update.bottom || top + height + 24 < update.top)
					continue;
				PaintLine(context, story[p], layout, l,
					BPoint(kNormalLeft + placed.normalX * fZoom, top));
			}
		}
	}

	// Word's end mark: a short bar below the last line of the document
	if (fPaginate && !fLayout.Lines().empty()) {
		const PlacedLine& last = fLayout.Lines().back();
		const ParaLayout& layout = fLayout.Para(last.para);
		float y = floorf(kNormalTop + (last.normalTop
			+ layout.lines[last.line].height) * fZoom) + 3;
		if (y + 3 >= update.top && y <= update.bottom) {
			float x = floorf(kNormalLeft + std::max(0.0f,
				layout.textLeft) * fZoom);
			view->SetHighColor(0, 0, 0);
			view->FillRect(BRect(x, y, x + std::max(8.0f, 10 * fZoom) - 1,
				y + std::max(1.0f, roundf(2 * fZoom)) - 1));
		}
	}

	if (fMode == kModeOutline) {
		_DrawOutlineSymbols(view, update);
		return;
	}

	// Page breaks: sparse dots for automatic breaks, dense for manual
	// ones, a double line for section breaks.
	const std::vector<PageInfo>& pages = fLayout.Pages();
	const std::vector<PlacedLine>& lines = fLayout.Lines();
	BRect bounds = Bounds();
	for (int32 p = 1; p < (int32)pages.size(); p++) {
		int32 firstLine = pages[p].firstLine;
		if (firstLine <= 0 || firstLine >= (int32)lines.size())
			continue;
		const PlacedLine& previous = lines[firstLine - 1];
		const LineBox& box = fLayout.Para(previous.para).lines[previous.line];
		float y = floorf(kNormalTop + (previous.normalTop + box.height)
			* fZoom) + 1;
		if (y < update.top - 4 || y > update.bottom + 4)
			continue;
		view->SetHighColor(0, 0, 0);
		BPoint from(bounds.left, y);
		BPoint to(bounds.right, y);
		if (box.sectionBreak) {
			StrokeDotted(view, from, to, 2);
			StrokeDotted(view, from + BPoint(0, 2), to + BPoint(0, 2), 2);
		} else if (box.pageBreak)
			StrokeDotted(view, from, to, 2);
		else
			StrokeDotted(view, from, to, 5);
	}
}


void
DocView::_DrawOutlineSymbols(BView* view, BRect update)
{
	const Story& story = *fStory;
	view->SetHighColor(0, 0, 0);
	for (int32 p = 0; p < (int32)story.size(); p++) {
		const OutlineInfo* info = fLayout.Outline(p);
		if (info == nullptr || !info->visible || story[p].IsTablePart())
			continue;
		const ParaLayout& layout = fLayout.Para(p);
		const PlacedLine& placed = fLayout.Placed(p, 0);
		const LineBox& line = layout.lines[0];
		float top = kNormalTop + placed.normalTop * fZoom;
		if (top > update.bottom || top + line.height * fZoom < update.top)
			continue;
		float size = std::max(5.0f, roundf(7 * fZoom));
		float x = floorf(kNormalLeft + (info->indent * 36 + 5) * fZoom);
		float y = floorf(top + (line.baseline - line.ascent * 0.35f) * fZoom
			- size / 2);
		BRect box(x, y, x + size - 1, y + size - 1);
		float middleY = floorf(box.top + size / 2);
		float middleX = floorf(box.left + size / 2);
		if (info->level == 0) {
			// body text: a small square
			BRect square = box.InsetByCopy(size / 4, size / 4);
			view->StrokeRect(square);
		} else if (info->hasChildren) {
			view->FillRect(BRect(box.left, middleY - 1, box.right, middleY));
			view->FillRect(BRect(middleX - 1, box.top, middleX, box.bottom));
		} else
			view->FillRect(BRect(box.left, middleY - 1, box.right, middleY));
		// a collapsed heading is underlined with a gray line
		if (story[p].collapsed && info->hasChildren) {
			view->SetHighColor(150, 150, 150);
			float underline = floorf(top + (line.baseline + 2) * fZoom);
			view->StrokeLine(BPoint(kNormalLeft + line.left * fZoom,
				underline), BPoint(kNormalLeft + line.endX * fZoom, underline));
			view->SetHighColor(0, 0, 0);
		}
	}
}


int32
DocView::_OutlineSymbolAt(BPoint where)
{
	if (fMode != kModeOutline)
		return -1;
	bool lineEnd;
	Pos hit = _HitTest(where, &lineEnd);
	const OutlineInfo* info = fLayout.Outline(hit.para);
	if (info == nullptr || !info->visible)
		return -1;
	float x = kNormalLeft + (info->indent * 36 + 2) * fZoom;
	if (where.x >= x && where.x <= x + 14 * fZoom
		&& fLayout.LineIndexOf(hit, lineEnd) == 0)
		return hit.para;
	return -1;
}


void
DocView::_RenderPages(BView* view, BRect update)
{
	view->SetHighColor(152, 152, 152);
	view->FillRect(update);
	int32 count = fLayout.PageCount();
	int32 first = PageAtViewY(update.top);
	for (int32 page = first; page < count; page++) {
		BRect rect = PageViewRect(page);
		if (rect.top > update.bottom)
			break;
		if (!rect.InsetByCopy(-4, -4).Intersects(update))
			continue;
		PageRenderer::Options options;
		options.zoom = fZoom;
		options.showAll = fShowAll;
		options.boundaries = gPrefs.textBoundaries;
		fRenderer->RenderPage(view, fLayout, page, rect.LeftTop(), update,
			options);
	}
}


void
DocView::_RenderSelection(BView* view, BRect update)
{
	if (!fEditor.HasSelection())
		return;
	std::vector<SelectionRect> rects;
	int geometry = fMode == kModePage ? kGeomPage : kGeomNormal;
	fLayout.SelectionRects(fEditor.SelStart(), fEditor.SelEnd(), geometry, 6,
		rects);
	view->SetDrawingMode(B_OP_INVERT);
	for (const SelectionRect& rect : rects) {
		BPoint a = _ToView(rect.page, rect.left, rect.top);
		BPoint b = _ToView(rect.page, rect.right, rect.bottom);
		BRect frame(floorf(a.x), floorf(a.y), ceilf(b.x) - 1, ceilf(b.y) - 1);
		frame = frame & update;
		if (frame.IsValid())
			view->FillRect(frame);
	}
	view->SetDrawingMode(B_OP_COPY);
}


void
DocView::_DrawCaret(BView* view)
{
	if (!fCaretOn || !fActive || !IsFocus() || fEditor.HasSelection())
		return;
	BRect rect = _CaretRect();
	if (!rect.IsValid())
		return;
	view->SetDrawingMode(B_OP_INVERT);
	view->FillRect(rect);
	view->SetDrawingMode(B_OP_COPY);
}


void
DocView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	_UpdateScrollBars();
	Invalidate();
	if (fListener != nullptr)
		fListener->DocViewScrolled(this);
}


void
DocView::Pulse()
{
	if (!fActive || !IsFocus() || fEditor.HasSelection())
		return;
	fCaretOn = !fCaretOn;
	_InvalidateCaret();
}


void
DocView::WindowActivated(bool active)
{
	fActive = active;
	fCaretOn = true;
	_InvalidateCaret();
}


void
DocView::MakeFocus(bool focus)
{
	BView::MakeFocus(focus);
	fCaretOn = true;
	_InvalidateCaret();
	if (focus)
		_Notify(false);
}


// #pragma mark - mouse


void
DocView::_LineRange(Pos pos, bool lineEnd, Pos* start, Pos* end)
{
	const ParaLayout& layout = fLayout.Para(pos.para);
	const LineBox& line = layout.lines[fLayout.LineIndexOf(pos, lineEnd)];
	*start = Pos(pos.para, layout.cpByte[line.startCp]);
	if (line.last) {
		if (pos.para + 1 < (int32)fStory->size())
			*end = Pos(pos.para + 1, 0);
		else
			*end = Pos(pos.para, (*fStory)[pos.para].Length());
	} else
		*end = Pos(pos.para, layout.cpByte[line.endCp]);
}


void
DocView::_UnitRange(Pos pos, bool lineEnd, Pos* start, Pos* end)
{
	switch (fUnit) {
		case kUnitWord:
			fEditor.WordAt(pos, start, end);
			break;
		case kUnitSentence:
			fEditor.SentenceAt(pos, start, end);
			break;
		case kUnitParagraph:
			fEditor.ParagraphAt(pos, start, end);
			break;
		case kUnitLine:
			_LineRange(pos, lineEnd, start, end);
			break;
		case kUnitDocument:
			*start = Pos(0, 0);
			*end = fEditor.End();
			break;
		default:
			*start = *end = pos;
			break;
	}
}


void
DocView::MouseDown(BPoint where)
{
	if (!IsFocus())
		MakeFocus(true);
	BMessage* message = Window()->CurrentMessage();
	int32 clicks = 1;
	int32 buttons = B_PRIMARY_MOUSE_BUTTON;
	if (message != nullptr) {
		message->FindInt32("clicks", &clicks);
		message->FindInt32("buttons", &buttons);
	}
	uint32 mods = modifiers();
	int32 messageModifiers;
	if (message != nullptr
		&& message->FindInt32("modifiers", &messageModifiers) == B_OK)
		mods = (uint32)messageModifiers;
	if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
		if (fListener != nullptr)
			fListener->DocViewContextMenu(this, where);
		return;
	}
	bool lineEnd = false;
	Pos hit = _HitTest(where, &lineEnd);
	bool extend = (mods & B_SHIFT_KEY) != 0;
	bool command = (mods & B_COMMAND_KEY) != 0;

	int32 symbol = _OutlineSymbolAt(where);
	if (symbol >= 0 && !extend) {
		if (clicks >= 2) {
			Paragraph& paragraph = (*fStory)[symbol];
			paragraph.collapsed = !paragraph.collapsed;
			fLayout.Update();
			Select(Pos(symbol, 0), Pos(symbol, 0));
			Changed(false);
		} else
			SelectOutlineFamily(symbol);
		return;
	}

	if (_InSelectionBar(where)) {
		if (command)
			fUnit = kUnitDocument;
		else
			fUnit = clicks >= 2 ? kUnitParagraph : kUnitLine;
	} else if (command)
		fUnit = kUnitSentence;
	else if (clicks >= 3)
		fUnit = kUnitParagraph;
	else if (clicks == 2)
		fUnit = kUnitWord;
	else
		fUnit = kUnitChar;

	// a click in the selection may start dragging it
	if (!extend && !command && clicks == 1 && fEditor.HasSelection()
		&& !_InSelectionBar(where) && hit >= fEditor.SelStart()
		&& hit < fEditor.SelEnd()) {
		fDragPending = true;
		fDragStart = where;
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		return;
	}

	Pos start, end;
	_UnitRange(hit, lineEnd, &start, &end);
	if (extend) {
		Pos anchor = fEditor.Anchor();
		fUnitAnchorStart = fUnitAnchorEnd = anchor;
		if (hit < anchor)
			fEditor.Select(anchor, start);
		else
			fEditor.Select(anchor, fUnit == kUnitChar ? hit : end);
	} else {
		fUnitAnchorStart = start;
		fUnitAnchorEnd = end;
		if (fUnit == kUnitChar)
			fEditor.Select(hit, hit);
		else
			fEditor.Select(start, end);
	}
	fLineEnd = fUnit == kUnitChar && !fEditor.HasSelection() ? lineEnd : false;
	fGoalValid = false;
	fCaretOn = true;
	fTracking = true;
	fLastMouse = where;
	SetMouseEventMask(B_POINTER_EVENTS,
		B_LOCK_WINDOW_FOCUS | B_NO_POINTER_HISTORY);
	delete fAutoScroll;
	BMessage scroll(kMsgAutoScroll);
	fAutoScroll = new BMessageRunner(BMessenger(this), &scroll, 50000);
	Invalidate();
	_Notify(false);
}


void
DocView::_ExtendTracking(BPoint where)
{
	bool lineEnd = false;
	Pos hit = _HitTest(where, &lineEnd);
	Pos start, end;
	_UnitRange(hit, lineEnd, &start, &end);
	Pos oldAnchor = fEditor.Anchor();
	Pos oldCaret = fEditor.Caret();
	if (fUnit == kUnitChar) {
		fEditor.Select(fUnitAnchorStart, hit);
		fLineEnd = lineEnd;
	} else if (start < fUnitAnchorStart)
		fEditor.Select(fUnitAnchorEnd, start);
	else
		fEditor.Select(fUnitAnchorStart, std::max(end, fUnitAnchorEnd));
	if (fEditor.Anchor() != oldAnchor || fEditor.Caret() != oldCaret) {
		fCaretOn = true;
		Invalidate();
		_Notify(false);
	}
}


void
DocView::_SetCursorFor(BPoint where)
{
	int kind = kCursorBeam;
	if (_InSelectionBar(where))
		kind = kCursorBar;
	else if (fMode == kModePage) {
		int32 page = PageAtViewY(where.y);
		if (!PageViewRect(page).Contains(where))
			kind = kCursorDefault;
	}
	if (kind == fCursorKind)
		return;
	fCursorKind = kind;
	if (kind == kCursorBar)
		SetViewCursor(SelectionBarCursor());
	else if (kind == kCursorBeam) {
		BCursor beam(B_CURSOR_ID_I_BEAM);
		SetViewCursor(&beam);
	} else
		SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
}


void
DocView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	if (fDragPending) {
		if (fabs(where.x - fDragStart.x) > 4 || fabs(where.y - fDragStart.y) > 4) {
			fDragPending = false;
			_StartDrag();
		}
		return;
	}
	if (drag != nullptr && (drag->HasData("text/plain", B_MIME_TYPE)
			|| drag->HasRef("refs"))) {
		_ShowDropCaret(where, transit != B_EXITED_VIEW
			&& transit != B_OUTSIDE_VIEW);
		return;
	}
	if (fDropShown)
		_ShowDropCaret(where, false);
	if (fTracking) {
		fLastMouse = where;
		_ExtendTracking(where);
		return;
	}
	if (transit == B_EXITED_VIEW) {
		fCursorKind = -1;
		return;
	}
	_SetCursorFor(where);
}


void
DocView::MouseUp(BPoint where)
{
	if (fDragPending) {
		// a click in the selection without dragging places the caret
		fDragPending = false;
		bool lineEnd = false;
		Pos hit = _HitTest(where, &lineEnd);
		fEditor.Select(hit, hit);
		fLineEnd = lineEnd;
		fCaretOn = true;
		Invalidate();
		_Notify(false);
		return;
	}
	if (!fTracking)
		return;
	fTracking = false;
	delete fAutoScroll;
	fAutoScroll = nullptr;
	_Notify(false);
}


// #pragma mark - keyboard


void
DocView::_MoveCaret(Pos pos, bool extend, bool lineEnd, bool keepGoal)
{
	Pos anchor = extend ? fEditor.Anchor() : pos;
	fEditor.Select(anchor, pos);
	fLineEnd = lineEnd;
	if (!keepGoal)
		fGoalValid = false;
	fCaretOn = true;
	Invalidate();
	ScrollToCaret();
	_Notify(false);
}


std::string
DocView::_SmartQuote(uint32 c)
{
	Pos start = fEditor.SelStart();
	const std::string& text = (*fStory)[start.para].text;
	bool open = true;
	if (start.offset > 0) {
		uint32 previous = CharAt(text, PrevOffset(text, start.offset));
		open = IsSpace(previous) || previous == '\t' || previous == '('
			|| previous == '[' || previous == '{' || previous == 0x2014
			|| previous == 0x2013 || previous == kNbspChar
			|| previous == 0x201C || previous == 0x2018
			|| previous == kLineBreakChar;
	}
	if (c == '"')
		return open ? "\xE2\x80\x9C" : "\xE2\x80\x9D";
	return open ? "\xE2\x80\x98" : "\xE2\x80\x99";
}


void
DocView::TypeText(const std::string& text)
{
	fEditor.InsertText(text);
	fLineEnd = false;
	fGoalValid = false;
	Changed();
}


void
DocView::_NextCell(bool backward)
{
	const Story& story = *fStory;
	int32 para = fEditor.Caret().para;
	int32 target = -1;
	if (backward) {
		for (int32 p = para - 1; p >= 0; p--) {
			if (story[p].kind == kParaCell) {
				target = p;
				break;
			}
			if (story[p].kind == kParaNormal)
				break;
		}
	} else {
		for (int32 p = para + 1; p < (int32)story.size(); p++) {
			if (story[p].kind == kParaCell) {
				target = p;
				break;
			}
			if (story[p].kind == kParaNormal)
				break;
		}
	}
	if (target < 0)
		return;
	Select(Pos(target, 0), Pos(target, story[target].Length()));
}


void
DocView::KeyDown(const char* bytes, int32 numBytes)
{
	BMessage* message = Window()->CurrentMessage();
	uint32 mods = modifiers();
	int32 key = 0;
	if (message != nullptr) {
		message->FindInt32("key", &key);
		int32 messageModifiers;
		if (message->FindInt32("modifiers", &messageModifiers) == B_OK)
			mods = (uint32)messageModifiers;
	}
	bool shift = (mods & B_SHIFT_KEY) != 0;
	bool word = (mods & (B_CONTROL_KEY | B_OPTION_KEY)) != 0;
	bool command = (mods & B_COMMAND_KEY) != 0;
	int geometry = fMode == kModePage ? kGeomPage : kGeomNormal;
	Pos caret = fEditor.Caret();

	switch ((unsigned char)bytes[0]) {
		case B_LEFT_ARROW: {
			Pos target;
			if (!shift && !word && fEditor.HasSelection())
				target = fEditor.SelStart();
			else
				target = word ? fEditor.PrevWord(caret)
					: fEditor.PrevChar(caret);
			_MoveCaret(target, shift);
			return;
		}
		case B_RIGHT_ARROW: {
			Pos target;
			if (!shift && !word && fEditor.HasSelection())
				target = fEditor.SelEnd();
			else
				target = word ? fEditor.NextWord(caret)
					: fEditor.NextChar(caret);
			_MoveCaret(target, shift);
			return;
		}
		case B_UP_ARROW:
		case B_DOWN_ARROW: {
			bool down = (unsigned char)bytes[0] == B_DOWN_ARROW;
			if (word) {
				Pos target = down ? fEditor.NextParagraph(caret)
					: fEditor.PrevParagraph(caret);
				_MoveCaret(target, shift);
				return;
			}
			CaretInfo info;
			if (!fGoalValid && fLayout.Caret(caret, geometry, fLineEnd,
					&info)) {
				fGoalX = info.x;
				fGoalValid = true;
			}
			bool lineEnd = false;
			Pos target = fLayout.MoveVertical(caret, fLineEnd, geometry,
				fGoalX, down ? 1 : -1, &lineEnd);
			_MoveCaret(target, shift, lineEnd, true);
			return;
		}
		case B_HOME: {
			Pos target = word || command ? Pos(0, 0)
				: fLayout.LineStart(caret, fLineEnd);
			_MoveCaret(target, shift);
			return;
		}
		case B_END: {
			if (word || command) {
				_MoveCaret(fEditor.End(), shift);
				return;
			}
			bool lineEnd = false;
			Pos target = fLayout.LineEnd(caret, fLineEnd, &lineEnd);
			_MoveCaret(target, shift, lineEnd);
			return;
		}
		case B_PAGE_UP:
		case B_PAGE_DOWN: {
			bool down = (unsigned char)bytes[0] == B_PAGE_DOWN;
			float height = Bounds().Height() - 24;
			int lines = std::max(1, (int)(height / (15 * fZoom)));
			CaretInfo info;
			if (!fGoalValid && fLayout.Caret(caret, geometry, fLineEnd,
					&info)) {
				fGoalX = info.x;
				fGoalValid = true;
			}
			bool lineEnd = false;
			Pos target = fLayout.MoveVertical(caret, fLineEnd, geometry,
				fGoalX, down ? lines : -lines, &lineEnd);
			ScrollBy(0, down ? height : -height);
			_MoveCaret(target, shift, lineEnd, true);
			return;
		}
		case B_BACKSPACE:
			fEditor.Backspace(word);
			fLineEnd = false;
			Changed();
			return;
		case B_DELETE:
			fEditor.DeleteForward(word);
			fLineEnd = false;
			Changed();
			return;
		case B_ENTER:
			if (command) {
				fEditor.InsertSpecial(kSectionBreakChar, nullptr,
					"Section Break");
				fEditor.InsertParagraphBreak(true);
			} else if (shift && key == 0x5b) {
				fEditor.InsertSpecial(kPageBreakChar, nullptr, "Page Break");
				fEditor.InsertParagraphBreak(true);
			} else if (shift)
				fEditor.InsertSpecial(kLineBreakChar);
			else
				fEditor.InsertParagraphBreak();
			fLineEnd = false;
			Changed();
			return;
		case B_TAB:
			if ((*fStory)[caret.para].IsTablePart()) {
				_NextCell(shift);
				return;
			}
			TypeText("\t");
			return;
		case B_ESCAPE:
		case B_INSERT:
		case B_FUNCTION_KEY:
			return;
		default:
			break;
	}
	if (command || (unsigned char)bytes[0] < 0x20) {
		BView::KeyDown(bytes, numBytes);
		return;
	}
	std::string text(bytes, numBytes);
	if (gPrefs.smartQuotes && numBytes == 1
		&& (bytes[0] == '"' || bytes[0] == '\''))
		text = _SmartQuote((unsigned char)bytes[0]);
	TypeText(text);
}


// #pragma mark - clipboard


void
DocView::CopyToClipboard(const Story& fragment)
{
	std::string text = StoryToText(fragment);
	std::string rtf = WriteRTFFragment(*fDocument, fragment);
	if (!be_clipboard->Lock())
		return;
	be_clipboard->Clear();
	BMessage* data = be_clipboard->Data();
	data->AddData("text/plain", B_MIME_TYPE, text.c_str(), text.size());
	data->AddData("text/rtf", B_MIME_TYPE, rtf.c_str(), rtf.size());
	be_clipboard->Commit();
	be_clipboard->Unlock();
}


bool
DocView::StoryFromClipboard(Story& fragment)
{
	std::string rtf;
	std::string text;
	if (!be_clipboard->Lock())
		return false;
	BMessage* data = be_clipboard->Data();
	const void* bytes;
	ssize_t size;
	if (data != nullptr) {
		if (data->FindData("text/rtf", B_MIME_TYPE, &bytes, &size) == B_OK)
			rtf.assign((const char*)bytes, size);
		if (data->FindData("text/plain", B_MIME_TYPE, &bytes, &size) == B_OK)
			text.assign((const char*)bytes, size);
	}
	be_clipboard->Unlock();

	if (!rtf.empty()) {
		Document scratch;
		Story read;
		if (ReadRTFFragment(rtf, scratch, read)) {
			fragment = ImportStory(*fDocument, scratch, read);
			return true;
		}
	}
	if (text.empty())
		return false;
	text = TextToUtf8(text, nullptr);
	std::string clean;
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] == '\r') {
			clean += '\n';
			if (i + 1 < text.size() && text[i + 1] == '\n')
				i++;
		} else
			clean += text[i];
	}
	ParaFormat format = (*fStory)[fEditor.SelStart().para].format;
	fragment = FragmentFromText(clean, fEditor.InsertionFormat(), format);
	return true;
}


void
DocView::Copy()
{
	if (!fEditor.HasSelection())
		return;
	CopyToClipboard(fEditor.CopySelection());
}


void
DocView::Cut()
{
	if (!fEditor.HasSelection())
		return;
	Copy();
	fEditor.DeleteSelection("Cut");
	Changed();
}


void
DocView::Paste()
{
	Story fragment;
	if (!StoryFromClipboard(fragment))
		return;
	fEditor.ReplaceSelection(fragment, "Paste");
	fLineEnd = false;
	Changed();
}


void
DocView::Clear()
{
	if (!fEditor.HasSelection())
		return;
	fEditor.DeleteSelection("Clear");
	Changed();
}


// #pragma mark - drag and drop


void
DocView::_ShowDropCaret(BPoint where, bool show)
{
	bool lineEnd = false;
	Pos pos = show ? _HitTest(where, &lineEnd) : fDropPos;
	if (show == fDropShown && pos == fDropPos)
		return;
	fDropShown = show;
	fDropPos = pos;
	Invalidate();
}


void
DocView::_StartDrag()
{
	Story fragment = fEditor.CopySelection();
	std::string text = StoryToText(fragment);
	std::string rtf = WriteRTFFragment(*fDocument, fragment);
	BMessage drag(B_MIME_DATA);
	drag.AddData("text/plain", B_MIME_TYPE, text.c_str(), text.size());
	drag.AddData("text/rtf", B_MIME_TYPE, rtf.c_str(), rtf.size());
	drag.AddPointer("quill:view", this);

	std::vector<SelectionRect> rects;
	fLayout.SelectionRects(fEditor.SelStart(), fEditor.SelEnd(),
		fMode == kModePage ? kGeomPage : kGeomNormal, 6, rects);
	BRect frame;
	for (const SelectionRect& rect : rects) {
		BPoint a = _ToView(rect.page, rect.left, rect.top);
		BPoint b = _ToView(rect.page, rect.right, rect.bottom);
		frame = frame.IsValid() ? frame | BRect(a, b) : BRect(a, b);
	}
	frame = frame & Bounds();
	if (!frame.IsValid() || frame.Width() * frame.Height() > 200000)
		frame.Set(fDragStart.x - 20, fDragStart.y - 8, fDragStart.x + 20,
			fDragStart.y + 8);
	DragMessage(&drag, frame, this);
}


static Pos
AfterDeletion(Pos pos, Pos start, Pos end)
{
	if (pos <= start)
		return pos;
	if (pos < end)
		return start;
	if (pos.para == end.para)
		return Pos(start.para, start.offset + (pos.offset - end.offset));
	return Pos(pos.para - (end.para - start.para), pos.offset);
}


void
DocView::_Drop(BMessage* message)
{
	BPoint where = ConvertFromScreen(message->DropPoint());
	_ShowDropCaret(where, false);
	bool lineEnd = false;
	Pos target = _HitTest(where, &lineEnd);

	entry_ref ref;
	if (message->FindRef("refs", &ref) == B_OK) {
		// pictures are inserted, documents opened
		BNode node(&ref);
		BNodeInfo info(&node);
		char type[B_MIME_TYPE_LENGTH] = "";
		info.GetType(type);
		if (strncmp(type, "image/", 6) == 0) {
			fEditor.Select(target, target);
			BMessage insert('Iprf');
			insert.AddBool("picture", true);
			for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
				insert.AddRef("refs", &ref);
			Window()->PostMessage(&insert);
		} else {
			BMessage open(B_REFS_RECEIVED);
			for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
				open.AddRef("refs", &ref);
			be_app->PostMessage(&open);
		}
		return;
	}

	void* source = nullptr;
	message->FindPointer("quill:view", &source);
	uint32 mods = modifiers();
	bool copy = (mods & (B_CONTROL_KEY | B_OPTION_KEY)) != 0;

	if (source == this && !copy) {
		Pos start = fEditor.SelStart();
		Pos end = fEditor.SelEnd();
		if (target >= start && target <= end)
			return;
		Story fragment = fEditor.CopySelection();
		Story empty;
		empty.push_back(Paragraph(fEditor.InsertionFormat()));
		fEditor.BeginBulk("Move");
		fEditor.ReplaceRange(start, end, empty, "Move");
		Pos adjusted = AfterDeletion(target, start, end);
		Pos insertEnd = fEditor.ReplaceRange(adjusted, adjusted, fragment,
			"Move");
		fEditor.Select(adjusted, insertEnd);
		fEditor.EndBulk();
		Changed();
		return;
	}

	Story fragment;
	const void* bytes;
	ssize_t size;
	if (message->FindData("text/rtf", B_MIME_TYPE, &bytes, &size) == B_OK) {
		Document scratch;
		Story read;
		if (ReadRTFFragment(std::string((const char*)bytes, size), scratch,
				read))
			fragment = ImportStory(*fDocument, scratch, read);
	}
	if (fragment.empty() && message->FindData("text/plain", B_MIME_TYPE,
			&bytes, &size) == B_OK) {
		std::string text = TextToUtf8(std::string((const char*)bytes, size),
			nullptr);
		ParaFormat format = (*fStory)[target.para].format;
		fEditor.Select(target, target);
		fragment = FragmentFromText(text, fEditor.InsertionFormat(), format);
	}
	if (fragment.empty())
		return;
	Pos insertEnd = fEditor.ReplaceRange(target, target, fragment, "Drag");
	fEditor.Select(target, insertEnd);
	Changed();
}


// #pragma mark - messages


void
DocView::MessageReceived(BMessage* message)
{
	if (message->WasDropped()) {
		_Drop(message);
		return;
	}
	switch (message->what) {
		case kMsgAutoScroll: {
			if (!fTracking)
				break;
			BRect bounds = Bounds();
			float dx = 0;
			float dy = 0;
			if (fLastMouse.y < bounds.top)
				dy = std::max(-60.0f, fLastMouse.y - bounds.top);
			else if (fLastMouse.y > bounds.bottom)
				dy = std::min(60.0f, fLastMouse.y - bounds.bottom);
			if (fLastMouse.x < bounds.left)
				dx = std::max(-60.0f, fLastMouse.x - bounds.left);
			else if (fLastMouse.x > bounds.right)
				dx = std::min(60.0f, fLastMouse.x - bounds.right);
			if (dx != 0 || dy != 0) {
				ScrollBy(dx, dy);
				fLastMouse += BPoint(dx, dy);
				_ExtendTracking(fLastMouse);
			}
			break;
		}
		case B_CUT:
			Cut();
			break;
		case B_COPY:
			Copy();
			break;
		case B_PASTE:
			Paste();
			break;
		case B_SELECT_ALL:
			Select(Pos(0, 0), fEditor.End());
			break;
		default:
			BView::MessageReceived(message);
			break;
	}
}
