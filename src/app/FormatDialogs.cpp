/*
 * Quill - dialogs of the Format menu.
 */
#include "Dialogs.h"

#include "Constants.h"
#include "Fonts.h"
#include "FormatMessages.h"
#include "Prefs.h"

#include <Alert.h>
#include <Beep.h>
#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <GridView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <RadioButton.h>
#include <ScrollView.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace quill;

enum {
	kMsgOK = 'dOK ',
	kMsgApply = 'dApl',
	kMsgCancel = 'dCnl',
	kMsgChanged = 'dChg',
	kMsgOpenTabs = 'dTab',
	kMsgOpenBorder = 'dBrd',
	kMsgOpenCharacter = 'dChr',
	kMsgOpenParagraph = 'dPar',
	kMsgSetTab = 'dSet',
	kMsgClearTab = 'dClr',
	kMsgClearAllTabs = 'dCla',
	kMsgTabSelected = 'dTbs',
	kMsgPreset = 'dPre',
	kMsgStyleSelected = 'dSsl',
	kMsgDefine = 'dDef',
	kMsgDelete = 'dDel',
	kMsgPaper = 'dPpr',
	kMsgOpenDocument = 'dDoc',
	kMsgTableOp = 'dTop'
};


// #pragma mark - DialogWindow


DialogWindow::DialogWindow(BWindow* parent, const char* title, uint32 flags)
	:
	BWindow(BRect(0, 0, 100, 100), title, B_TITLED_WINDOW_LOOK,
		B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE
			| B_NOT_MINIMIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS | B_CLOSE_ON_ESCAPE | flags),
	fParent(parent),
	fMessenger(parent)
{
	if (parent != nullptr)
		AddToSubset(parent);
}


void
DialogWindow::Show()
{
	if (fParent != nullptr && fParent->Lock()) {
		BRect frame = fParent->Frame();
		fParent->Unlock();
		Layout(true);
		CenterIn(frame);
	} else
		CenterOnScreen();
	BWindow::Show();
}


void
DialogWindow::SendToParent(BMessage* message)
{
	fMessenger.SendMessage(message);
}


bool
DialogWindow::QuitRequested()
{
	return true;
}


// #pragma mark - helpers


static BTextControl*
MeasureField(const char* label, float points, int unit, bool mixed,
	uint32 message = kMsgChanged)
{
	BTextControl* control = new BTextControl(label, label,
		mixed ? "" : FormatMeasure(points, unit).c_str(),
		new BMessage(message));
	control->SetModificationMessage(new BMessage(message));
	control->TextView()->SetExplicitMinSize(BSize(
		be_plain_font->StringWidth("00.00 in") + 10, B_SIZE_UNSET));
	return control;
}


static BTextControl*
PointsField(const char* label, float points, bool mixed)
{
	BTextControl* control = new BTextControl(label, label,
		mixed ? "" : FormatPoints(points).c_str(), new BMessage(kMsgChanged));
	control->TextView()->SetExplicitMinSize(BSize(
		be_plain_font->StringWidth("000.0 pt") + 10, B_SIZE_UNSET));
	return control;
}


static bool
ReadMeasure(BTextControl* control, int unit, float* value)
{
	return ParseMeasure(control->Text(), unit, value);
}


static BMenuField*
ChoiceField(const char* label, const char* const* items, int count,
	int selected, uint32 message = kMsgChanged)
{
	BPopUpMenu* menu = new BPopUpMenu("");
	for (int i = 0; i < count; i++) {
		BMessage* itemMessage = new BMessage(message);
		itemMessage->AddInt32("index", i);
		BMenuItem* item = new BMenuItem(items[i], itemMessage);
		if (i == selected)
			item->SetMarked(true);
		menu->AddItem(item);
	}
	return new BMenuField(label, label, menu);
}


static int
ChoiceIndex(BMenuField* field)
{
	BMenuItem* item = field->Menu()->FindMarked();
	return item != nullptr ? field->Menu()->IndexOf(item) : -1;
}


static void
SetCheck(BCheckBox* box, bool value, bool same)
{
	box->SetValue(same ? (value ? B_CONTROL_ON : B_CONTROL_OFF)
		: B_CONTROL_PARTIALLY_ON);
}


static BGroupView*
ButtonColumn(BButton* first, BButton* second, BButton* third = nullptr,
	BButton* fourth = nullptr, BButton* fifth = nullptr)
{
	BGroupView* group = new BGroupView(B_VERTICAL, B_USE_SMALL_SPACING);
	BGroupLayout* layout = group->GroupLayout();
	for (BButton* button : { first, second, third, fourth, fifth }) {
		if (button == nullptr)
			continue;
		button->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
		layout->AddView(button);
	}
	layout->AddItem(BSpaceLayoutItem::CreateGlue());
	return group;
}


static BBox*
LabeledBox(const char* label, BView* content)
{
	BBox* box = new BBox(label);
	box->SetLabel(label);
	BGroupLayout* layout = new BGroupLayout(B_VERTICAL, 0);
	box->SetLayout(layout);
	layout->SetInsets(B_USE_SMALL_SPACING, B_USE_BIG_SPACING,
		B_USE_SMALL_SPACING, B_USE_SMALL_SPACING);
	layout->AddView(content);
	layout->AddItem(BSpaceLayoutItem::CreateGlue());
	return box;
}


// #pragma mark - Character


class CharacterDialog : public DialogWindow {
public:
	CharacterDialog(BWindow* parent, const CharFormat& format, uint32 same)
		:
		DialogWindow(parent, "Character"),
		fFormat(format),
		fSame(same)
	{
		BPopUpMenu* fonts = new BPopUpMenu("");
		for (const std::string& family : FontCache::Get().Families()) {
			BMenuItem* item = new BMenuItem(family.c_str(),
				new BMessage(kMsgChanged));
			if ((same & kCFFont) && family == format.font)
				item->SetMarked(true);
			fonts->AddItem(item);
		}
		if ((same & kCFFont) && fonts->FindMarked() == nullptr) {
			BMenuItem* item = new BMenuItem(format.font.c_str(),
				new BMessage(kMsgChanged));
			item->SetMarked(true);
			fonts->AddItem(item, 0);
		}
		fFont = new BMenuField("font", "Font:", fonts);

		BString size;
		if (same & kCFSize)
			size.SetToFormat("%g", format.size);
		fSize = new BTextControl("size", "Size:", size.String(),
			new BMessage(kMsgChanged));
		fSize->TextView()->SetExplicitMinSize(BSize(
			be_plain_font->StringWidth("0000.0"), B_SIZE_UNSET));
		BPopUpMenu* sizes = new BPopUpMenu("sizes", false, false);
		for (int i = 0; i < kStandardSizeCount; i++) {
			BString label;
			label << (int)kStandardSizes[i];
			BMessage* message = new BMessage('dSiz');
			message->AddString("size", label.String());
			sizes->AddItem(new BMenuItem(label.String(), message));
		}
		BMenuField* sizeMenu = new BMenuField("sizeMenu", nullptr, sizes);
		sizes->Superitem()->SetLabel("\xE2\x96\xBE");

		static const char* kUnderlines[] = { "None", "Single", "Word",
			"Double", "Dotted" };
		fUnderline = ChoiceField("Underline:", kUnderlines, 5,
			(same & kCFUnderline) ? format.underline : -1);
		const char* colors[kNamedColorCount];
		for (int i = 0; i < kNamedColorCount; i++)
			colors[i] = kNamedColors[i].name;
		int colorIndex = NamedColorIndex(format.color);
		fColor = ChoiceField("Color:", colors, kNamedColorCount,
			(same & kCFColor) ? colorIndex : -1);
		if ((same & kCFColor) && colorIndex < 0) {
			BMenuItem* item = new BMenuItem("Other",
				new BMessage(kMsgChanged));
			item->SetMarked(true);
			fColor->Menu()->AddItem(item);
		}

		struct Box {
			const char*	label;
			uint32		field;
			bool		value;
		} boxes[] = {
			{ "Bold", kCFBold, format.bold },
			{ "Italic", kCFItalic, format.italic },
			{ "Outline", kCFOutline, format.outline },
			{ "Shadow", kCFShadow, format.shadow },
			{ "Strikethru", kCFStrike, format.strike },
			{ "Small Caps", kCFSmallCaps, format.smallCaps },
			{ "All Caps", kCFAllCaps, format.allCaps },
			{ "Hidden", kCFHidden, format.hidden }
		};
		BGroupView* styleGroup = new BGroupView(B_VERTICAL, 0);
		for (int i = 0; i < 8; i++) {
			fBoxes[i] = new BCheckBox(boxes[i].label, boxes[i].label,
				new BMessage(kMsgChanged));
			SetCheck(fBoxes[i], boxes[i].value, (same & boxes[i].field) != 0);
			fFields[i] = boxes[i].field;
			styleGroup->GroupLayout()->AddView(fBoxes[i]);
		}

		bool positionSame = (same & kCFPosition) != 0;
		fNormalPosition = new BRadioButton("normal", "Normal",
			new BMessage(kMsgChanged));
		fSuper = new BRadioButton("super", "Superscript",
			new BMessage(kMsgChanged));
		fSub = new BRadioButton("sub", "Subscript", new BMessage(kMsgChanged));
		if (positionSame) {
			if (format.position > 0)
				fSuper->SetValue(1);
			else if (format.position < 0)
				fSub->SetValue(1);
			else
				fNormalPosition->SetValue(1);
		}
		BString by;
		if (positionSame && format.position != 0)
			by = FormatPoints(fabs(format.position)).c_str();
		else
			by = "3 pt";
		fPositionBy = new BTextControl("by", "By:", by.String(),
			new BMessage(kMsgChanged));

		bool spacingSame = (same & kCFSpacing) != 0;
		fNormalSpacing = new BRadioButton("normal", "Normal",
			new BMessage(kMsgChanged));
		fCondensed = new BRadioButton("condensed", "Condensed",
			new BMessage(kMsgChanged));
		fExpanded = new BRadioButton("expanded", "Expanded",
			new BMessage(kMsgChanged));
		if (spacingSame) {
			if (format.spacing > 0)
				fExpanded->SetValue(1);
			else if (format.spacing < 0)
				fCondensed->SetValue(1);
			else
				fNormalSpacing->SetValue(1);
		}
		BString spacingBy;
		if (spacingSame && format.spacing != 0)
			spacingBy = FormatPoints(fabs(format.spacing)).c_str();
		else
			spacingBy = "1 pt";
		fSpacingBy = new BTextControl("by", "By:", spacingBy.String(),
			new BMessage(kMsgChanged));

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BButton* apply = new BButton("apply", "Apply", new BMessage(kMsgApply));

		BGroupView* positionGroup = new BGroupView(B_VERTICAL, 0);
		BLayoutBuilder::Group<>(positionGroup)
			.Add(fNormalPosition)
			.Add(fSuper)
			.Add(fSub)
			.Add(fPositionBy);
		BGroupView* spacingGroup = new BGroupView(B_VERTICAL, 0);
		BLayoutBuilder::Group<>(spacingGroup)
			.Add(fNormalSpacing)
			.Add(fCondensed)
			.Add(fExpanded)
			.Add(fSpacingBy);

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGrid()
					.AddMenuField(fFont, 0, 0)
					.AddGroup(B_HORIZONTAL, 0, 2, 0)
						.Add(fSize->CreateLabelLayoutItem())
						.Add(fSize->CreateTextViewLayoutItem())
						.Add(sizeMenu)
					.End()
					.AddMenuField(fUnderline, 0, 1)
					.AddMenuField(fColor, 2, 1)
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(LabeledBox("Style", styleGroup))
					.AddGroup(B_VERTICAL)
						.Add(LabeledBox("Position", positionGroup))
						.Add(LabeledBox("Spacing", spacingGroup))
					.End()
				.End()
			.End()
			.Add(ButtonColumn(ok, cancel, apply));
		SetDefaultButton(ok);
	}

	uint32 Collect(CharFormat* format)
	{
		uint32 mask = 0;
		BMenuItem* font = fFont->Menu()->FindMarked();
		if (font != nullptr && (!(fSame & kCFFont)
				|| fFormat.font != font->Label())) {
			format->font = font->Label();
			mask |= kCFFont;
		}
		float size = (float)atof(fSize->Text());
		if (size >= 1 && size <= 1638 && (!(fSame & kCFSize)
				|| size != fFormat.size)) {
			format->size = size;
			mask |= kCFSize;
		}
		int underline = ChoiceIndex(fUnderline);
		if (underline >= 0 && (!(fSame & kCFUnderline)
				|| underline != fFormat.underline)) {
			format->underline = (uint8)underline;
			mask |= kCFUnderline;
		}
		int color = ChoiceIndex(fColor);
		if (color >= 0 && color < kNamedColorCount && (!(fSame & kCFColor)
				|| kNamedColors[color].rgb != fFormat.color)) {
			format->color = kNamedColors[color].rgb;
			mask |= kCFColor;
		}
		for (int i = 0; i < 8; i++) {
			int32 value = fBoxes[i]->Value();
			if (value == B_CONTROL_PARTIALLY_ON)
				continue;
			bool on = value == B_CONTROL_ON;
			bool* target = nullptr;
			switch (fFields[i]) {
				case kCFBold: target = &format->bold; break;
				case kCFItalic: target = &format->italic; break;
				case kCFOutline: target = &format->outline; break;
				case kCFShadow: target = &format->shadow; break;
				case kCFStrike: target = &format->strike; break;
				case kCFSmallCaps: target = &format->smallCaps; break;
				case kCFAllCaps: target = &format->allCaps; break;
				case kCFHidden: target = &format->hidden; break;
			}
			*target = on;
			mask |= fFields[i];
		}
		float by = 0;
		ParseMeasure(fPositionBy->Text(), kUnitPoint, &by);
		by = fabs(by);
		if (fNormalPosition->Value() || fSuper->Value() || fSub->Value()) {
			float position = fSuper->Value() ? by
				: (fSub->Value() ? -by : 0);
			if (!(fSame & kCFPosition) || position != fFormat.position) {
				format->position = position;
				mask |= kCFPosition;
			}
		}
		float spacingBy = 0;
		ParseMeasure(fSpacingBy->Text(), kUnitPoint, &spacingBy);
		spacingBy = fabs(spacingBy);
		if (fNormalSpacing->Value() || fCondensed->Value()
			|| fExpanded->Value()) {
			float spacing = fExpanded->Value() ? spacingBy
				: (fCondensed->Value() ? -spacingBy : 0);
			if (!(fSame & kCFSpacing) || spacing != fFormat.spacing) {
				format->spacing = spacing;
				mask |= kCFSpacing;
			}
		}
		return mask;
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case 'dSiz': {
				const char* size;
				if (message->FindString("size", &size) == B_OK)
					fSize->SetText(size);
				break;
			}
			case kMsgOK:
			case kMsgApply: {
				CharFormat format = fFormat;
				uint32 mask = Collect(&format);
				if (mask != 0) {
					BMessage result(kMsgApplyCharacter);
					AddCharFormat(&result, format);
					result.AddUInt32("mask", mask);
					SendToParent(&result);
					// what was applied is now the starting point
					CopyCharFields(fFormat, format, mask);
					fSame |= mask;
				}
				if (message->what == kMsgOK)
					Quit();
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			case kMsgChanged:
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	CharFormat		fFormat;
	uint32			fSame;
	BMenuField*		fFont;
	BTextControl*	fSize;
	BMenuField*		fUnderline;
	BMenuField*		fColor;
	BCheckBox*		fBoxes[8];
	uint32			fFields[8];
	BRadioButton*	fNormalPosition;
	BRadioButton*	fSuper;
	BRadioButton*	fSub;
	BTextControl*	fPositionBy;
	BRadioButton*	fNormalSpacing;
	BRadioButton*	fCondensed;
	BRadioButton*	fExpanded;
	BTextControl*	fSpacingBy;
};


void
ShowCharacterDialog(BWindow* parent, const CharFormat& format, uint32 same)
{
	(new CharacterDialog(parent, format, same))->Show();
}


// #pragma mark - Paragraph


class ParagraphDialog : public DialogWindow {
public:
	ParagraphDialog(BWindow* parent, const ParaFormat& format, uint32 same,
		int unit)
		:
		DialogWindow(parent, "Paragraph"),
		fFormat(format),
		fSame(same),
		fUnit(unit)
	{
		fLeft = MeasureField("Left:", format.leftIndent, unit,
			!(same & kPFLeft));
		fRight = MeasureField("Right:", format.rightIndent, unit,
			!(same & kPFRight));
		fFirst = MeasureField("First:", format.firstIndent, unit,
			!(same & kPFFirst));
		fBefore = PointsField("Before:", format.spaceBefore,
			!(same & kPFBefore));
		fAfter = PointsField("After:", format.spaceAfter, !(same & kPFAfter));

		static const char* kLines[] = { "Auto", "At Least", "Exactly" };
		fLineRule = ChoiceField("Line:", kLines, 3,
			(same & kPFLine) ? format.lineRule : -1);
		BString lineValue;
		if (same & kPFLine) {
			if (format.lineRule == kLineAuto)
				lineValue.SetToFormat("%g li", format.lineValue);
			else
				lineValue = FormatPoints(format.lineValue).c_str();
		}
		fLineValue = new BTextControl("lineValue", nullptr, lineValue.String(),
			new BMessage(kMsgChanged));

		static const char* kAligns[] = { "Flush Left", "Centered",
			"Flush Right", "Justified" };
		fAlign = ChoiceField("Alignment:", kAligns, 4,
			(same & kPFAlign) ? format.align : -1);

		fBreakBefore = new BCheckBox("breakBefore", "Page Break Before",
			new BMessage(kMsgChanged));
		SetCheck(fBreakBefore, format.pageBreakBefore,
			(same & kPFBreakBefore) != 0);
		fKeepNext = new BCheckBox("keepNext", "Keep With Next \xC2\xB6",
			new BMessage(kMsgChanged));
		SetCheck(fKeepNext, format.keepWithNext, (same & kPFKeepNext) != 0);
		fKeepTogether = new BCheckBox("keepTogether", "Keep Lines Together",
			new BMessage(kMsgChanged));
		SetCheck(fKeepTogether, format.keepTogether,
			(same & kPFKeepTogether) != 0);
		fNoLineNumbers = new BCheckBox("noLineNumbers",
			"Suppress Line Numbers", new BMessage(kMsgChanged));
		SetCheck(fNoLineNumbers, format.noLineNumbers,
			(same & kPFNoLineNum) != 0);

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BButton* apply = new BButton("apply", "Apply", new BMessage(kMsgApply));
		BButton* tabs = new BButton("tabs", "Tabs" B_UTF8_ELLIPSIS,
			new BMessage(kMsgOpenTabs));
		BButton* border = new BButton("border", "Border" B_UTF8_ELLIPSIS,
			new BMessage(kMsgOpenBorder));

		BGridView* indents = new BGridView();
		BLayoutBuilder::Grid<>(indents)
			.AddTextControl(fLeft, 0, 0)
			.AddTextControl(fRight, 0, 1)
			.AddTextControl(fFirst, 0, 2);
		BGridView* spacing = new BGridView();
		BLayoutBuilder::Grid<>(spacing)
			.AddTextControl(fBefore, 0, 0)
			.AddTextControl(fAfter, 0, 1)
			.AddMenuField(fLineRule, 0, 2)
			.Add(fLineValue, 2, 2);
		BGroupView* pagination = new BGroupView(B_VERTICAL, 0);
		BLayoutBuilder::Group<>(pagination)
			.Add(fBreakBefore)
			.Add(fKeepNext)
			.Add(fKeepTogether)
			.Add(fNoLineNumbers);

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGroup(B_HORIZONTAL)
					.Add(LabeledBox("Indentation", indents))
					.Add(LabeledBox("Spacing", spacing))
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(LabeledBox("Pagination", pagination))
					.AddGroup(B_VERTICAL)
						.AddGrid()
							.AddMenuField(fAlign, 0, 0)
						.End()
						.AddGlue()
						.AddGroup(B_HORIZONTAL)
							.Add(tabs)
							.Add(border)
						.End()
					.End()
				.End()
			.End()
			.Add(ButtonColumn(ok, cancel, apply));
		SetDefaultButton(ok);
	}

	uint32 Collect(ParaFormat* format)
	{
		uint32 mask = 0;
		float value;
		struct Measure {
			BTextControl*	control;
			uint32			field;
			float*			target;
			float			old;
			bool			points;
		} measures[] = {
			{ fLeft, kPFLeft, &format->leftIndent, fFormat.leftIndent, false },
			{ fRight, kPFRight, &format->rightIndent, fFormat.rightIndent,
				false },
			{ fFirst, kPFFirst, &format->firstIndent, fFormat.firstIndent,
				false },
			{ fBefore, kPFBefore, &format->spaceBefore, fFormat.spaceBefore,
				true },
			{ fAfter, kPFAfter, &format->spaceAfter, fFormat.spaceAfter, true }
		};
		for (const Measure& measure : measures) {
			if (measure.control->Text()[0] == '\0')
				continue;
			if (!ParseMeasure(measure.control->Text(),
					measure.points ? kUnitPoint : fUnit, &value))
				continue;
			if (measure.points)
				value = std::max(0.0f, value);
			if (!(fSame & measure.field) || fabs(value - measure.old) > 0.01f) {
				*measure.target = value;
				mask |= measure.field;
			}
		}
		int rule = ChoiceIndex(fLineRule);
		if (rule >= 0 && fLineValue->Text()[0] != '\0') {
			float line;
			bool ok;
			if (rule == kLineAuto) {
				line = (float)atof(fLineValue->Text());
				ok = line >= 0.5f && line <= 10;
			} else
				ok = ParseMeasure(fLineValue->Text(), kUnitPoint, &line)
					&& line > 0;
			if (ok && (!(fSame & kPFLine) || rule != fFormat.lineRule
					|| line != fFormat.lineValue)) {
				format->lineRule = (uint8)rule;
				format->lineValue = line;
				mask |= kPFLine;
			}
		} else if (rule >= 0 && rule != fFormat.lineRule) {
			format->lineRule = (uint8)rule;
			format->lineValue = rule == kLineAuto ? 1 : 12;
			mask |= kPFLine;
		}
		int align = ChoiceIndex(fAlign);
		if (align >= 0 && (!(fSame & kPFAlign) || align != fFormat.align)) {
			format->align = (uint8)align;
			mask |= kPFAlign;
		}
		struct Flag {
			BCheckBox*	box;
			uint32		field;
			bool*		target;
		} flags[] = {
			{ fBreakBefore, kPFBreakBefore, &format->pageBreakBefore },
			{ fKeepNext, kPFKeepNext, &format->keepWithNext },
			{ fKeepTogether, kPFKeepTogether, &format->keepTogether },
			{ fNoLineNumbers, kPFNoLineNum, &format->noLineNumbers }
		};
		for (const Flag& flag : flags) {
			if (flag.box->Value() == B_CONTROL_PARTIALLY_ON)
				continue;
			*flag.target = flag.box->Value() == B_CONTROL_ON;
			mask |= flag.field;
		}
		return mask;
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgOK:
			case kMsgApply: {
				ParaFormat format = fFormat;
				uint32 mask = Collect(&format);
				if (mask != 0) {
					BMessage result(kMsgApplyParagraph);
					AddParaFormat(&result, format);
					result.AddUInt32("mask", mask & ~(kPFTabs | kPFBorder
						| kPFStyle));
					SendToParent(&result);
					CopyParaFields(fFormat, format, mask);
					fSame |= mask;
				}
				if (message->what == kMsgOK)
					Quit();
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			case kMsgOpenTabs:
				ShowTabsDialog(fParent, fFormat, fUnit);
				break;
			case kMsgOpenBorder:
				ShowBorderDialog(fParent, fFormat.border);
				break;
			case kMsgChanged:
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	ParaFormat		fFormat;
	uint32			fSame;
	int				fUnit;
	BTextControl*	fLeft;
	BTextControl*	fRight;
	BTextControl*	fFirst;
	BTextControl*	fBefore;
	BTextControl*	fAfter;
	BMenuField*		fLineRule;
	BTextControl*	fLineValue;
	BMenuField*		fAlign;
	BCheckBox*		fBreakBefore;
	BCheckBox*		fKeepNext;
	BCheckBox*		fKeepTogether;
	BCheckBox*		fNoLineNumbers;
};


void
ShowParagraphDialog(BWindow* parent, const ParaFormat& format, uint32 same,
	int unit)
{
	(new ParagraphDialog(parent, format, same, unit))->Show();
}


// #pragma mark - Tabs


class TabsDialog : public DialogWindow {
public:
	TabsDialog(BWindow* parent, const ParaFormat& format, int unit)
		:
		DialogWindow(parent, "Tabs"),
		fTabs(format.tabs),
		fUnit(unit)
	{
		fPosition = new BTextControl("position", "Position:", "",
			new BMessage(kMsgSetTab));
		fList = new BListView("tabs");
		fList->SetSelectionMessage(new BMessage(kMsgTabSelected));
		BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true);
		scroll->SetExplicitMinSize(BSize(140, 120));

		static const char* kKinds[] = { "Left", "Center", "Right", "Decimal",
			"Vertical Line" };
		BGroupView* kinds = new BGroupView(B_VERTICAL, 0);
		for (int i = 0; i < 5; i++) {
			fKinds[i] = new BRadioButton(kKinds[i], kKinds[i],
				new BMessage(kMsgChanged));
			kinds->GroupLayout()->AddView(fKinds[i]);
		}
		fKinds[0]->SetValue(1);
		static const char* kLeaders[] = { "None", ".........", "- - - - -",
			"________" };
		BGroupView* leaders = new BGroupView(B_VERTICAL, 0);
		for (int i = 0; i < 4; i++) {
			fLeaders[i] = new BRadioButton(kLeaders[i], kLeaders[i],
				new BMessage(kMsgChanged));
			leaders->GroupLayout()->AddView(fLeaders[i]);
		}
		fLeaders[0]->SetValue(1);

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BButton* apply = new BButton("apply", "Apply", new BMessage(kMsgApply));
		BButton* set = new BButton("set", "Set", new BMessage(kMsgSetTab));
		BButton* clear = new BButton("clear", "Clear",
			new BMessage(kMsgClearTab));
		BButton* clearAll = new BButton("clearAll", "Clear All",
			new BMessage(kMsgClearAllTabs));

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.Add(fPosition)
				.Add(scroll)
				.AddGroup(B_HORIZONTAL)
					.Add(set)
					.Add(clear)
					.Add(clearAll)
				.End()
			.End()
			.AddGroup(B_VERTICAL)
				.Add(LabeledBox("Type", kinds))
				.Add(LabeledBox("Leader", leaders))
			.End()
			.Add(ButtonColumn(ok, cancel, apply));
		SetDefaultButton(ok);
		_Fill();
	}

	void _Fill()
	{
		fList->MakeEmpty();
		static const char* kKinds[] = { "", " (Center)", " (Right)",
			" (Decimal)", " (Line)" };
		for (const TabStop& tab : fTabs) {
			BString label = FormatMeasure(tab.pos, fUnit).c_str();
			label << kKinds[tab.kind % 5];
			fList->AddItem(new BStringItem(label.String()));
		}
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgSetTab: {
				float pos;
				if (!ParseMeasure(fPosition->Text(), fUnit, &pos) || pos < 0)
					break;
				TabStop tab;
				tab.pos = pos;
				for (int i = 0; i < 5; i++) {
					if (fKinds[i]->Value())
						tab.kind = (uint8)i;
				}
				for (int i = 0; i < 4; i++) {
					if (fLeaders[i]->Value())
						tab.leader = (uint8)i;
				}
				ParaFormat format;
				format.tabs = fTabs;
				format.SetTab(tab);
				fTabs = format.tabs;
				_Fill();
				break;
			}
			case kMsgClearTab: {
				int32 index = fList->CurrentSelection();
				if (index >= 0 && index < (int32)fTabs.size()) {
					fTabs.erase(fTabs.begin() + index);
					_Fill();
				}
				break;
			}
			case kMsgClearAllTabs:
				fTabs.clear();
				_Fill();
				break;
			case kMsgTabSelected: {
				int32 index = fList->CurrentSelection();
				if (index >= 0 && index < (int32)fTabs.size()) {
					const TabStop& tab = fTabs[index];
					fPosition->SetText(FormatMeasure(tab.pos, fUnit).c_str());
					fKinds[tab.kind % 5]->SetValue(1);
					fLeaders[tab.leader % 4]->SetValue(1);
				}
				break;
			}
			case kMsgOK:
			case kMsgApply: {
				BMessage result(kMsgApplyTabs);
				AddTabs(&result, fTabs);
				result.AddBool("replace", true);
				SendToParent(&result);
				if (message->what == kMsgOK)
					Quit();
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			case kMsgChanged:
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	std::vector<TabStop> fTabs;
	int				fUnit;
	BTextControl*	fPosition;
	BListView*		fList;
	BRadioButton*	fKinds[5];
	BRadioButton*	fLeaders[4];
};


void
ShowTabsDialog(BWindow* parent, const ParaFormat& format, int unit)
{
	(new TabsDialog(parent, format, unit))->Show();
}


// #pragma mark - Border


class BorderPreview : public BView {
public:
	BorderPreview()
		: BView("preview", B_WILL_DRAW)
	{
		SetExplicitMinSize(BSize(120, 90));
		SetExplicitMaxSize(BSize(120, 90));
		SetViewColor(255, 255, 255);
	}

	void SetBorder(const ParaBorder& border)
	{
		fBorder = border;
		Invalidate();
	}

	void Line(uint8 kind, BPoint a, BPoint b)
	{
		if (kind == kBorderNone)
			return;
		SetHighColor(0, 0, 0);
		if (kind == kBorderDotted) {
			for (float t = 0; t <= 1; t += 3 / std::max(1.0f,
					(float)hypot(b.x - a.x, b.y - a.y)))
				FillRect(BRect(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
					a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t));
			return;
		}
		if (kind == kBorderDouble) {
			BPoint offset = a.x == b.x ? BPoint(1.5f, 0) : BPoint(0, 1.5f);
			StrokeLine(a - offset, b - offset);
			StrokeLine(a + offset, b + offset);
			return;
		}
		SetPenSize(kind == kBorderThick ? 2 : 1);
		if (kind == kBorderHairline)
			SetHighColor(100, 100, 100);
		StrokeLine(a, b);
		SetPenSize(1);
	}

	virtual void Draw(BRect update)
	{
		(void)update;
		BRect rect = Bounds().InsetByCopy(12, 10);
		if (fBorder.shading > 0) {
			uint8 gray = (uint8)(255 - fBorder.shading * 255 / 100);
			SetHighColor(gray, gray, gray);
			FillRect(rect);
		}
		SetHighColor(150, 150, 150);
		for (float y = rect.top + 8; y < rect.bottom - 4; y += 6) {
			if (fabs(y - (rect.top + rect.bottom) / 2) < 3)
				continue;
			StrokeLine(BPoint(rect.left + 6, y), BPoint(rect.right - 6, y));
		}
		Line(fBorder.top, rect.LeftTop(), rect.RightTop());
		Line(fBorder.bottom, rect.LeftBottom(), rect.RightBottom());
		Line(fBorder.left, rect.LeftTop(), rect.LeftBottom());
		Line(fBorder.right, rect.RightTop(), rect.RightBottom());
		float middle = floorf((rect.top + rect.bottom) / 2);
		Line(fBorder.between, BPoint(rect.left, middle),
			BPoint(rect.right, middle));
		if (fBorder.shadow) {
			SetHighColor(0, 0, 0);
			FillRect(BRect(rect.left + 3, rect.bottom + 1, rect.right + 3,
				rect.bottom + 3));
			FillRect(BRect(rect.right + 1, rect.top + 3, rect.right + 3,
				rect.bottom + 3));
		}
	}

private:
	ParaBorder	fBorder;
};


class BorderDialog : public DialogWindow {
public:
	BorderDialog(BWindow* parent, const ParaBorder& border)
		:
		DialogWindow(parent, "Border"),
		fBorder(border)
	{
		fPreview = new BorderPreview();
		static const char* kSides[] = { "Top", "Bottom", "Left", "Right",
			"Between" };
		uint8 values[] = { border.top, border.bottom, border.left,
			border.right, border.between };
		BGroupView* sides = new BGroupView(B_VERTICAL, 0);
		uint8 line = kBorderSingle;
		for (int i = 0; i < 5; i++) {
			fSides[i] = new BCheckBox(kSides[i], kSides[i],
				new BMessage(kMsgChanged));
			fSides[i]->SetValue(values[i] != kBorderNone);
			if (values[i] != kBorderNone)
				line = values[i];
			sides->GroupLayout()->AddView(fSides[i]);
		}
		static const char* kLines[] = { "Single", "Thick", "Double", "Dotted",
			"Hairline" };
		fLine = ChoiceField("Line:", kLines, 5, line - 1);
		static const char* kShades[] = { "None", "5%", "10%", "20%", "25%",
			"30%", "40%", "50%", "60%", "70%", "75%", "80%", "90%", "100%" };
		static const int kShadeValues[] = { 0, 5, 10, 20, 25, 30, 40, 50, 60,
			70, 75, 80, 90, 100 };
		int shade = 0;
		for (int i = 0; i < 14; i++) {
			if (kShadeValues[i] == border.shading)
				shade = i;
		}
		fShading = ChoiceField("Shading:", kShades, 14, shade);
		fShadow = new BCheckBox("shadow", "Shadow", new BMessage(kMsgChanged));
		fShadow->SetValue(border.shadow);
		fSpace = PointsField("From Text:", border.space, false);
		fSpace->SetModificationMessage(new BMessage(kMsgChanged));

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BButton* apply = new BButton("apply", "Apply", new BMessage(kMsgApply));
		BMessage* none = new BMessage(kMsgPreset);
		none->AddInt32("preset", 0);
		BMessage* box = new BMessage(kMsgPreset);
		box->AddInt32("preset", 1);
		BMessage* shadowBox = new BMessage(kMsgPreset);
		shadowBox->AddInt32("preset", 2);

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGroup(B_HORIZONTAL)
					.Add(fPreview)
					.Add(LabeledBox("Border", sides))
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(new BButton("none", "None", none))
					.Add(new BButton("box", "Box", box))
					.Add(new BButton("shadowBox", "Shadow", shadowBox))
				.End()
				.AddGrid()
					.AddMenuField(fLine, 0, 0)
					.AddMenuField(fShading, 0, 1)
					.AddTextControl(fSpace, 0, 2)
					.Add(fShadow, 1, 3)
				.End()
			.End()
			.Add(ButtonColumn(ok, cancel, apply));
		SetDefaultButton(ok);
		_Update();
	}

	void _Update()
	{
		int line = ChoiceIndex(fLine) + 1;
		if (line < 1)
			line = kBorderSingle;
		uint8* targets[] = { &fBorder.top, &fBorder.bottom, &fBorder.left,
			&fBorder.right, &fBorder.between };
		for (int i = 0; i < 5; i++)
			*targets[i] = fSides[i]->Value() ? (uint8)line : kBorderNone;
		static const int kShadeValues[] = { 0, 5, 10, 20, 25, 30, 40, 50, 60,
			70, 75, 80, 90, 100 };
		int shade = ChoiceIndex(fShading);
		fBorder.shading = shade >= 0 ? kShadeValues[shade] : 0;
		fBorder.shadow = fShadow->Value() != 0;
		float space;
		if (ParseMeasure(fSpace->Text(), kUnitPoint, &space))
			fBorder.space = std::max(0.0f, space);
		fPreview->SetBorder(fBorder);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgPreset: {
				int32 preset = 0;
				message->FindInt32("preset", &preset);
				for (int i = 0; i < 4; i++)
					fSides[i]->SetValue(preset != 0);
				fSides[4]->SetValue(0);
				fShadow->SetValue(preset == 2);
				_Update();
				break;
			}
			case kMsgChanged:
				_Update();
				break;
			case kMsgOK:
			case kMsgApply: {
				_Update();
				BMessage result(kMsgApplyBorder);
				AddBorder(&result, fBorder);
				SendToParent(&result);
				if (message->what == kMsgOK)
					Quit();
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	ParaBorder		fBorder;
	BorderPreview*	fPreview;
	BCheckBox*		fSides[5];
	BMenuField*		fLine;
	BMenuField*		fShading;
	BCheckBox*		fShadow;
	BTextControl*	fSpace;
};


void
ShowBorderDialog(BWindow* parent, const ParaBorder& border)
{
	(new BorderDialog(parent, border))->Show();
}


// #pragma mark - Document


class DocumentDialog : public DialogWindow {
public:
	DocumentDialog(BWindow* parent, const DocSettings& settings, int unit)
		:
		DialogWindow(parent, "Document"),
		fSettings(settings),
		fUnit(unit)
	{
		fTop = MeasureField("Top:", settings.marginTop, unit, false);
		fBottom = MeasureField("Bottom:", settings.marginBottom, unit, false);
		fLeft = MeasureField("Left:", settings.marginLeft, unit, false);
		fRight = MeasureField("Right:", settings.marginRight, unit, false);
		fGutter = MeasureField("Gutter:", settings.gutter, unit, false);
		fMirror = new BCheckBox("mirror", "Mirror Even/Odd",
			new BMessage(kMsgChanged));
		fMirror->SetValue(settings.mirrorMargins);
		fTab = MeasureField("Default Tab Stops:", settings.defaultTab, unit,
			false);
		fWidow = new BCheckBox("widow", "Widow Control",
			new BMessage(kMsgChanged));
		fWidow->SetValue(settings.widowControl);
		fPrintHidden = new BCheckBox("printHidden", "Print Hidden Text",
			new BMessage(kMsgChanged));
		fPrintHidden->SetValue(settings.printHidden);
		static const char* kPositions[] = { "Bottom of Page",
			"Beneath Text", "End of Document" };
		fNotePosition = ChoiceField("Position:", kPositions, 3,
			settings.footnotePos);
		BString start;
		start << settings.footnoteStart;
		fNoteStart = new BTextControl("start", "Number From:", start.String(),
			nullptr);

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));

		BGridView* margins = new BGridView();
		BLayoutBuilder::Grid<>(margins)
			.AddTextControl(fTop, 0, 0)
			.AddTextControl(fLeft, 2, 0)
			.AddTextControl(fBottom, 0, 1)
			.AddTextControl(fRight, 2, 1)
			.AddTextControl(fGutter, 0, 2)
			.Add(fMirror, 2, 2, 2);
		BGridView* footnotes = new BGridView();
		BLayoutBuilder::Grid<>(footnotes)
			.AddMenuField(fNotePosition, 0, 0)
			.AddTextControl(fNoteStart, 0, 1);

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.Add(LabeledBox("Margins", margins))
				.AddGrid()
					.AddTextControl(fTab, 0, 0)
				.End()
				.Add(fWidow)
				.Add(fPrintHidden)
				.Add(LabeledBox("Footnotes", footnotes))
			.End()
			.Add(ButtonColumn(ok, cancel));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgOK: {
				DocSettings settings = fSettings;
				float value;
				struct Field {
					BTextControl*	control;
					float*			target;
				} fields[] = {
					{ fTop, &settings.marginTop },
					{ fBottom, &settings.marginBottom },
					{ fLeft, &settings.marginLeft },
					{ fRight, &settings.marginRight },
					{ fGutter, &settings.gutter },
					{ fTab, &settings.defaultTab }
				};
				for (const Field& field : fields) {
					if (ReadMeasure(field.control, fUnit, &value))
						*field.target = std::max(0.0f, value);
				}
				settings.defaultTab = std::max(4.5f, settings.defaultTab);
				if (settings.TextWidth() < 36 || settings.TextHeight() < 36) {
					(new BAlert("Document", "The margins leave no room for "
						"text.", "OK"))->Go(nullptr);
					break;
				}
				settings.mirrorMargins = fMirror->Value() != 0;
				settings.widowControl = fWidow->Value() != 0;
				settings.printHidden = fPrintHidden->Value() != 0;
				int position = ChoiceIndex(fNotePosition);
				if (position >= 0)
					settings.footnotePos = (uint8)position;
				settings.footnoteStart = std::max(0,
					atoi(fNoteStart->Text()));
				BMessage result(kMsgApplyDocument);
				AddSettings(&result, settings);
				SendToParent(&result);
				Quit();
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			case kMsgChanged:
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	DocSettings		fSettings;
	int				fUnit;
	BTextControl*	fTop;
	BTextControl*	fBottom;
	BTextControl*	fLeft;
	BTextControl*	fRight;
	BTextControl*	fGutter;
	BCheckBox*		fMirror;
	BTextControl*	fTab;
	BCheckBox*		fWidow;
	BCheckBox*		fPrintHidden;
	BMenuField*		fNotePosition;
	BTextControl*	fNoteStart;
};


void
ShowDocumentDialog(BWindow* parent, const DocSettings& settings, int unit)
{
	(new DocumentDialog(parent, settings, unit))->Show();
}


// #pragma mark - Section


class SectionDialog : public DialogWindow {
public:
	SectionDialog(BWindow* parent, const DocSettings& settings, int unit)
		:
		DialogWindow(parent, "Section"),
		fSettings(settings),
		fUnit(unit)
	{
		static const char* kStarts[] = { "No Break", "New Column",
			"New Page", "Even Page", "Odd Page" };
		fStart = ChoiceField("Section Start:", kStarts, 5,
			settings.sectionStart);
		BString columns;
		columns << settings.columns;
		fColumns = new BTextControl("columns", "Number:", columns.String(),
			nullptr);
		fSpacing = MeasureField("Spacing:", settings.columnSpacing, unit,
			false);
		fHeader = MeasureField("From Top:", settings.headerY, unit, false);
		fFooter = MeasureField("From Bottom:", settings.footerY, unit, false);
		fFirst = new BCheckBox("first", "Different First Page", nullptr);
		fFirst->SetValue(settings.titlePage);
		static const char* kFormats[] = { "1 2 3", "I II III", "i ii iii",
			"A B C", "a b c" };
		fFormat = ChoiceField("Format:", kFormats, 5,
			settings.pageNumberFormat);
		BString startAt;
		startAt << settings.pageNumberStart;
		fStartAt = new BTextControl("startAt", "Start At:", startAt.String(),
			nullptr);

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));

		BGridView* columnsGrid = new BGridView();
		BLayoutBuilder::Grid<>(columnsGrid)
			.AddTextControl(fColumns, 0, 0)
			.AddTextControl(fSpacing, 0, 1);
		BGridView* headerGrid = new BGridView();
		BLayoutBuilder::Grid<>(headerGrid)
			.AddTextControl(fHeader, 0, 0)
			.AddTextControl(fFooter, 0, 1)
			.Add(fFirst, 0, 2, 2);
		BGridView* numbers = new BGridView();
		BLayoutBuilder::Grid<>(numbers)
			.AddMenuField(fFormat, 0, 0)
			.AddTextControl(fStartAt, 0, 1);

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGrid()
					.AddMenuField(fStart, 0, 0)
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(LabeledBox("Columns", columnsGrid))
					.Add(LabeledBox("Header/Footer", headerGrid))
				.End()
				.Add(LabeledBox("Page Numbers", numbers))
			.End()
			.Add(ButtonColumn(ok, cancel));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgOK: {
				DocSettings settings = fSettings;
				int start = ChoiceIndex(fStart);
				if (start >= 0)
					settings.sectionStart = (uint8)start;
				settings.columns = std::max(1, std::min(12,
					atoi(fColumns->Text())));
				float value;
				if (ReadMeasure(fSpacing, fUnit, &value))
					settings.columnSpacing = std::max(0.0f, value);
				if (ReadMeasure(fHeader, fUnit, &value))
					settings.headerY = std::max(0.0f, value);
				if (ReadMeasure(fFooter, fUnit, &value))
					settings.footerY = std::max(0.0f, value);
				settings.titlePage = fFirst->Value() != 0;
				int format = ChoiceIndex(fFormat);
				if (format >= 0)
					settings.pageNumberFormat = (uint8)format;
				settings.pageNumberStart = atoi(fStartAt->Text());
				BMessage result(kMsgApplyDocument);
				AddSettings(&result, settings);
				SendToParent(&result);
				Quit();
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	DocSettings		fSettings;
	int				fUnit;
	BMenuField*		fStart;
	BTextControl*	fColumns;
	BTextControl*	fSpacing;
	BTextControl*	fHeader;
	BTextControl*	fFooter;
	BCheckBox*		fFirst;
	BMenuField*		fFormat;
	BTextControl*	fStartAt;
};


void
ShowSectionDialog(BWindow* parent, const DocSettings& settings, int unit)
{
	(new SectionDialog(parent, settings, unit))->Show();
}


// #pragma mark - Page Setup


struct PaperSize {
	const char*	name;
	float		width;
	float		height;
};

static const PaperSize kPapers[] = {
	{ "US Letter", 612, 792 },
	{ "US Legal", 612, 1008 },
	{ "A4", 595.3f, 841.9f },
	{ "A5", 419.5f, 595.3f },
	{ "B5", 498.9f, 708.7f },
	{ "Tabloid", 792, 1224 },
	{ "Executive", 522, 756 }
};
static const int kPaperCount = sizeof(kPapers) / sizeof(kPapers[0]);


class PageSetupDialog : public DialogWindow {
public:
	PageSetupDialog(BWindow* parent, const DocSettings& settings)
		:
		DialogWindow(parent, "Page Setup"),
		fSettings(settings)
	{
		gPrefsLock.Lock();
		fUnit = gPrefs.unit;
		gPrefsLock.Unlock();
		const char* names[kPaperCount + 1];
		int selected = kPaperCount;
		for (int i = 0; i < kPaperCount; i++) {
			names[i] = kPapers[i].name;
			if (fabs(kPapers[i].width - settings.pageWidth) < 1
				&& fabs(kPapers[i].height - settings.pageHeight) < 1)
				selected = i;
		}
		names[kPaperCount] = "Custom";
		fPaper = ChoiceField("Paper:", names, kPaperCount + 1, selected,
			kMsgPaper);
		fWidth = MeasureField("Width:", settings.pageWidth, fUnit, false);
		fHeight = MeasureField("Height:", settings.pageHeight, fUnit, false);
		fPortrait = new BRadioButton("portrait", "Portrait", nullptr);
		fLandscape = new BRadioButton("landscape", "Landscape", nullptr);
		(settings.landscape ? fLandscape : fPortrait)->SetValue(1);
		BString scale;
		scale << settings.scale;
		fScale = new BTextControl("scale", "Reduce or Enlarge (%):",
			scale.String(), nullptr);

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BButton* document = new BButton("document", "Document" B_UTF8_ELLIPSIS,
			new BMessage(kMsgOpenDocument));

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGrid()
					.AddMenuField(fPaper, 0, 0)
					.AddTextControl(fWidth, 0, 1)
					.AddTextControl(fHeight, 0, 2)
					.AddTextControl(fScale, 0, 3)
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(fPortrait)
					.Add(fLandscape)
				.End()
			.End()
			.Add(ButtonColumn(ok, cancel, document));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgPaper: {
				int32 index = 0;
				message->FindInt32("index", &index);
				if (index < kPaperCount) {
					fWidth->SetText(FormatMeasure(kPapers[index].width,
						fUnit).c_str());
					fHeight->SetText(FormatMeasure(kPapers[index].height,
						fUnit).c_str());
				}
				break;
			}
			case kMsgOpenDocument:
			case kMsgOK: {
				DocSettings settings = fSettings;
				float value;
				if (ReadMeasure(fWidth, fUnit, &value) && value >= 144)
					settings.pageWidth = value;
				if (ReadMeasure(fHeight, fUnit, &value) && value >= 144)
					settings.pageHeight = value;
				settings.landscape = fLandscape->Value() != 0;
				settings.scale = std::max(25, std::min(400,
					atoi(fScale->Text())));
				BMessage result(kMsgApplyDocument);
				AddSettings(&result, settings);
				SendToParent(&result);
				if (message->what == kMsgOpenDocument) {
					BMessage open(kMsgDocument);
					SendToParent(&open);
				}
				Quit();
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	DocSettings		fSettings;
	int				fUnit;
	BMenuField*		fPaper;
	BTextControl*	fWidth;
	BTextControl*	fHeight;
	BRadioButton*	fPortrait;
	BRadioButton*	fLandscape;
	BTextControl*	fScale;
};


void
ShowPageSetupDialog(BWindow* parent, const DocSettings& settings)
{
	(new PageSetupDialog(parent, settings))->Show();
}


// #pragma mark - Style


class StyleDialog : public DialogWindow {
public:
	StyleDialog(BWindow* parent, const StyleSheet& styles, int32 current,
		const CharFormat& selectionChar, const ParaFormat& selectionPara,
		int unit)
		:
		DialogWindow(parent, "Style"),
		fStyles(styles),
		fSelectionChar(selectionChar),
		fSelectionPara(selectionPara),
		fUnit(unit),
		fIndex(-1)
	{
		fName = new BTextControl("name", "Style:", "",
			new BMessage(kMsgChanged));
		fName->SetModificationMessage(new BMessage('dNam'));
		fList = new BListView("styles");
		fList->SetSelectionMessage(new BMessage(kMsgStyleSelected));
		fList->SetInvocationMessage(new BMessage(kMsgOK));
		BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true);
		scroll->SetExplicitMinSize(BSize(220, 160));
		fDescription = new BTextView("description");
		fDescription->MakeEditable(false);
		fDescription->SetWordWrap(true);
		fDescription->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		fDescription->SetExplicitMinSize(BSize(220, 50));
		fBasedOn = new BMenuField("basedOn", "Based On:", new BPopUpMenu(""));
		fNext = new BMenuField("next", "Next Style:", new BPopUpMenu(""));

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BButton* apply = new BButton("apply", "Apply", new BMessage(kMsgApply));
		BButton* define = new BButton("define", "Define",
			new BMessage(kMsgDefine));
		fDelete = new BButton("delete", "Delete", new BMessage(kMsgDelete));
		BButton* character = new BButton("character",
			"Character" B_UTF8_ELLIPSIS, new BMessage(kMsgOpenCharacter));
		BButton* paragraph = new BButton("paragraph",
			"Paragraph" B_UTF8_ELLIPSIS, new BMessage(kMsgOpenParagraph));
		BButton* tabs = new BButton("tabs", "Tabs" B_UTF8_ELLIPSIS,
			new BMessage(kMsgOpenTabs));
		BButton* border = new BButton("border", "Border" B_UTF8_ELLIPSIS,
			new BMessage(kMsgOpenBorder));

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.Add(fName)
				.Add(scroll)
				.Add(fDescription)
				.AddGrid()
					.AddMenuField(fBasedOn, 0, 0)
					.AddMenuField(fNext, 0, 1)
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(character)
					.Add(paragraph)
					.Add(tabs)
					.Add(border)
				.End()
			.End()
			.Add(ButtonColumn(ok, cancel, apply, define, fDelete));
		SetDefaultButton(ok);
		_FillList();
		if (current >= 0 && current < fStyles.Count())
			fList->Select(current);
		else
			_SelectNew();
	}

	void _FillList()
	{
		fList->MakeEmpty();
		for (const Style& style : fStyles.styles) {
			BString label;
			label << (style.automatic ? "\xE2\x80\xA2 " : "  ")
				<< style.name.c_str();
			fList->AddItem(new BStringItem(label.String()));
		}
	}

	void _FillMenus()
	{
		BMenu* menus[2] = { fBasedOn->Menu(), fNext->Menu() };
		int32 values[2] = { fStyle.basedOn, fStyle.next };
		for (int m = 0; m < 2; m++) {
			BMenu* menu = menus[m];
			while (menu->CountItems() > 0)
				delete menu->RemoveItem((int32)0);
			BMessage* none = new BMessage(kMsgChanged);
			none->AddInt32("index", -1);
			BMenuItem* noneItem = new BMenuItem(m == 0 ? "(no style)"
				: "(same style)", none);
			menu->AddItem(noneItem);
			if (values[m] < 0)
				noneItem->SetMarked(true);
			for (int32 i = 0; i < fStyles.Count(); i++) {
				BMessage* message = new BMessage(kMsgChanged);
				message->AddInt32("index", i);
				BMenuItem* item = new BMenuItem(fStyles.styles[i].name.c_str(),
					message);
				if (values[m] == i)
					item->SetMarked(true);
				menu->AddItem(item);
			}
		}
	}

	void _SelectNew()
	{
		fIndex = -1;
		fStyle = Style();
		fStyle.name = "";
		fStyle.cf = fSelectionChar;
		fStyle.cf.picture = 0;
		fStyle.cf.footnote = 0;
		fStyle.pf = fSelectionPara;
		fStyle.basedOn = 0;
		fStyle.next = -1;
		_FillMenus();
		_UpdateDescription();
		fDelete->SetEnabled(false);
	}

	void _UpdateDescription()
	{
		StyleSheet sheet = fStyles;
		int32 index = fIndex;
		if (index < 0) {
			sheet.styles.push_back(fStyle);
			index = sheet.Count() - 1;
		} else
			sheet.styles[index] = fStyle;
		fDescription->SetText(sheet.Describe(index, fUnit).c_str());
	}

	void _ReadMenus()
	{
		BMenuItem* based = fBasedOn->Menu()->FindMarked();
		BMenuItem* next = fNext->Menu()->FindMarked();
		fStyle.basedOn = based != nullptr
			? fBasedOn->Menu()->IndexOf(based) - 1 : -1;
		fStyle.next = next != nullptr ? fNext->Menu()->IndexOf(next) - 1 : -1;
	}

	void _Define(bool apply)
	{
		_ReadMenus();
		BString name = fName->Text();
		name.Trim();
		if (name.Length() == 0)
			return;
		fStyle.name = name.String();
		int32 existing = fStyles.Find(fStyle.name);
		if (fIndex < 0 && existing >= 0)
			fIndex = existing;
		BMessage message(kMsgStyleDefine);
		message.AddInt32("index", fIndex);
		message.AddString("name", fStyle.name.c_str());
		message.AddInt32("basedOn", fStyle.basedOn);
		message.AddInt32("next", fStyle.next);
		message.AddBool("apply", apply);
		AddCharFormat(&message, fStyle.cf);
		AddParaFormat(&message, fStyle.pf);
		SendToParent(&message);
		if (fIndex < 0) {
			fStyle.pf.style = fStyles.Count();
			fStyles.styles.push_back(fStyle);
			fIndex = fStyles.Count() - 1;
		} else
			fStyles.styles[fIndex] = fStyle;
		_FillList();
		fList->Select(fIndex);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgStyleSelected: {
				int32 index = fList->CurrentSelection();
				if (index < 0 || index >= fStyles.Count())
					break;
				fIndex = index;
				fStyle = fStyles.styles[index];
				fName->SetText(fStyle.name.c_str());
				_FillMenus();
				_UpdateDescription();
				fDelete->SetEnabled(!fStyle.automatic);
				break;
			}
			case 'dNam': {
				int32 existing = fStyles.Find(fName->Text());
				if (existing >= 0 && existing != fIndex)
					fList->Select(existing);
				else if (existing < 0 && fIndex >= 0) {
					fList->DeselectAll();
					BString name = fName->Text();
					_SelectNew();
					fName->SetText(name.String());
				}
				break;
			}
			case kMsgChanged:
				_ReadMenus();
				_UpdateDescription();
				break;
			case kMsgApplyCharacter: {
				uint32 mask = 0;
				CharFormat format = fStyle.cf;
				FindCharFormat(message, &format);
				message->FindUInt32("mask", &mask);
				CopyCharFields(fStyle.cf, format, mask);
				_UpdateDescription();
				break;
			}
			case kMsgApplyParagraph: {
				uint32 mask = 0;
				ParaFormat format = fStyle.pf;
				FindParaFormat(message, &format);
				message->FindUInt32("mask", &mask);
				CopyParaFields(fStyle.pf, format, mask & ~kPFStyle);
				_UpdateDescription();
				break;
			}
			case kMsgApplyTabs: {
				FindTabs(message, &fStyle.pf.tabs);
				_UpdateDescription();
				break;
			}
			case kMsgApplyBorder: {
				FindBorder(message, &fStyle.pf.border);
				_UpdateDescription();
				break;
			}
			case kMsgOpenCharacter:
				ShowCharacterDialog(this, fStyle.cf, kCFAll);
				break;
			case kMsgOpenParagraph:
				ShowParagraphDialog(this, fStyle.pf, kPFAll, fUnit);
				break;
			case kMsgOpenTabs:
				ShowTabsDialog(this, fStyle.pf, fUnit);
				break;
			case kMsgOpenBorder:
				ShowBorderDialog(this, fStyle.pf.border);
				break;
			case kMsgDefine:
				_Define(false);
				break;
			case kMsgApply:
			case kMsgOK: {
				bool changed = fIndex < 0
					|| !(fStyles.styles[fIndex] == fStyle);
				_ReadMenus();
				if (fIndex >= 0 && !changed) {
					BMessage apply(kMsgStyleApply);
					apply.AddInt32("index", fIndex);
					SendToParent(&apply);
				} else
					_Define(true);
				if (message->what == kMsgOK)
					Quit();
				break;
			}
			case kMsgDelete: {
				if (fIndex <= 0 || fIndex >= fStyles.Count()
					|| fStyles.styles[fIndex].automatic)
					break;
				BMessage remove(kMsgStyleDelete);
				remove.AddInt32("index", fIndex);
				SendToParent(&remove);
				fStyles.styles.erase(fStyles.styles.begin() + fIndex);
				for (Style& style : fStyles.styles) {
					if (style.basedOn == fIndex)
						style.basedOn = 0;
					else if (style.basedOn > fIndex)
						style.basedOn--;
					if (style.next == fIndex)
						style.next = -1;
					else if (style.next > fIndex)
						style.next--;
				}
				_FillList();
				fList->Select(0);
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	StyleSheet		fStyles;
	CharFormat		fSelectionChar;
	ParaFormat		fSelectionPara;
	int				fUnit;
	int32			fIndex;
	Style			fStyle;
	BTextControl*	fName;
	BListView*		fList;
	BTextView*		fDescription;
	BMenuField*		fBasedOn;
	BMenuField*		fNext;
	BButton*		fDelete;
};


void
ShowStyleDialog(BWindow* parent, const StyleSheet& styles, int32 current,
	const CharFormat& selectionChar, const ParaFormat& selectionPara,
	int unit)
{
	(new StyleDialog(parent, styles, current, selectionChar, selectionPara,
		unit))->Show();
}


// #pragma mark - Change Case


class ChangeCaseDialog : public DialogWindow {
public:
	ChangeCaseDialog(BWindow* parent)
		:
		DialogWindow(parent, "Change Case")
	{
		static const char* kModes[] = { "UPPERCASE", "lowercase",
			"Title Case", "Sentence case.", "tOGGLE cASE" };
		BGroupView* modes = new BGroupView(B_VERTICAL, 0);
		for (int i = 0; i < 5; i++) {
			fModes[i] = new BRadioButton(kModes[i], kModes[i], nullptr);
			modes->GroupLayout()->AddView(fModes[i]);
		}
		fModes[0]->SetValue(1);
		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(LabeledBox("Change Case", modes))
			.Add(ButtonColumn(ok, cancel));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgOK: {
				BMessage result(kMsgDoChangeCase);
				for (int i = 0; i < 5; i++) {
					if (fModes[i]->Value())
						result.AddInt32("mode", i);
				}
				SendToParent(&result);
				Quit();
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	BRadioButton*	fModes[5];
};


void
ShowChangeCaseDialog(BWindow* parent)
{
	(new ChangeCaseDialog(parent))->Show();
}


// #pragma mark - Table Layout


class TableLayoutDialog : public DialogWindow {
public:
	TableLayoutDialog(BWindow* parent, const RowFormat& row, int32 cell,
		int unit)
		:
		DialogWindow(parent, "Table Layout"),
		fUnit(unit)
	{
		cell = std::max<int32>(0, std::min<int32>(cell,
			(int32)row.cells.size() - 1));
		float width = row.cells.empty() ? 72
			: row.cells[cell].right - row.CellLeft(cell);
		fWidth = MeasureField("Column Width:", width, unit, false);
		fGap = MeasureField("Space Between Columns:", row.gap * 2, unit,
			false);
		fIndent = MeasureField("Indent Rows:", row.leftEdge + row.gap, unit,
			false);
		static const char* kAligns[] = { "Left", "Center", "Right" };
		fAlign = ChoiceField("Alignment:", kAligns, 3,
			std::min<int>(row.align, 2));

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		auto opButton = [](const char* label, const char* op) {
			BMessage* message = new BMessage(kMsgTableOp);
			message->AddString("op", op);
			return new BButton(op, label, message);
		};

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGrid()
					.AddTextControl(fWidth, 0, 0)
					.AddTextControl(fGap, 0, 1)
					.AddTextControl(fIndent, 0, 2)
					.AddMenuField(fAlign, 0, 3)
				.End()
				.AddGrid()
					.Add(opButton("Insert Row", "insertRow"), 0, 0)
					.Add(opButton("Delete Row", "deleteRow"), 1, 0)
					.Add(opButton("Insert Column", "insertColumn"), 0, 1)
					.Add(opButton("Delete Column", "deleteColumn"), 1, 1)
				.End()
			.End()
			.Add(ButtonColumn(ok, cancel));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgTableOp: {
				BMessage result(kMsgSetTableLayout);
				const char* op;
				if (message->FindString("op", &op) == B_OK)
					result.AddString("op", op);
				SendToParent(&result);
				Quit();
				break;
			}
			case kMsgOK: {
				BMessage result(kMsgSetTableLayout);
				result.AddString("op", "set");
				float value;
				if (ReadMeasure(fWidth, fUnit, &value) && value >= 9)
					result.AddFloat("width", value);
				if (ReadMeasure(fGap, fUnit, &value) && value >= 0)
					result.AddFloat("gap", value);
				if (ReadMeasure(fIndent, fUnit, &value))
					result.AddFloat("indent", value);
				int align = ChoiceIndex(fAlign);
				if (align >= 0)
					result.AddInt32("align", align);
				SendToParent(&result);
				Quit();
				break;
			}
			case kMsgCancel:
				Quit();
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

private:
	int				fUnit;
	BTextControl*	fWidth;
	BTextControl*	fGap;
	BTextControl*	fIndent;
	BMenuField*		fAlign;
};


void
ShowTableLayoutDialog(BWindow* parent, const RowFormat& row, int32 cell,
	int unit)
{
	(new TableLayoutDialog(parent, row, cell, unit))->Show();
}
