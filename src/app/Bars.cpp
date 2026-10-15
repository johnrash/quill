/*
 * Quill - Toolbar, Ribbon and Ruler.
 */
#include "Bars.h"

#include "Constants.h"
#include "Controls.h"
#include "Fonts.h"
#include "Icons.h"

#include <LayoutBuilder.h>
#include <Window.h>

#include <algorithm>
#include <cmath>

using namespace quill;


static BMessage*
Message(uint32 what)
{
	return new BMessage(what);
}


static BMessage*
Message(uint32 what, const char* name, int32 value)
{
	BMessage* message = new BMessage(what);
	message->AddInt32(name, value);
	return message;
}


static BMessage*
FloatMessage(uint32 what, const char* name, float value)
{
	BMessage* message = new BMessage(what);
	message->AddFloat(name, value);
	return message;
}


static void
DrawBarEdges(BView* view)
{
	BRect rect = view->Bounds();
	view->SetHighColor(tint_color(view->ViewColor(), B_LIGHTEN_2_TINT));
	view->StrokeLine(rect.LeftTop(), rect.RightTop());
	view->SetHighColor(tint_color(view->ViewColor(), B_DARKEN_3_TINT));
	view->StrokeLine(rect.LeftBottom(), rect.RightBottom());
}


// #pragma mark - Toolbar


Toolbar::Toolbar()
	:
	BGroupView("toolbar", B_HORIZONTAL, 2)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetFlags(Flags() | B_WILL_DRAW);
	GroupLayout()->SetInsets(4, 3, 4, 3);

	fUndo = new IconButton("undo", kIconUndo, Message(kMsgUndo), "Undo");
	fCut = new IconButton("cut", kIconCut, Message(B_CUT), "Cut");
	fCopy = new IconButton("copy", kIconCopy, Message(B_COPY), "Copy");
	fShowAll = new IconButton("showAll", kIconShowAll, Message(kMsgShowAll),
		"Show \xC2\xB6", true);
	fNormal = new IconButton("normal", kIconNormalView,
		Message(kMsgViewNormal), "Normal view", true);
	fPage = new IconButton("page", kIconPageView, Message(kMsgViewPage),
		"Page Layout view", true);

	BLayoutBuilder::Group<>(this)
		.Add(new IconButton("new", kIconNew, Message(kMsgNew), "New"))
		.Add(new IconButton("open", kIconOpen, Message(kMsgOpen), "Open"))
		.Add(new IconButton("save", kIconSave, Message(kMsgSave), "Save"))
		.Add(new IconButton("print", kIconPrint, Message(kMsgPrint), "Print"))
		.Add(new SeparatorBar())
		.Add(fCut)
		.Add(fCopy)
		.Add(new IconButton("paste", kIconPaste, Message(B_PASTE), "Paste"))
		.Add(fUndo)
		.Add(new SeparatorBar())
		.Add(new IconButton("bullets", kIconBullets, Message(kMsgBullets),
			"Bulleted list"))
		.Add(new IconButton("numbers", kIconNumbers, Message(kMsgNumbers),
			"Numbered list"))
		.Add(new IconButton("unindent", kIconUnindent,
			Message(kMsgNest, "in", 0), "Unindent"))
		.Add(new IconButton("indent", kIconIndent, Message(kMsgNest, "in", 1),
			"Indent"))
		.Add(new SeparatorBar())
		.Add(new IconButton("table", kIconTable, Message(kMsgInsertTable),
			"Insert table"))
		.Add(new IconButton("columns", kIconColumns, Message(kMsgSection),
			"Columns"))
		.Add(new IconButton("picture", kIconPicture, Message(kMsgInsertPicture),
			"Insert picture"))
		.Add(new IconButton("footnote", kIconFootnote,
			Message(kMsgInsertFootnote), "Insert footnote"))
		.Add(new SeparatorBar())
		.Add(new IconButton("spelling", kIconSpelling, Message(kMsgSpelling),
			"Spelling"))
		.Add(fShowAll)
		.Add(new SeparatorBar())
		.Add(fNormal)
		.Add(fPage)
		.Add(new IconButton("preview", kIconPreview, Message(kMsgPrintPreview),
			"Print Preview"))
		.Add(new IconButton("zoom", kIconZoom, Message(kMsgZoom), "Zoom"))
		.AddGlue();
}


void
Toolbar::Draw(BRect update)
{
	BGroupView::Draw(update);
	DrawBarEdges(this);
}


void
Toolbar::SetState(bool canUndo, bool hasSelection, bool showAll, int mode)
{
	fUndo->SetEnabled(canUndo);
	fCut->SetEnabled(hasSelection);
	fCopy->SetEnabled(hasSelection);
	fShowAll->SetOn(showAll);
	fNormal->SetOn(mode == kModeNormal);
	fPage->SetOn(mode == kModePage);
}


// #pragma mark - Ribbon


Ribbon::Ribbon()
	:
	BGroupView("ribbon", B_HORIZONTAL, 2)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetFlags(Flags() | B_WILL_DRAW);
	GroupLayout()->SetInsets(4, 3, 4, 3);

	fFont = new PopupField("font", Message(kMsgFontFamily), "family",
		be_plain_font->StringWidth("Noto Sans Mono CJK JP") + 24, "Font");
	fSize = new SizeField("size", Message(kMsgFontSize));
	fBold = new IconButton("bold", "B", B_BOLD_FACE,
		Message(kMsgToggleChar, "field", kCFBold), "Bold", true);
	fItalic = new IconButton("italic", "I", B_ITALIC_FACE | B_BOLD_FACE,
		Message(kMsgToggleChar, "field", kCFItalic), "Italic", true);
	fUnderline = new IconButton("underline", "U",
		B_UNDERSCORE_FACE | B_BOLD_FACE,
		Message(kMsgToggleChar, "field", kCFUnderline), "Underline", true);
	fSuper = new IconButton("superscript", kIconSuperscript,
		Message(kMsgSuperscript), "Superscript", true);
	fSub = new IconButton("subscript", kIconSubscript, Message(kMsgSubscript),
		"Subscript", true);
	fShowAll = new IconButton("showAll", kIconShowAll, Message(kMsgShowAll),
		"Show \xC2\xB6", true);
	for (int i = 0; i < 3; i++) {
		static const char* kTips[3] = { "One column", "Two columns",
			"Three columns" };
		fColumns[i] = new IconButton("columns", kIconColumns1 + i,
			Message(kMsgColumns, "columns", i + 1), kTips[i]);
	}

	BLayoutBuilder::Group<>(this)
		.Add(fFont)
		.Add(fSize)
		.Add(new SeparatorBar())
		.Add(fBold)
		.Add(fItalic)
		.Add(fUnderline)
		.Add(new SeparatorBar())
		.Add(fSuper)
		.Add(fSub)
		.Add(new SeparatorBar())
		.Add(new IconButton("picture", kIconPicture, Message(kMsgInsertPicture),
			"Insert picture"))
		.Add(new SeparatorBar())
		.Add(fShowAll)
		.Add(new SeparatorBar())
		.Add(fColumns[0])
		.Add(fColumns[1])
		.Add(fColumns[2])
		.AddGlue();
	UpdateFontList();
}


void
Ribbon::UpdateFontList()
{
	fFont->SetItems(FontCache::Get().Families());
}


void
Ribbon::Draw(BRect update)
{
	BGroupView::Draw(update);
	DrawBarEdges(this);
}


void
Ribbon::SetState(const CharFormat& format, uint32 same, bool showAll,
	int columns)
{
	fFont->SetText((same & kCFFont) ? format.font.c_str() : "");
	fSize->SetSize((same & kCFSize) ? format.size : 0);
	fBold->SetOn((same & kCFBold) && format.bold);
	fItalic->SetOn((same & kCFItalic) && format.italic);
	fUnderline->SetOn((same & kCFUnderline) && format.underline != kULNone);
	fSuper->SetOn((same & kCFPosition) && format.position > 0);
	fSub->SetOn((same & kCFPosition) && format.position < 0);
	fShowAll->SetOn(showAll);
	for (int i = 0; i < 3; i++)
		fColumns[i]->SetOn(columns == i + 1);
}


// #pragma mark - OutlineBar


static BMessage*
OutlineMessage(const char* op, int32 level = 0)
{
	BMessage* message = new BMessage(kMsgOutline);
	message->AddString("op", op);
	message->AddInt32("level", level);
	return message;
}


OutlineBar::OutlineBar()
	:
	BGroupView("outlineBar", B_HORIZONTAL, 2)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetFlags(Flags() | B_WILL_DRAW);
	GroupLayout()->SetInsets(4, 3, 4, 3);
	BLayoutBuilder::Group<> builder(this);
	builder
		.Add(new IconButton("promote", kIconPromote, OutlineMessage("promote"),
			"Promote"))
		.Add(new IconButton("demote", kIconDemote, OutlineMessage("demote"),
			"Demote"))
		.Add(new IconButton("body", kIconToBody, OutlineMessage("body"),
			"Demote to body text"))
		.Add(new SeparatorBar())
		.Add(new IconButton("up", kIconMoveUp, OutlineMessage("up"),
			"Move up"))
		.Add(new IconButton("down", kIconMoveDown, OutlineMessage("down"),
			"Move down"))
		.Add(new SeparatorBar())
		.Add(new IconButton("expand", kIconExpand, OutlineMessage("expand"),
			"Expand"))
		.Add(new IconButton("collapse", kIconCollapse,
			OutlineMessage("collapse"), "Collapse"))
		.Add(new SeparatorBar());
	for (int i = 0; i < 9; i++) {
		BString label;
		label << i + 1;
		BString tip;
		tip << "Show headings 1 to " << i + 1;
		fLevels[i] = new IconButton("level", label.String(), B_BOLD_FACE,
			OutlineMessage("level", i + 1), tip.String());
		builder.Add(fLevels[i]);
	}
	fLevels[9] = new IconButton("all", "All", B_BOLD_FACE,
		OutlineMessage("level", 0), "Show all headings and body text");
	fLevels[9]->SetExplicitMinSize(BSize(fLevels[9]->MinSize().width * 1.6f,
		B_SIZE_UNSET));
	fLevels[9]->SetExplicitMaxSize(fLevels[9]->ExplicitMinSize());
	fFirstLine = new IconButton("firstLine", kIconFirstLine,
		OutlineMessage("firstLine"), "Show first line of body text only");
	fFormatting = new IconButton("formatting", kIconFormatting,
		OutlineMessage("formatting"), "Show character formatting");
	builder
		.Add(fLevels[9])
		.Add(new SeparatorBar())
		.Add(fFirstLine)
		.Add(fFormatting)
		.AddGlue();
}


void
OutlineBar::Draw(BRect update)
{
	BGroupView::Draw(update);
	DrawBarEdges(this);
}


void
OutlineBar::SetState(int32 showLevel, bool firstLine, bool formatting)
{
	for (int i = 0; i < 9; i++)
		fLevels[i]->SetOn(showLevel == i + 1);
	fLevels[9]->SetOn(showLevel == 0);
	fFirstLine->SetOn(firstLine);
	fFormatting->SetOn(formatting);
}


// #pragma mark - RulerStrip


enum {
	kDragNone = 0,
	kDragFirst,
	kDragLeft,
	kDragRight,
	kDragTab,
	kDragMarginLeft,
	kDragMarginRight,
	kDragCell
};

static const float kBandTop = 3;
static const float kBandBottom = 16;


class RulerStrip : public BView {
public:
	RulerStrip(Ruler* ruler)
		:
		BView("strip", B_WILL_DRAW | B_FRAME_EVENTS),
		fRuler(ruler),
		fDrag(kDragNone),
		fTabIndex(-1),
		fRemoveTab(false)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		float height = kBandBottom + 9;
		SetExplicitMinSize(BSize(50, height));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, height));
	}

	void SetState(const RulerState& state)
	{
		if (fDrag != kDragNone)
			return;
		fState = state;
		fFormat = state.format;
		fDragMarginLeft = state.marginLeft;
		fDragMarginRight = state.marginRight;
		fCells.clear();
		for (const CellFormat& cell : state.row.cells)
			fCells.push_back(cell.right);
		Invalidate();
	}

	float Step() const
	{
		switch (fState.unit) {
			case kUnitCm: return 72.0f / 2.54f / 4;
			case kUnitPoint: return 3;
			case kUnitPica: return 6;
			default: return 4.5f;
		}
	}

	float Snap(float value) const
	{
		float step = Step();
		return roundf(value / step) * step;
	}

	float ToX(float pos) const { return fState.origin + pos * fState.zoom; }
	float FromX(float x) const
		{ return (x - fState.origin) / fState.zoom; }

	void DrawTab(float x, uint8 kind, bool gray)
	{
		SetHighColor(gray ? 128 : 0, gray ? 128 : 0, gray ? 128 : 0);
		float y = kBandBottom - 1;
		x = floorf(x);
		switch (kind) {
			case kTabLeft:
				FillRect(BRect(x, y - 6, x + 1, y));
				FillRect(BRect(x, y - 1, x + 5, y));
				break;
			case kTabCenter:
				FillRect(BRect(x, y - 6, x + 1, y));
				FillRect(BRect(x - 4, y - 1, x + 5, y));
				break;
			case kTabRight:
				FillRect(BRect(x, y - 6, x + 1, y));
				FillRect(BRect(x - 4, y - 1, x + 1, y));
				break;
			case kTabDecimal:
				FillRect(BRect(x, y - 6, x + 1, y));
				FillRect(BRect(x - 4, y - 1, x + 5, y));
				FillRect(BRect(x + 3, y - 4, x + 4, y - 3));
				break;
			default:
				FillRect(BRect(x, y - 8, x + 1, y));
				break;
		}
	}

	virtual void Draw(BRect update)
	{
		(void)update;
		BRect bounds = Bounds();
		SetHighColor(ViewColor());
		FillRect(bounds);
		float zoom = fState.zoom;

		// white band where the text is
		float bandLeft = ToX(0);
		float bandRight = ToX(fState.width);
		if (fState.pageView && fRuler->Scale() == 1) {
			bandLeft = fState.pageLeft;
			bandRight = fState.pageLeft + fState.paperWidth * zoom;
		}
		BRect band(bandLeft, kBandTop, bandRight, kBandBottom);
		SetHighColor(255, 255, 255);
		FillRect(band);
		SetHighColor(0, 0, 0);
		StrokeLine(BPoint(bounds.left, kBandBottom),
			BPoint(bounds.right, kBandBottom));

		// ticks and numbers
		float origin = fState.origin;
		if (fState.pageView && fRuler->Scale() == 1)
			origin = fState.pageLeft;
		float major;
		int minors;
		switch (fState.unit) {
			case kUnitCm: major = 72.0f / 2.54f; minors = 4; break;
			case kUnitPoint: major = 72; minors = 12; break;
			case kUnitPica: major = 72; minors = 6; break;
			default: major = 72; minors = 8; break;
		}
		float minor = major / minors;
		while (minor * zoom < 4 && minors > 1) {
			minors /= 2;
			minor = major / minors;
		}
		BFont font(be_plain_font);
		font.SetSize(std::max(8.0f, roundf(be_plain_font->Size() * 0.75f)));
		SetFont(&font);
		SetLowColor(255, 255, 255);
		int first = (int)floorf((bounds.left - origin) / (minor * zoom)) - 1;
		int last = (int)ceilf((bounds.right - origin) / (minor * zoom)) + 1;
		SetHighColor(0, 0, 0);
		for (int i = first; i <= last; i++) {
			float x = floorf(origin + i * minor * zoom);
			int phase = ((i % minors) + minors) % minors;
			float height;
			if (phase == 0)
				height = 5;
			else if (minors % 2 == 0 && phase == minors / 2)
				height = 4;
			else if (minors % 4 == 0 && phase % (minors / 4) == 0)
				height = 3;
			else
				height = 2;
			StrokeLine(BPoint(x, kBandBottom - height),
				BPoint(x, kBandBottom - 1));
			if (phase == 0 && i != 0) {
				int number = i / minors;
				if (fState.unit == kUnitPoint)
					number *= 72;
				else if (fState.unit == kUnitPica)
					number *= 6;
				BString label;
				label << number;
				float width = font.StringWidth(label.String());
				DrawString(label.String(), BPoint(x - width / 2,
					kBandBottom - 6));
			}
		}

		const ParaFormat& format = fFormat;
		bool indentsSame = (fState.same & (kPFLeft | kPFFirst | kPFRight))
			== (kPFLeft | kPFFirst | kPFRight);

		// default tab stops as small ticks below the band
		if ((fState.same & kPFTabs) && fState.defaultTab > 0) {
			float lastTab = 0;
			for (const TabStop& tab : format.tabs)
				lastTab = std::max(lastTab, tab.pos);
			SetHighColor(96, 96, 96);
			for (float pos = (floorf(lastTab / fState.defaultTab) + 1)
					* fState.defaultTab; pos < fState.width;
					pos += fState.defaultTab) {
				float x = floorf(ToX(pos));
				StrokeLine(BPoint(x, kBandBottom + 1),
					BPoint(x, kBandBottom + 2));
			}
		}

		// tab stops
		bool tabsSame = (fState.same & kPFTabs) != 0;
		for (size_t i = 0; i < format.tabs.size(); i++) {
			if (fDrag == kDragTab && (int)i == fTabIndex && fRemoveTab)
				continue;
			DrawTab(ToX(format.tabs[i].pos), format.tabs[i].kind, !tabsSame);
		}

		if (fState.pageView && fRuler->Scale() == 1) {
			// margins as brackets
			float left = fState.pageLeft + fMarginLeft() * zoom;
			float right = fState.pageLeft
				+ (fState.paperWidth - fMarginRight()) * zoom;
			SetHighColor(0, 0, 0);
			for (float x : { left, right }) {
				x = floorf(x);
				bool isLeft = x == floorf(left);
				FillRect(BRect(x, kBandTop, x + 1, kBandBottom + 6));
				float dx = isLeft ? 4 : -4;
				StrokeLine(BPoint(x, kBandTop), BPoint(x + dx, kBandTop));
				StrokeLine(BPoint(x, kBandBottom + 6),
					BPoint(x + dx, kBandBottom + 6));
			}
		} else if (fState.inTable && fRuler->Scale() == 2) {
			SetHighColor(0, 0, 0);
			float rowLeft = fState.rowOrigin + fState.row.leftEdge * zoom;
			for (size_t i = 0; i < fCells.size(); i++) {
				float x = floorf(rowLeft + fCells[i] * zoom);
				StrokeLine(BPoint(x - 3, kBandBottom + 2),
					BPoint(x + 3, kBandBottom + 2));
				StrokeLine(BPoint(x, kBandBottom + 2),
					BPoint(x, kBandBottom + 7));
			}
		} else {
			// indent markers
			rgb_color color = indentsSame ? make_color(0, 0, 0)
				: make_color(128, 128, 128);
			float first = floorf(ToX(format.leftIndent + format.firstIndent));
			float left = floorf(ToX(format.leftIndent));
			float right = floorf(ToX(fState.width - format.rightIndent));
			SetHighColor(color);
			FillTriangle(BPoint(first - 4, kBandTop), BPoint(first + 4,
				kBandTop), BPoint(first, kBandTop + 5));
			FillTriangle(BPoint(left, kBandBottom + 1),
				BPoint(left - 4, kBandBottom + 6),
				BPoint(left + 4, kBandBottom + 6));
			FillTriangle(BPoint(right, kBandBottom + 1),
				BPoint(right - 4, kBandBottom + 6),
				BPoint(right + 4, kBandBottom + 6));
		}
	}

	float fMarginLeft() const { return fDragMarginLeft; }
	float fMarginRight() const { return fDragMarginRight; }

	virtual void MouseDown(BPoint where)
	{
		int32 clicks = 1;
		if (Window()->CurrentMessage() != nullptr)
			Window()->CurrentMessage()->FindInt32("clicks", &clicks);
		fFormat = fState.format;
		fDragMarginLeft = fState.marginLeft;
		fDragMarginRight = fState.marginRight;
		fCells.clear();
		for (const CellFormat& cell : fState.row.cells)
			fCells.push_back(cell.right);
		fDrag = kDragNone;
		fRemoveTab = false;
		float zoom = fState.zoom;

		int tabHit = -1;
		for (size_t i = 0; i < fFormat.tabs.size(); i++) {
			if (fabs(where.x - ToX(fFormat.tabs[i].pos)) <= 4
				&& where.y >= kBandTop + 4 && where.y <= kBandBottom + 1)
				tabHit = (int)i;
		}
		if (clicks >= 2) {
			Window()->PostMessage(tabHit >= 0 ? kMsgTabs : kMsgParagraph);
			return;
		}

		if (fState.pageView && fRuler->Scale() == 1) {
			float left = fState.pageLeft + fState.marginLeft * zoom;
			float right = fState.pageLeft
				+ (fState.paperWidth - fState.marginRight) * zoom;
			if (fabs(where.x - left) <= 4)
				fDrag = kDragMarginLeft;
			else if (fabs(where.x - right) <= 4)
				fDrag = kDragMarginRight;
		} else if (fState.inTable && fRuler->Scale() == 2) {
			float rowLeft = fState.rowOrigin + fState.row.leftEdge * zoom;
			for (size_t i = 0; i < fCells.size(); i++) {
				if (fabs(where.x - (rowLeft + fCells[i] * zoom)) <= 4) {
					fDrag = kDragCell;
					fTabIndex = (int)i;
				}
			}
		} else {
			float first = ToX(fFormat.leftIndent + fFormat.firstIndent);
			float left = ToX(fFormat.leftIndent);
			float right = ToX(fState.width - fFormat.rightIndent);
			if (where.y <= kBandTop + 7 && fabs(where.x - first) <= 5)
				fDrag = kDragFirst;
			else if (where.y > kBandBottom && fabs(where.x - left) <= 5)
				fDrag = kDragLeft;
			else if (where.y > kBandBottom && fabs(where.x - right) <= 5)
				fDrag = kDragRight;
		}
		if (fDrag == kDragNone && tabHit >= 0) {
			fDrag = kDragTab;
			fTabIndex = tabHit;
		} else if (fDrag == kDragNone && where.y >= kBandTop
			&& where.y <= kBandBottom + 1) {
			float pos = Snap(FromX(where.x));
			if (pos >= 0 && pos <= fState.width + 72) {
				TabStop tab;
				tab.pos = pos;
				tab.kind = (uint8)fRuler->TabKind();
				fFormat.SetTab(tab);
				for (size_t i = 0; i < fFormat.tabs.size(); i++) {
					if (fFormat.tabs[i].pos == pos)
						fTabIndex = (int)i;
				}
				fDrag = kDragTab;
			}
		}
		if (fDrag != kDragNone) {
			fShift = (modifiers() & B_SHIFT_KEY) != 0;
			SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
			Invalidate();
		}
	}

	virtual void MouseMoved(BPoint where, uint32 transit,
		const BMessage* drag)
	{
		(void)transit;
		(void)drag;
		if (fDrag == kDragNone)
			return;
		float pos = Snap(FromX(where.x));
		switch (fDrag) {
			case kDragFirst:
				fFormat.firstIndent = pos - fFormat.leftIndent;
				break;
			case kDragLeft: {
				float firstAbsolute = fFormat.leftIndent + fFormat.firstIndent;
				fFormat.leftIndent = pos;
				if (fShift)
					fFormat.firstIndent = firstAbsolute - pos;
				break;
			}
			case kDragRight:
				fFormat.rightIndent = fState.width - pos;
				break;
			case kDragTab:
				if (fTabIndex >= 0 && fTabIndex < (int)fFormat.tabs.size()) {
					fRemoveTab = where.y < -6 || where.y > Bounds().bottom + 10;
					fFormat.tabs[fTabIndex].pos = std::max(0.0f, pos);
				}
				break;
			case kDragMarginLeft:
				fDragMarginLeft = std::max(0.0f, Snap((where.x
					- fState.pageLeft) / fState.zoom));
				break;
			case kDragMarginRight:
				fDragMarginRight = std::max(0.0f, fState.paperWidth
					- Snap((where.x - fState.pageLeft) / fState.zoom));
				break;
			case kDragCell: {
				float rowLeft = fState.rowOrigin
					+ fState.row.leftEdge * fState.zoom;
				float value = Snap((where.x - rowLeft) / fState.zoom);
				float minimum = fTabIndex > 0 ? fCells[fTabIndex - 1] + 9 : 9;
				fCells[fTabIndex] = std::max(minimum, value);
				break;
			}
		}
		Invalidate();
	}

	virtual void MouseUp(BPoint where)
	{
		(void)where;
		int drag = fDrag;
		fDrag = kDragNone;
		BMessage message;
		switch (drag) {
			case kDragFirst:
			case kDragLeft:
			case kDragRight:
				message.what = kMsgApplyIndents;
				message.AddFloat("left", fFormat.leftIndent);
				message.AddFloat("first", fFormat.firstIndent);
				message.AddFloat("right", fFormat.rightIndent);
				message.AddInt32("which", drag);
				break;
			case kDragTab: {
				message.what = kMsgApplyTabs;
				std::vector<TabStop> tabs = fFormat.tabs;
				if (fRemoveTab && fTabIndex >= 0
					&& fTabIndex < (int)tabs.size())
					tabs.erase(tabs.begin() + fTabIndex);
				std::sort(tabs.begin(), tabs.end(),
					[](const TabStop& a, const TabStop& b) {
						return a.pos < b.pos;
					});
				for (const TabStop& tab : tabs) {
					message.AddFloat("pos", tab.pos);
					message.AddInt32("kind", tab.kind);
					message.AddInt32("leader", tab.leader);
				}
				message.AddBool("replace", true);
				break;
			}
			case kDragMarginLeft:
			case kDragMarginRight:
				message.what = kMsgApplyMargins;
				message.AddFloat("left", fDragMarginLeft);
				message.AddFloat("right", fDragMarginRight);
				break;
			case kDragCell:
				message.what = kMsgApplyRow;
				for (float right : fCells)
					message.AddFloat("right", right);
				break;
			default:
				return;
		}
		Window()->PostMessage(&message);
	}

private:
	Ruler*		fRuler;
	RulerState	fState;
	ParaFormat	fFormat;
	int			fDrag;
	int			fTabIndex;
	bool		fRemoveTab;
	bool		fShift = false;
	float		fDragMarginLeft = 90;
	float		fDragMarginRight = 90;
	std::vector<float> fCells;
};


// #pragma mark - Ruler


Ruler::Ruler()
	:
	BGroupView("ruler", B_VERTICAL, 0),
	fTabKind(kTabLeft),
	fScale(0)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetFlags(Flags() | B_WILL_DRAW);

	fStyle = new PopupField("style", Message(kMsgApplyStyle), "name",
		be_plain_font->StringWidth("footnote reference") + 24, "Style");
	static const char* kAlignTips[4] = { "Flush left", "Centered",
		"Flush right", "Justified" };
	for (int i = 0; i < 4; i++) {
		fAlign[i] = new IconButton("align", kIconAlignLeft + i,
			Message(kMsgAlign, "align", i), kAlignTips[i]);
	}
	static const float kSpacing[3] = { 1, 1.5f, 2 };
	static const char* kSpacingTips[3] = { "Single space",
		"1\xC2\xBD line space", "Double space" };
	for (int i = 0; i < 3; i++) {
		fSpacing[i] = new IconButton("spacing", kIconSpaceSingle + i,
			FloatMessage(kMsgLineSpacing, "value", kSpacing[i]),
			kSpacingTips[i]);
	}
	fParaSpacing[0] = new IconButton("closed", kIconParaClosed,
		FloatMessage(kMsgParaSpacing, "before", 0), "Closed paragraph spacing");
	fParaSpacing[1] = new IconButton("open", kIconParaOpen,
		FloatMessage(kMsgParaSpacing, "before", 12),
		"Open paragraph spacing (12 pt before)");
	static const char* kTabTips[5] = { "Left tab", "Centered tab",
		"Right tab", "Decimal tab", "Vertical line" };
	for (int i = 0; i < 5; i++) {
		fTabs[i] = new IconButton("tab", kIconTabLeft + i,
			Message(kMsgTabType, "kind", i), kTabTips[i]);
	}
	fScaleButton = new IconButton("scale", kIconScale, Message(kMsgRulerScale),
		"Ruler scale: normal, page, table");
	fStrip = new RulerStrip(this);

	BLayoutBuilder::Group<>(this)
		.AddGroup(B_HORIZONTAL, 2)
			.SetInsets(4, 3, 4, 1)
			.Add(fStyle)
			.Add(new SeparatorBar())
			.Add(fAlign[0])
			.Add(fAlign[1])
			.Add(fAlign[2])
			.Add(fAlign[3])
			.Add(new SeparatorBar())
			.Add(fSpacing[0])
			.Add(fSpacing[1])
			.Add(fSpacing[2])
			.Add(new SeparatorBar())
			.Add(fParaSpacing[0])
			.Add(fParaSpacing[1])
			.Add(new SeparatorBar())
			.Add(fTabs[0])
			.Add(fTabs[1])
			.Add(fTabs[2])
			.Add(fTabs[3])
			.Add(fTabs[4])
			.Add(new SeparatorBar())
			.Add(fScaleButton)
			.AddGlue()
		.End()
		.Add(fStrip);
	fTabs[0]->SetOn(true);
}


void
Ruler::AttachedToWindow()
{
	BGroupView::AttachedToWindow();
	for (int i = 0; i < 5; i++)
		fTabs[i]->SetTarget(this);
	fScaleButton->SetTarget(this);
}


void
Ruler::Draw(BRect update)
{
	BGroupView::Draw(update);
	DrawBarEdges(this);
}


void
Ruler::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgTabType: {
			int32 kind;
			if (message->FindInt32("kind", &kind) == B_OK) {
				fTabKind = kind;
				for (int i = 0; i < 5; i++)
					fTabs[i]->SetOn(i == kind);
			}
			break;
		}
		case kMsgRulerScale:
			fScale = (fScale + 1) % 3;
			if (fScale == 1 && !fState.pageView)
				fScale = 2;
			if (fScale == 2 && !fState.inTable)
				fScale = 0;
			fScaleButton->SetOn(fScale != 0);
			fStrip->SetState(fState);
			break;
		default:
			BGroupView::MessageReceived(message);
			break;
	}
}


void
Ruler::SetState(const RulerState& state)
{
	fState = state;
	if (fScale == 1 && !state.pageView)
		fScale = 0;
	if (fScale == 2 && !state.inTable)
		fScale = 0;
	fScaleButton->SetOn(fScale != 0);
	const ParaFormat& format = state.format;
	fStyle->SetItems(state.styleNames);
	fStyle->SetText(state.style >= 0
		&& state.style < (int32)state.styleNames.size()
		? state.styleNames[state.style].c_str() : "");
	bool alignSame = (state.same & kPFAlign) != 0;
	for (int i = 0; i < 4; i++)
		fAlign[i]->SetOn(alignSame && format.align == i);
	bool lineSame = (state.same & kPFLine) != 0
		&& format.lineRule == kLineAuto;
	fSpacing[0]->SetOn(lineSame && format.lineValue == 1);
	fSpacing[1]->SetOn(lineSame && format.lineValue == 1.5f);
	fSpacing[2]->SetOn(lineSame && format.lineValue == 2);
	bool beforeSame = (state.same & kPFBefore) != 0;
	fParaSpacing[0]->SetOn(beforeSame && format.spaceBefore == 0);
	fParaSpacing[1]->SetOn(beforeSame && format.spaceBefore == 12);
	fStrip->SetState(state);
}
