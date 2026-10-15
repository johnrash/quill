/*
 * Quill - bar controls.
 */
#include "Controls.h"

#include "Constants.h"
#include "Icons.h"

#include <Bitmap.h>
#include <GroupLayout.h>
#include <ScrollBar.h>
#include <LayoutUtils.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TextControl.h>
#include <Window.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

static const rgb_color kFace = { 222, 222, 222, 255 };
static const rgb_color kFaceDown = { 170, 170, 170, 255 };
static const rgb_color kLight = { 255, 255, 255, 255 };
static const rgb_color kShadow = { 128, 128, 128, 255 };
static const rgb_color kBlackColor = { 0, 0, 0, 255 };


rgb_color
BarColor()
{
	return ui_color(B_PANEL_BACKGROUND_COLOR);
}


// #pragma mark - IconButton


IconButton::IconButton(const char* name, int icon, BMessage* message,
	const char* toolTip, bool toggle)
	:
	BControl(name, nullptr, message, B_WILL_DRAW),
	fIcon(icon),
	fFace(0),
	fToggle(toggle),
	fTracking(false),
	fInside(false)
{
	if (toolTip != nullptr)
		SetToolTip(toolTip);
	SetViewColor(B_TRANSPARENT_COLOR);
}


IconButton::IconButton(const char* name, const char* text, uint16 face,
	BMessage* message, const char* toolTip, bool toggle)
	:
	BControl(name, nullptr, message, B_WILL_DRAW),
	fIcon(-1),
	fText(text),
	fFace(face),
	fToggle(toggle),
	fTracking(false),
	fInside(false)
{
	if (toolTip != nullptr)
		SetToolTip(toolTip);
	SetViewColor(B_TRANSPARENT_COLOR);
}


BSize
IconButton::MinSize()
{
	int scale = IconScale();
	return BSize(21 * scale + (scale - 1) * 2, 19 * scale + (scale - 1) * 2);
}


BSize
IconButton::MaxSize()
{
	return MinSize();
}


BSize
IconButton::PreferredSize()
{
	return MinSize();
}


void
IconButton::SetOn(bool on)
{
	int32 value = on ? B_CONTROL_ON : B_CONTROL_OFF;
	if (Value() != value)
		SetValue(value);
}


void
IconButton::Draw(BRect update)
{
	(void)update;
	BRect rect = Bounds();
	bool down = (fTracking && fInside) || Value() == B_CONTROL_ON;
	rgb_color background = Parent() != nullptr ? Parent()->ViewColor()
		: BarColor();
	if (background == B_TRANSPARENT_COLOR)
		background = BarColor();
	SetHighColor(background);
	FillRect(rect);

	rect.InsetBy(0, 0);
	SetHighColor(kBlackColor);
	StrokeRoundRect(rect, 2, 2);
	BRect inner = rect.InsetByCopy(1, 1);
	SetHighColor(down ? kFaceDown : kFace);
	FillRect(inner);
	SetHighColor(down ? kShadow : kLight);
	StrokeLine(inner.LeftTop(), inner.RightTop());
	StrokeLine(inner.LeftTop(), inner.LeftBottom());
	SetHighColor(down ? kLight : kShadow);
	StrokeLine(inner.LeftBottom(), inner.RightBottom());
	StrokeLine(inner.RightTop(), inner.RightBottom());

	float offset = down ? 1 : 0;
	if (fIcon >= 0) {
		BBitmap* icon = GetIcon(fIcon);
		if (icon != nullptr) {
			BRect bounds = icon->Bounds();
			BPoint where(floorf((rect.Width() - bounds.Width()) / 2) + offset,
				floorf((rect.Height() - bounds.Height()) / 2) + offset);
			SetDrawingMode(B_OP_ALPHA);
			SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
			if (!IsEnabled()) {
				SetHighColor(0, 0, 0, 80);
				SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
			}
			DrawBitmap(icon, where);
			SetDrawingMode(B_OP_COPY);
		}
	} else {
		BFont font(be_bold_font);
		font.SetFamilyAndFace("Noto Serif", fFace != 0 ? fFace : B_BOLD_FACE);
		font.SetSize(floorf(rect.Height() * 0.62f));
		SetFont(&font);
		font_height height;
		font.GetHeight(&height);
		float width = font.StringWidth(fText.String());
		BPoint where(floorf((rect.Width() - width) / 2) + offset,
			floorf((rect.Height() + height.ascent - height.descent) / 2)
				+ offset);
		SetHighColor(IsEnabled() ? kBlackColor : kShadow);
		SetLowColor(down ? kFaceDown : kFace);
		DrawString(fText.String(), where);
		if (fFace & B_UNDERSCORE_FACE) {
			StrokeLine(BPoint(where.x, where.y + 2),
				BPoint(where.x + width - 1, where.y + 2));
		}
	}
}


void
IconButton::MouseDown(BPoint where)
{
	if (!IsEnabled())
		return;
	fTracking = true;
	fInside = Bounds().Contains(where);
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	Invalidate();
}


void
IconButton::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	(void)transit;
	(void)drag;
	if (!fTracking)
		return;
	bool inside = Bounds().Contains(where);
	if (inside != fInside) {
		fInside = inside;
		Invalidate();
	}
}


void
IconButton::MouseUp(BPoint where)
{
	if (!fTracking)
		return;
	fTracking = false;
	if (Bounds().Contains(where)) {
		if (fToggle)
			SetValue(Value() == B_CONTROL_ON ? B_CONTROL_OFF : B_CONTROL_ON);
		Invoke();
	}
	Invalidate();
}


// #pragma mark - PopupField


PopupField::PopupField(const char* name, BMessage* message, const char* field,
	float width, const char* toolTip)
	:
	BView(name, B_WILL_DRAW),
	fMessage(message),
	fField(field),
	fWidth(width),
	fTarget(nullptr),
	fExtraMessage(nullptr)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	if (toolTip != nullptr)
		SetToolTip(toolTip);
}


PopupField::~PopupField()
{
	delete fMessage;
	delete fExtraMessage;
}


BSize
PopupField::MinSize()
{
	font_height height;
	be_plain_font->GetHeight(&height);
	return BSize(fWidth, ceilf(height.ascent + height.descent) + 7);
}


BSize
PopupField::MaxSize()
{
	return MinSize();
}


BSize
PopupField::PreferredSize()
{
	return MinSize();
}


void
PopupField::SetItems(const std::vector<std::string>& items)
{
	fItems = items;
}


void
PopupField::SetText(const char* text)
{
	if (fText == text)
		return;
	fText = text;
	Invalidate();
}


void
PopupField::SetExtraItem(const char* label, BMessage* message)
{
	fExtraLabel = label;
	delete fExtraMessage;
	fExtraMessage = message;
}


void
PopupField::Draw(BRect update)
{
	(void)update;
	BRect rect = Bounds();
	rgb_color background = Parent() != nullptr ? Parent()->ViewColor()
		: BarColor();
	if (background == B_TRANSPARENT_COLOR)
		background = BarColor();
	SetHighColor(background);
	FillRect(rect);
	BRect box = rect;
	box.right -= 1;
	box.bottom -= 1;
	SetHighColor(255, 255, 255);
	FillRect(box);
	SetHighColor(0, 0, 0);
	StrokeRect(box);
	// drop shadow, as on System 7 pop-up menus
	StrokeLine(BPoint(box.left + 2, rect.bottom), rect.RightBottom());
	StrokeLine(BPoint(rect.right, box.top + 2), rect.RightBottom());

	SetFont(be_plain_font);
	font_height height;
	be_plain_font->GetHeight(&height);
	float baseline = floorf((box.Height() + height.ascent - height.descent)
		/ 2) + 1;
	float arrowWidth = 12;
	BString text(fText);
	be_plain_font->TruncateString(&text, B_TRUNCATE_END,
		box.Width() - arrowWidth - 6);
	SetLowColor(255, 255, 255);
	DrawString(text.String(), BPoint(box.left + 4, baseline));

	// down-pointing triangle
	float cx = box.right - 7;
	float cy = floorf(box.Height() / 2) + 1;
	FillTriangle(BPoint(cx - 4, cy - 2), BPoint(cx + 4, cy - 2),
		BPoint(cx, cy + 2));
}


void
PopupField::MouseDown(BPoint where)
{
	(void)where;
	BPopUpMenu* menu = new BPopUpMenu("popup", false, false);
	menu->SetFont(be_plain_font);
	for (const std::string& item : fItems) {
		BMessage* message = new BMessage(*fMessage);
		message->AddString(fField.String(), item.c_str());
		BMenuItem* menuItem = new BMenuItem(item.c_str(), message);
		if (fText == item.c_str())
			menuItem->SetMarked(true);
		menu->AddItem(menuItem);
	}
	if (fExtraMessage != nullptr) {
		menu->AddSeparatorItem();
		menu->AddItem(new BMenuItem(fExtraLabel.String(),
			new BMessage(*fExtraMessage)));
	}
	BHandler* target = fTarget != nullptr ? fTarget : Window();
	menu->SetTargetForItems(target);
	menu->SetAsyncAutoDestruct(true);
	BPoint at = ConvertToScreen(Bounds().LeftBottom());
	BMenuItem* marked = menu->FindMarked();
	if (marked != nullptr) {
		menu->Go(ConvertToScreen(Bounds().LeftTop()), true, false,
			ConvertToScreen(Bounds()), true);
	} else
		menu->Go(at, true, false, ConvertToScreen(Bounds()), true);
}


// #pragma mark - SizeField


enum {
	kMsgSizeEntered = 'Szen',
	kMsgSizePicked = 'Szpk'
};


SizeField::SizeField(const char* name, BMessage* message)
	:
	BView(name, 0),
	fMessage(message),
	fTarget(nullptr)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	fText = new BTextControl("text", nullptr, "12",
		new BMessage(kMsgSizeEntered));
	fText->SetExplicitMinSize(BSize(be_plain_font->StringWidth("888.5") + 12,
		B_SIZE_UNSET));
	fText->SetExplicitMaxSize(BSize(be_plain_font->StringWidth("888.5") + 12,
		B_SIZE_UNSET));
	fText->SetToolTip("Font size");
	fPopup = new PopupField("sizes", new BMessage(kMsgSizePicked), "item",
		16, "Font size");
	std::vector<std::string> sizes = { "7", "9", "10", "12", "14", "18", "24",
		"36", "48", "60", "72" };
	fPopup->SetItems(sizes);
	BGroupLayout* layout = new BGroupLayout(B_HORIZONTAL, 0);
	SetLayout(layout);
	layout->AddView(fText);
	layout->AddView(fPopup);
}


void
SizeField::AttachedToWindow()
{
	BView::AttachedToWindow();
	fText->SetTarget(this);
	fPopup->SetTarget(this);
}


void
SizeField::SetTarget(BHandler* target)
{
	fTarget = target;
}


void
SizeField::SetSize(float size)
{
	if (fText->TextView()->IsFocus())
		return;
	BString text;
	if (size > 0) {
		if (size == floorf(size))
			text.SetToFormat("%d", (int)size);
		else
			text.SetToFormat("%.1f", size);
	}
	if (text != fText->Text())
		fText->SetText(text.String());
}


void
SizeField::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSizeEntered:
		case kMsgSizePicked: {
			const char* text = fText->Text();
			if (message->what == kMsgSizePicked)
				message->FindString("item", &text);
			float size = (float)atof(text);
			if (size < 1 || size > 1638)
				break;
			BMessage forward(*fMessage);
			forward.AddFloat("size", size);
			forward.AddBool("refocus", true);
			BHandler* target = fTarget != nullptr ? fTarget : Window();
			if (target != nullptr && target->Looper() != nullptr)
				target->Looper()->PostMessage(&forward, target);
			break;
		}
		default:
			BView::MessageReceived(message);
			break;
	}
}


// #pragma mark - StatusView


StatusView::StatusView(const char* name)
	:
	BView(name, B_WILL_DRAW)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


BSize
StatusView::MinSize()
{
	return BSize(be_plain_font->StringWidth("Pg 9999   Sec 1  999/999") + 10,
		B_H_SCROLL_BAR_HEIGHT);
}


BSize
StatusView::MaxSize()
{
	return MinSize();
}


BSize
StatusView::PreferredSize()
{
	return MinSize();
}


void
StatusView::SetText(const char* text)
{
	if (fText == text)
		return;
	fText = text;
	Invalidate();
}


void
StatusView::SetMessage(const char* text)
{
	fMessage = text;
	Invalidate();
}


void
StatusView::Draw(BRect update)
{
	(void)update;
	BRect rect = Bounds();
	SetHighColor(255, 255, 255);
	FillRect(rect);
	SetHighColor(0, 0, 0);
	StrokeLine(rect.LeftTop(), rect.RightTop());
	StrokeLine(rect.RightTop(), rect.RightBottom());
	BFont font(be_plain_font);
	font.SetSize(std::max(9.0f, be_plain_font->Size() - 1));
	SetFont(&font);
	font_height height;
	font.GetHeight(&height);
	float baseline = floorf((rect.Height() + height.ascent - height.descent)
		/ 2) + 1;
	SetLowColor(255, 255, 255);
	const char* text = fMessage.Length() > 0 ? fMessage.String()
		: fText.String();
	DrawString(text, BPoint(5, baseline));
}


void
StatusView::MouseDown(BPoint where)
{
	(void)where;
	int32 clicks = 1;
	if (Window()->CurrentMessage() != nullptr)
		Window()->CurrentMessage()->FindInt32("clicks", &clicks);
	if (clicks >= 2)
		Window()->PostMessage(kMsgGoTo);
}


// #pragma mark - SeparatorBar


SeparatorBar::SeparatorBar(float width)
	:
	BView("separator", B_WILL_DRAW)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetExplicitMinSize(BSize(width, 4));
	SetExplicitMaxSize(BSize(width, B_SIZE_UNLIMITED));
}


void
SeparatorBar::Draw(BRect update)
{
	(void)update;
	BRect rect = Bounds();
	float x = floorf(rect.Width() / 2);
	SetHighColor(tint_color(ViewColor(), B_DARKEN_2_TINT));
	StrokeLine(BPoint(x, rect.top + 3), BPoint(x, rect.bottom - 3));
	SetHighColor(tint_color(ViewColor(), B_LIGHTEN_2_TINT));
	StrokeLine(BPoint(x + 1, rect.top + 3), BPoint(x + 1, rect.bottom - 3));
}
