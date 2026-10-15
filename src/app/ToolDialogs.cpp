/*
 * Quill - dialogs of the Edit, View, Insert, Tools and Help menus.
 */
#include "Dialogs.h"

#include "App.h"
#include "Constants.h"
#include "Fonts.h"
#include "Icons.h"
#include "Prefs.h"
#include "Text.h"

#include <Alert.h>
#include <Beep.h>
#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <Clipboard.h>
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
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace quill;

enum {
	kMsgOK = 'tOK ',
	kMsgCancel = 'tCnl',
	kMsgChanged = 'tChg',
	kMsgSpecial = 'tSpc',
	kMsgListSelected = 'tLsl',
	kMsgListInvoked = 'tLiv',
	kMsgButton = 'tBtn',
	kMsgSymbolClicked = 'tSym',
	kMsgRemoveWord = 'tRmw'
};


static BGroupView*
Buttons(std::initializer_list<BButton*> buttons)
{
	BGroupView* group = new BGroupView(B_VERTICAL, B_USE_SMALL_SPACING);
	for (BButton* button : buttons) {
		button->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
		group->GroupLayout()->AddView(button);
	}
	group->GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
	return group;
}


// #pragma mark - Find and Replace


class FindDialog : public DialogWindow {
public:
	FindDialog(BWindow* parent, bool replace, const char* what,
		const char* with, bool matchCase, bool wholeWord)
		:
		DialogWindow(parent, replace ? "Replace" : "Find"),
		fReplace(replace)
	{
		SetFeel(B_FLOATING_SUBSET_WINDOW_FEEL);
		fWhat = new BTextControl("what", "Find What:", what, nullptr);
		fWith = new BTextControl("with", "Replace With:", with, nullptr);
		fWord = new BCheckBox("word", "Match Whole Word Only", nullptr);
		fWord->SetValue(wholeWord);
		fCase = new BCheckBox("case", "Match Case", nullptr);
		fCase->SetValue(matchCase);
		static const char* kDirections[] = { "Down", "Up" };
		BPopUpMenu* directions = new BPopUpMenu("");
		for (int i = 0; i < 2; i++) {
			BMenuItem* item = new BMenuItem(kDirections[i], nullptr);
			if (i == 0)
				item->SetMarked(true);
			directions->AddItem(item);
		}
		fDirection = new BMenuField("direction", "Search:", directions);

		BPopUpMenu* special = new BPopUpMenu("Special", false, false);
		struct Code {
			const char*	label;
			const char*	code;
		} codes[] = {
			{ "Tab Character", "^t" }, { "Paragraph Mark", "^p" },
			{ "New Line", "^n" }, { "Page or Section Break", "^d" },
			{ "Nonbreaking Space", "^s" }, { "Optional Hyphen", "^-" },
			{ "Nonbreaking Hyphen", "^~" }, { "Any Character", "^?" },
			{ "Any Digit", "^#" }, { "Any Letter", "^$" },
			{ "White Space", "^w" }
		};
		for (const Code& code : codes) {
			BMessage* message = new BMessage(kMsgSpecial);
			message->AddString("code", code.code);
			special->AddItem(new BMenuItem(code.label, message));
		}
		BMenuField* specialField = new BMenuField("special", nullptr, special);

		fFindNext = new BButton("findNext", "Find Next",
			new BMessage(kMsgFindNext));
		fReplaceButton = new BButton("replace", "Replace",
			new BMessage(kMsgReplaceOne));
		fReplaceAll = new BButton("replaceAll", "Replace All",
			new BMessage(kMsgReplaceAll));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGrid()
					.AddTextControl(fWhat, 0, 0)
					.AddTextControl(fWith, 0, 1)
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(fWord)
					.Add(fCase)
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(fDirection)
					.AddGlue()
					.Add(specialField)
				.End()
			.End()
			.Add(Buttons({ fFindNext, fReplaceButton, fReplaceAll, cancel }));
		fWhat->TextView()->SetExplicitMinSize(BSize(240, B_SIZE_UNSET));
		SetDefaultButton(fFindNext);
		_SetMode(replace);
		fWhat->MakeFocus(true);
		fWhat->TextView()->SelectAll();
	}

	void _SetMode(bool replace)
	{
		fReplace = replace;
		SetTitle(replace ? "Replace" : "Find");
		if (replace) {
			if (fWith->IsHidden(fWith)) {
				fWith->Show();
				fReplaceButton->Show();
				fReplaceAll->Show();
			}
		} else if (!fWith->IsHidden(fWith)) {
			fWith->Hide();
			fReplaceButton->Hide();
			fReplaceAll->Hide();
		}
	}

	void _Send(uint32 what)
	{
		BMessage message(what);
		message.AddString("what", fWhat->Text());
		message.AddString("with", fWith->Text());
		message.AddBool("case", fCase->Value() != 0);
		message.AddBool("word", fWord->Value() != 0);
		BMenuItem* direction = fDirection->Menu()->FindMarked();
		message.AddBool("up", direction != nullptr
			&& fDirection->Menu()->IndexOf(direction) == 1);
		SendToParent(&message);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgFindNext:
			case kMsgReplaceOne:
			case kMsgReplaceAll:
				_Send(message->what);
				break;
			case kMsgSpecial: {
				const char* code;
				if (message->FindString("code", &code) == B_OK) {
					BTextControl* target = fWith->TextView()->IsFocus()
						? fWith : fWhat;
					BTextView* text = target->TextView();
					text->Insert(code);
					target->MakeFocus(true);
				}
				break;
			}
			case 'Fmod': {
				bool replace = false;
				message->FindBool("replace", &replace);
				_SetMode(replace);
				Activate();
				break;
			}
			case kMsgCancel:
				PostMessage(B_QUIT_REQUESTED);
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

	virtual bool QuitRequested()
	{
		BMessage closed(kMsgFindClosed);
		SendToParent(&closed);
		return true;
	}

private:
	bool			fReplace;
	BTextControl*	fWhat;
	BTextControl*	fWith;
	BCheckBox*		fWord;
	BCheckBox*		fCase;
	BMenuField*		fDirection;
	BButton*		fFindNext;
	BButton*		fReplaceButton;
	BButton*		fReplaceAll;
};


BWindow*
ShowFindDialog(BWindow* parent, bool replace, const char* what,
	const char* with, bool matchCase, bool wholeWord)
{
	FindDialog* dialog = new FindDialog(parent, replace, what, with,
		matchCase, wholeWord);
	dialog->Show();
	return dialog;
}


// #pragma mark - simple dialogs


class NumberDialog : public DialogWindow {
public:
	NumberDialog(BWindow* parent, const char* title, const char* label,
		const char* value, uint32 what, const char* field, int32 minimum,
		int32 maximum)
		:
		DialogWindow(parent, title),
		fWhat(what),
		fField(field),
		fMinimum(minimum),
		fMaximum(maximum)
	{
		fValue = new BTextControl("value", label, value, nullptr);
		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.Add(fValue)
				.AddGlue()
			.End()
			.Add(Buttons({ ok, cancel }));
		SetDefaultButton(ok);
		fValue->MakeFocus(true);
		fValue->TextView()->SelectAll();
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgOK: {
				int32 value = atoi(fValue->Text());
				if (value < fMinimum || value > fMaximum) {
					beep();
					break;
				}
				BMessage result(fWhat);
				result.AddInt32(fField.String(), value);
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
	uint32			fWhat;
	BString			fField;
	int32			fMinimum;
	int32			fMaximum;
	BTextControl*	fValue;
};


void
ShowGoToDialog(BWindow* parent, int32 pageCount)
{
	BString label;
	label << "Go To Page (1 - " << pageCount << "):";
	(new NumberDialog(parent, "Go To", label.String(), "", kMsgGoToPage,
		"page", -1000, 100000))->Show();
}


class ZoomDialog : public DialogWindow {
public:
	ZoomDialog(BWindow* parent, int32 percent)
		:
		DialogWindow(parent, "Zoom")
	{
		static const int kPercents[] = { 200, 150, 125, 100, 75, 50 };
		BGroupView* choices = new BGroupView(B_VERTICAL, 0);
		for (int i = 0; i < 6; i++) {
			BString label;
			label << kPercents[i] << "%";
			BMessage* message = new BMessage(kMsgChanged);
			message->AddInt32("percent", kPercents[i]);
			fChoices[i] = new BRadioButton(label.String(), label.String(),
				message);
			fChoices[i]->SetValue(kPercents[i] == percent);
			choices->GroupLayout()->AddView(fChoices[i]);
		}
		BString value;
		value << percent;
		fCustom = new BTextControl("custom", "Percent:", value.String(),
			nullptr);
		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.Add(choices)
				.Add(fCustom)
			.End()
			.Add(Buttons({ ok, cancel }));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgChanged: {
				int32 percent;
				if (message->FindInt32("percent", &percent) == B_OK) {
					BString value;
					value << percent;
					fCustom->SetText(value.String());
				}
				break;
			}
			case kMsgOK: {
				int32 percent = atoi(fCustom->Text());
				if (percent < 25 || percent > 400) {
					beep();
					break;
				}
				BMessage result(kMsgSetZoom);
				result.AddInt32("percent", percent);
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
	BRadioButton*	fChoices[6];
	BTextControl*	fCustom;
};


void
ShowZoomDialog(BWindow* parent, int32 percent)
{
	(new ZoomDialog(parent, percent))->Show();
}


static float
ReadWidth(BTextControl* control, int unit)
{
	// empty or "Auto" means: divide the column among the cells
	BString text(control->Text());
	text.Trim();
	if (text.Length() == 0 || text.ICompare("auto") == 0)
		return 0;
	float width = 0;
	if (!ParseMeasure(text.String(), unit, &width))
		return -1;
	return width >= 9 ? width : -1;
}


class InsertTableDialog : public DialogWindow {
public:
	InsertTableDialog(BWindow* parent, int unit)
		:
		DialogWindow(parent, "Insert Table"),
		fUnit(unit)
	{
		fColumns = new BTextControl("columns", "Number of Columns:", "2",
			nullptr);
		fRows = new BTextControl("rows", "Number of Rows:", "1", nullptr);
		fWidth = new BTextControl("width", "Column Width:", "Auto", nullptr);
		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGrid()
				.AddTextControl(fColumns, 0, 0)
				.AddTextControl(fRows, 0, 1)
				.AddTextControl(fWidth, 0, 2)
			.End()
			.Add(Buttons({ ok, cancel }));
		SetDefaultButton(ok);
		fColumns->MakeFocus(true);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgOK: {
				int32 columns = atoi(fColumns->Text());
				int32 rows = atoi(fRows->Text());
				float width = ReadWidth(fWidth, fUnit);
				if (columns < 1 || columns > 31 || rows < 1 || rows > 200
					|| width < 0) {
					beep();
					break;
				}
				BMessage result(kMsgCreateTable);
				result.AddInt32("rows", rows);
				result.AddInt32("columns", columns);
				result.AddFloat("width", width);
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
	BTextControl*	fColumns;
	BTextControl*	fRows;
	BTextControl*	fWidth;
};


void
ShowInsertTableDialog(BWindow* parent, int unit)
{
	(new InsertTableDialog(parent, unit))->Show();
}


// Text to Table: like Insert Table, with the number of rows following
// from the selected text and the way it is divided into cells.
class TextToTableDialog : public DialogWindow {
public:
	TextToTableDialog(BWindow* parent, const TextTableAnalysis& analysis,
		int unit)
		:
		DialogWindow(parent, "Text to Table"),
		fAnalysis(analysis),
		fUnit(unit)
	{
		fColumns = new BTextControl("columns", "Number of Columns:", "",
			new BMessage(kMsgChanged));
		fColumns->SetModificationMessage(new BMessage(kMsgChanged));
		fRowsLabel = new BStringView("rowsLabel", "Number of Rows:");
		fRows = new BStringView("rows", "");
		fWidth = new BTextControl("width", "Column Width:", "Auto", nullptr);
		static const char* kFormats[] = { "Paragraphs", "Tab Delimited",
			"Comma Delimited" };
		BGroupView* formats = new BGroupView(B_VERTICAL, 0);
		for (int i = 0; i < 3; i++) {
			BMessage* message = new BMessage(kMsgButton);
			message->AddInt32("format", i);
			fFormats[i] = new BRadioButton(kFormats[i], kFormats[i], message);
			formats->GroupLayout()->AddView(fFormats[i]);
		}
		BBox* box = new BBox("convertFrom");
		box->SetLabel("Convert From");
		BGroupLayout* boxLayout = new BGroupLayout(B_VERTICAL, 0);
		box->SetLayout(boxLayout);
		boxLayout->SetInsets(B_USE_SMALL_SPACING, B_USE_BIG_SPACING,
			B_USE_SMALL_SPACING, B_USE_SMALL_SPACING);
		boxLayout->AddView(formats);

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGrid()
					.AddTextControl(fColumns, 0, 0)
					.Add(fRowsLabel, 0, 1)
					.Add(fRows, 1, 1)
					.AddTextControl(fWidth, 0, 2)
				.End()
				.Add(box)
			.End()
			.Add(Buttons({ ok, cancel }));
		SetDefaultButton(ok);
		_SetFormat(analysis.suggested);
		fColumns->MakeFocus(true);
	}

	void _SetFormat(int format)
	{
		fFormat = format;
		fFormats[format]->SetValue(B_CONTROL_ON);
		BString columns;
		columns << TextTableColumns(fAnalysis, format);
		fColumns->SetText(columns.String());
		_UpdateRows();
	}

	void _UpdateRows()
	{
		int32 columns = atoi(fColumns->Text());
		BString rows;
		if (columns >= 1 && columns <= 31)
			rows << TextTableRows(fAnalysis, fFormat, columns);
		fRows->SetText(rows.String());
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgButton: {
				int32 format = kTableParagraphs;
				message->FindInt32("format", &format);
				_SetFormat(format);
				break;
			}
			case kMsgChanged:
				_UpdateRows();
				break;
			case kMsgOK: {
				int32 columns = atoi(fColumns->Text());
				float width = ReadWidth(fWidth, fUnit);
				if (columns < 1 || columns > 31 || width < 0) {
					beep();
					break;
				}
				BMessage result(kMsgTextToTable);
				result.AddInt32("format", fFormat);
				result.AddInt32("columns", columns);
				result.AddFloat("width", width);
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
	TextTableAnalysis fAnalysis;
	int				fUnit;
	int				fFormat;
	BTextControl*	fColumns;
	BStringView*	fRowsLabel;
	BStringView*	fRows;
	BTextControl*	fWidth;
	BRadioButton*	fFormats[3];
};


void
ShowTextToTableDialog(BWindow* parent, const TextTableAnalysis& analysis,
	int unit)
{
	(new TextToTableDialog(parent, analysis, unit))->Show();
}


class TableToTextDialog : public DialogWindow {
public:
	TableToTextDialog(BWindow* parent)
		:
		DialogWindow(parent, "Table to Text")
	{
		static const char* kFormats[] = { "Paragraphs", "Tab Delimited",
			"Comma Delimited" };
		BGroupView* formats = new BGroupView(B_VERTICAL, 0);
		for (int i = 0; i < 3; i++) {
			fFormats[i] = new BRadioButton(kFormats[i], kFormats[i], nullptr);
			formats->GroupLayout()->AddView(fFormats[i]);
		}
		fFormats[kTableTabs]->SetValue(B_CONTROL_ON);
		BBox* box = new BBox("convertTo");
		box->SetLabel("Convert To");
		BGroupLayout* boxLayout = new BGroupLayout(B_VERTICAL, 0);
		box->SetLayout(boxLayout);
		boxLayout->SetInsets(B_USE_SMALL_SPACING, B_USE_BIG_SPACING,
			B_USE_SMALL_SPACING, B_USE_SMALL_SPACING);
		boxLayout->AddView(formats);
		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(box)
			.Add(Buttons({ ok, cancel }));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgOK: {
				BMessage result(kMsgTableToText);
				for (int i = 0; i < 3; i++) {
					if (fFormats[i]->Value() == B_CONTROL_ON)
						result.AddInt32("format", i);
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
	BRadioButton*	fFormats[3];
};


void
ShowTableToTextDialog(BWindow* parent)
{
	(new TableToTextDialog(parent))->Show();
}


// #pragma mark - Symbol


class SymbolGrid : public BView {
public:
	SymbolGrid(const char* font)
		:
		BView("symbols", B_WILL_DRAW),
		fSelected(-1)
	{
		for (uint32 c = 0x21; c < 0x7F; c++)
			fChars.push_back(c);
		for (uint32 c = 0xA1; c <= 0xFF; c++) {
			if (c != 0xAD)
				fChars.push_back(c);
		}
		static const uint32 kExtra[] = { 0x152, 0x153, 0x160, 0x161, 0x178,
			0x17D, 0x192, 0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C,
			0x201D, 0x201E, 0x2020, 0x2021, 0x2022, 0x2026, 0x2030, 0x2039,
			0x203A, 0x20AC, 0x2122, 0x2190, 0x2191, 0x2192, 0x2193, 0x2194,
			0x21D2, 0x21D4, 0x2200, 0x2202, 0x2203, 0x2205, 0x2206, 0x2207,
			0x2208, 0x220F, 0x2211, 0x221A, 0x221E, 0x2220, 0x2227, 0x2228,
			0x2229, 0x222A, 0x222B, 0x2234, 0x2248, 0x2260, 0x2261, 0x2264,
			0x2265, 0x2282, 0x2283, 0x2286, 0x2287, 0x2295, 0x2297, 0x22A5,
			0x25CA, 0x2660, 0x2663, 0x2665, 0x2666, 0x2713, 0x2717,
			0x391, 0x392, 0x393, 0x394, 0x395, 0x396, 0x397, 0x398, 0x399,
			0x39A, 0x39B, 0x39C, 0x39D, 0x39E, 0x39F, 0x3A0, 0x3A1, 0x3A3,
			0x3A4, 0x3A5, 0x3A6, 0x3A7, 0x3A8, 0x3A9, 0x3B1, 0x3B2, 0x3B3,
			0x3B4, 0x3B5, 0x3B6, 0x3B7, 0x3B8, 0x3B9, 0x3BA, 0x3BB, 0x3BC,
			0x3BD, 0x3BE, 0x3BF, 0x3C0, 0x3C1, 0x3C2, 0x3C3, 0x3C4, 0x3C5,
			0x3C6, 0x3C7, 0x3C8, 0x3C9 };
		for (uint32 c : kExtra)
			fChars.push_back(c);
		CharFormat format;
		format.font = font;
		fFont = FontCache::Get().FontFor(format, 18);
		fCell = 26;
		fColumns = 24;
		int rows = ((int)fChars.size() + fColumns - 1) / fColumns;
		SetExplicitMinSize(BSize(fColumns * fCell, rows * fCell));
		SetExplicitMaxSize(BSize(fColumns * fCell, rows * fCell));
		SetViewColor(255, 255, 255);
	}

	virtual void Draw(BRect update)
	{
		(void)update;
		SetFont(&fFont);
		font_height height;
		fFont.GetHeight(&height);
		for (size_t i = 0; i < fChars.size(); i++) {
			int column = (int)i % fColumns;
			int row = (int)i / fColumns;
			BRect cell(column * fCell, row * fCell, (column + 1) * fCell - 1,
				(row + 1) * fCell - 1);
			if ((int)i == fSelected) {
				SetHighColor(0, 0, 0);
				FillRect(cell);
				SetHighColor(255, 255, 255);
			} else {
				SetHighColor(220, 220, 220);
				StrokeRect(cell);
				SetHighColor(0, 0, 0);
			}
			std::string text = Utf8(fChars[i]);
			float width = fFont.StringWidth(text.c_str());
			SetDrawingMode(B_OP_OVER);
			DrawString(text.c_str(), BPoint(cell.left + (fCell - width) / 2,
				cell.top + (fCell + height.ascent - height.descent) / 2));
			SetDrawingMode(B_OP_COPY);
		}
	}

	virtual void MouseDown(BPoint where)
	{
		int column = (int)(where.x / fCell);
		int row = (int)(where.y / fCell);
		int index = row * fColumns + column;
		if (column < 0 || column >= fColumns || index < 0
			|| index >= (int)fChars.size())
			return;
		fSelected = index;
		Invalidate();
		int32 clicks = 1;
		Window()->CurrentMessage()->FindInt32("clicks", &clicks);
		BMessage message(kMsgSymbolClicked);
		message.AddInt32("clicks", clicks);
		message.AddString("text", Utf8(fChars[index]).c_str());
		message.AddInt32("code", (int32)fChars[index]);
		Window()->PostMessage(&message);
	}

	std::string Selected() const
	{
		return fSelected >= 0 ? Utf8(fChars[fSelected]) : std::string();
	}

private:
	std::vector<uint32>	fChars;
	BFont				fFont;
	int					fCell;
	int					fColumns;
	int					fSelected;
};


class SymbolDialog : public DialogWindow {
public:
	SymbolDialog(BWindow* parent, const char* font)
		:
		DialogWindow(parent, "Symbol")
	{
		fGrid = new SymbolGrid(font);
		fInfo = new BStringView("info", "Click a character, then Insert.");
		BButton* insert = new BButton("insert", "Insert", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Close",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(fGrid)
			.AddGroup(B_HORIZONTAL)
				.Add(fInfo)
				.AddGlue()
				.Add(insert)
				.Add(cancel)
			.End();
		SetDefaultButton(insert);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgSymbolClicked: {
				int32 code = 0;
				message->FindInt32("code", &code);
				BString info;
				info.SetToFormat("Character U+%04X", (unsigned)code);
				fInfo->SetText(info.String());
				int32 clicks = 1;
				message->FindInt32("clicks", &clicks);
				if (clicks >= 2)
					PostMessage(kMsgOK);
				break;
			}
			case kMsgOK: {
				std::string text = fGrid->Selected();
				if (text.empty())
					break;
				BMessage result(kMsgInsertChar);
				result.AddString("text", text.c_str());
				SendToParent(&result);
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
	SymbolGrid*		fGrid;
	BStringView*	fInfo;
};


void
ShowSymbolDialog(BWindow* parent, const char* font)
{
	SymbolDialog* dialog = new SymbolDialog(parent, font);
	dialog->SetFeel(B_FLOATING_SUBSET_WINDOW_FEEL);
	dialog->Show();
}


// #pragma mark - Word Count


class WordCountDialog : public DialogWindow {
public:
	WordCountDialog(BWindow* parent, const WordCount& count, bool selection)
		:
		DialogWindow(parent, "Word Count")
	{
		BGridView* grid = new BGridView();
		BLayoutBuilder::Grid<> builder(grid);
		struct Row {
			const char*	label;
			int64		value;
		} rows[] = {
			{ "Characters:", count.characters },
			{ "Words:", count.words },
			{ "Lines:", count.lines },
			{ "Paragraphs:", count.paragraphs }
		};
		int row = 0;
		for (const Row& entry : rows) {
			BString value;
			value << entry.value;
			builder.Add(new BStringView("label", entry.label), 0, row);
			builder.Add(new BStringView("value", value.String()), 1, row);
			row++;
		}
		if (!selection && (count.footnoteWords > 0
				|| count.footnoteCharacters > 0)) {
			BString value;
			value << count.footnoteWords << " words, "
				<< count.footnoteCharacters << " characters";
			builder.Add(new BStringView("label", "Footnotes:"), 0, row);
			builder.Add(new BStringView("value", value.String()), 1, row);
		}
		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.Add(new BStringView("scope", selection ? "In the selection:"
					: "In the document:"))
				.Add(grid)
			.End()
			.Add(Buttons({ ok }));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what == kMsgOK || message->what == kMsgCancel)
			Quit();
		else
			DialogWindow::MessageReceived(message);
	}
};


void
ShowWordCountDialog(BWindow* parent, const WordCount& count, bool selection)
{
	(new WordCountDialog(parent, count, selection))->Show();
}


// #pragma mark - Renumber


class RenumberDialog : public DialogWindow {
public:
	RenumberDialog(BWindow* parent)
		:
		DialogWindow(parent, "Renumber")
	{
		fAll = new BRadioButton("all", "All", nullptr);
		fOnly = new BRadioButton("only", "Only If Already Numbered", nullptr);
		fAll->SetValue(1);
		fStart = new BTextControl("start", "Start At:", "1", nullptr);
		BPopUpMenu* formats = new BPopUpMenu("");
		static const char* kFormats[] = { "1.", "1)", "I.", "i.", "A.", "a.",
			"1.1 (by heading level)" };
		for (int i = 0; i < 7; i++) {
			BMenuItem* item = new BMenuItem(kFormats[i], nullptr);
			if (i == 0)
				item->SetMarked(true);
			formats->AddItem(item);
		}
		fFormat = new BMenuField("format", "Format:", formats);
		fRemove = new BCheckBox("remove", "Remove Numbers", nullptr);
		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL, 0)
				.Add(new BStringView("paragraphs", "Paragraphs:"))
				.Add(fAll)
				.Add(fOnly)
				.AddStrut(B_USE_SMALL_SPACING)
				.AddGrid()
					.AddTextControl(fStart, 0, 0)
					.AddMenuField(fFormat, 0, 1)
				.End()
				.Add(fRemove)
			.End()
			.Add(Buttons({ ok, cancel }));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgOK: {
				BMessage result(kMsgDoRenumber);
				result.AddBool("onlyNumbered", fOnly->Value() != 0);
				result.AddInt32("start", std::max(0, atoi(fStart->Text())));
				BMenuItem* item = fFormat->Menu()->FindMarked();
				result.AddInt32("format", item != nullptr
					? fFormat->Menu()->IndexOf(item) : 0);
				result.AddBool("remove", fRemove->Value() != 0);
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
	BRadioButton*	fAll;
	BRadioButton*	fOnly;
	BTextControl*	fStart;
	BMenuField*		fFormat;
	BCheckBox*		fRemove;
};


void
ShowRenumberDialog(BWindow* parent)
{
	(new RenumberDialog(parent))->Show();
}


// #pragma mark - Summary Info


class SummaryDialog : public DialogWindow {
public:
	SummaryDialog(BWindow* parent, const DocInfo& info, const char* name)
		:
		DialogWindow(parent, "Summary Info")
	{
		gPrefsLock.Lock();
		std::string author = info.author.empty() ? gPrefs.userName
			: info.author;
		gPrefsLock.Unlock();
		fTitle = new BTextControl("title", "Title:", info.title.c_str(),
			nullptr);
		fSubject = new BTextControl("subject", "Subject:",
			info.subject.c_str(), nullptr);
		fAuthor = new BTextControl("author", "Author:", author.c_str(),
			nullptr);
		fVersion = new BTextControl("version", "Version:",
			info.version.c_str(), nullptr);
		fKeywords = new BTextControl("keywords", "Keywords:",
			info.keywords.c_str(), nullptr);
		fComments = new BTextControl("comments", "Comments:",
			info.comments.c_str(), nullptr);
		fTitle->TextView()->SetExplicitMinSize(BSize(300, B_SIZE_UNSET));
		BString dates;
		char buffer[64];
		if (info.created != 0) {
			struct tm parts;
			localtime_r(&info.created, &parts);
			strftime(buffer, sizeof(buffer), "%x %X", &parts);
			dates << "Created: " << buffer;
		}
		if (info.revised != 0) {
			struct tm parts;
			localtime_r(&info.revised, &parts);
			strftime(buffer, sizeof(buffer), "%x %X", &parts);
			if (dates.Length() > 0)
				dates << "    ";
			dates << "Last saved: " << buffer;
		}
		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.Add(new BStringView("name", name))
				.AddGrid()
					.AddTextControl(fTitle, 0, 0)
					.AddTextControl(fSubject, 0, 1)
					.AddTextControl(fAuthor, 0, 2)
					.AddTextControl(fVersion, 0, 3)
					.AddTextControl(fKeywords, 0, 4)
					.AddTextControl(fComments, 0, 5)
				.End()
				.Add(new BStringView("dates", dates.String()))
			.End()
			.Add(Buttons({ ok, cancel }));
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgOK: {
				BMessage result(kMsgSetSummary);
				result.AddString("title", fTitle->Text());
				result.AddString("subject", fSubject->Text());
				result.AddString("author", fAuthor->Text());
				result.AddString("version", fVersion->Text());
				result.AddString("keywords", fKeywords->Text());
				result.AddString("comments", fComments->Text());
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
	BTextControl*	fTitle;
	BTextControl*	fSubject;
	BTextControl*	fAuthor;
	BTextControl*	fVersion;
	BTextControl*	fKeywords;
	BTextControl*	fComments;
};


void
ShowSummaryInfoDialog(BWindow* parent, const DocInfo& info,
	const char* fileName)
{
	(new SummaryDialog(parent, info, fileName))->Show();
}


// #pragma mark - Glossary


class GlossaryDialog : public DialogWindow {
public:
	GlossaryDialog(BWindow* parent, bool hasSelection)
		:
		DialogWindow(parent, "Glossary")
	{
		fName = new BTextControl("name", "Name:", "", nullptr);
		fList = new BListView("entries");
		fList->SetSelectionMessage(new BMessage(kMsgListSelected));
		fList->SetInvocationMessage(new BMessage(kMsgOK));
		BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true);
		scroll->SetExplicitMinSize(BSize(220, 180));
		fDefine = new BButton("define", "Define", new BMessage('gDef'));
		fDefine->SetEnabled(hasSelection);
		fDelete = new BButton("delete", "Delete", new BMessage('gDel'));
		BButton* insert = new BButton("insert", "Insert", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.Add(fName)
				.Add(scroll)
			.End()
			.Add(Buttons({ insert, fDefine, fDelete, cancel }));
		SetDefaultButton(insert);
		_Fill();
	}

	void _Fill()
	{
		fList->MakeEmpty();
		static const char* kStandard[] = { "date", "page number",
			"print date", "print time", "time" };
		gPrefsLock.Lock();
		for (const auto& entry : gPrefs.glossary)
			fList->AddItem(new BStringItem(entry.first.c_str()));
		gPrefsLock.Unlock();
		for (const char* name : kStandard)
			fList->AddItem(new BStringItem(name));
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgListSelected: {
				BStringItem* item = dynamic_cast<BStringItem*>(
					fList->ItemAt(fList->CurrentSelection()));
				if (item != nullptr)
					fName->SetText(item->Text());
				break;
			}
			case kMsgOK: {
				if (fName->Text()[0] == '\0')
					break;
				BMessage result(kMsgGlossaryInsert);
				result.AddString("name", fName->Text());
				SendToParent(&result);
				Quit();
				break;
			}
			case 'gDef': {
				if (fName->Text()[0] == '\0') {
					beep();
					break;
				}
				BMessage result(kMsgGlossaryDefine);
				result.AddString("name", fName->Text());
				SendToParent(&result);
				// the window stores the entry; show it right away
				gPrefsLock.Lock();
				if (gPrefs.glossary.find(fName->Text())
						== gPrefs.glossary.end())
					gPrefs.glossary[fName->Text()] = "";
				gPrefsLock.Unlock();
				_Fill();
				break;
			}
			case 'gDel': {
				BMessage result(kMsgGlossaryDelete);
				result.AddString("name", fName->Text());
				SendToParent(&result);
				gPrefsLock.Lock();
				gPrefs.glossary.erase(fName->Text());
				gPrefsLock.Unlock();
				_Fill();
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
	BTextControl*	fName;
	BListView*		fList;
	BButton*		fDefine;
	BButton*		fDelete;
};


void
ShowGlossaryDialog(BWindow* parent, bool hasSelection)
{
	(new GlossaryDialog(parent, hasSelection))->Show();
}


// #pragma mark - Spelling


class SpellingDialog : public DialogWindow {
public:
	SpellingDialog(BWindow* parent)
		:
		DialogWindow(parent, "Spelling")
	{
		SetFeel(B_FLOATING_SUBSET_WINDOW_FEEL);
		fWord = new BStringView("word", "Checking" B_UTF8_ELLIPSIS);
		BFont bold(be_bold_font);
		fWord->SetFont(&bold);
		fChange = new BTextControl("change", "Change To:", "", nullptr);
		fChange->TextView()->SetExplicitMinSize(BSize(200, B_SIZE_UNSET));
		fList = new BListView("suggestions");
		fList->SetSelectionMessage(new BMessage(kMsgListSelected));
		fList->SetInvocationMessage(new BMessage(kMsgSpellChange));
		BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true);
		scroll->SetExplicitMinSize(BSize(200, 140));

		fIgnore = new BButton("ignore", "Ignore", new BMessage(kMsgSpellIgnore));
		fIgnoreAll = new BButton("ignoreAll", "Ignore All",
			new BMessage(kMsgSpellIgnoreAll));
		fChangeButton = new BButton("changeButton", "Change",
			new BMessage(kMsgSpellChange));
		fChangeAll = new BButton("changeAll", "Change All",
			new BMessage(kMsgSpellChangeAll));
		fAdd = new BButton("add", "Add", new BMessage(kMsgSpellAdd));
		fSuggest = new BButton("suggest", "Suggest",
			new BMessage(kMsgSpellSuggest));
		BButton* cancel = new BButton("cancel", "Close",
			new BMessage(kMsgCancel));

		BLayoutBuilder::Group<>(this, B_HORIZONTAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGroup(B_HORIZONTAL)
					.Add(new BStringView("label", "Not in Dictionary:"))
					.Add(fWord)
					.AddGlue()
				.End()
				.Add(fChange)
				.Add(new BStringView("suggestions", "Suggestions:"))
				.Add(scroll)
			.End()
			.Add(Buttons({ fIgnore, fIgnoreAll, fChangeButton, fChangeAll,
				fAdd, fSuggest, cancel }));
		SetDefaultButton(fChangeButton);
		_Enable(false);
	}

	virtual void Show()
	{
		DialogWindow::Show();
		BMessage start(kMsgSpellStart);
		SendToParent(&start);
	}

	void _Enable(bool enabled)
	{
		fIgnore->SetEnabled(enabled);
		fIgnoreAll->SetEnabled(enabled);
		fChangeButton->SetEnabled(enabled);
		fChangeAll->SetEnabled(enabled);
		fAdd->SetEnabled(enabled);
		fSuggest->SetEnabled(enabled);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgSpellState: {
				const char* word = "";
				message->FindString("word", &word);
				bool suggestOnly = false;
				message->FindBool("suggestOnly", &suggestOnly);
				if (!suggestOnly) {
					fWord->SetText(word);
					fChange->SetText(word);
				}
				fList->MakeEmpty();
				const char* suggestion;
				for (int32 i = 0; message->FindString("suggestion", i,
						&suggestion) == B_OK; i++)
					fList->AddItem(new BStringItem(suggestion));
				if (fList->CountItems() == 0)
					fList->AddItem(new BStringItem("(no suggestions)"));
				else if (!suggestOnly)
					fList->Select(0);
				_Enable(true);
				break;
			}
			case kMsgSpellDone:
				fWord->SetText("");
				fChange->SetText("");
				fList->MakeEmpty();
				_Enable(false);
				(new BAlert("Spelling", "The spelling check is complete.",
					"OK"))->Go(nullptr);
				break;
			case kMsgListSelected: {
				BStringItem* item = dynamic_cast<BStringItem*>(
					fList->ItemAt(fList->CurrentSelection()));
				if (item != nullptr && item->Text()[0] != '(')
					fChange->SetText(item->Text());
				break;
			}
			case kMsgSpellIgnore:
			case kMsgSpellIgnoreAll:
			case kMsgSpellAdd:
			case kMsgSpellChange:
			case kMsgSpellChangeAll:
			case kMsgSpellSuggest: {
				BMessage command(message->what);
				command.AddString("word", fChange->Text());
				SendToParent(&command);
				if (message->what != kMsgSpellSuggest)
					_Enable(false);
				break;
			}
			case kMsgCancel:
				PostMessage(B_QUIT_REQUESTED);
				break;
			default:
				DialogWindow::MessageReceived(message);
				break;
		}
	}

	virtual bool QuitRequested()
	{
		BMessage closed(kMsgSpellClosed);
		SendToParent(&closed);
		return true;
	}

private:
	BStringView*	fWord;
	BTextControl*	fChange;
	BListView*		fList;
	BButton*		fIgnore;
	BButton*		fIgnoreAll;
	BButton*		fChangeButton;
	BButton*		fChangeAll;
	BButton*		fAdd;
	BButton*		fSuggest;
};


BWindow*
ShowSpellingDialog(BWindow* parent)
{
	SpellingDialog* dialog = new SpellingDialog(parent);
	dialog->Show();
	return dialog;
}


// #pragma mark - Preferences


class PreferencesDialog : public DialogWindow {
public:
	PreferencesDialog(BWindow* parent)
		:
		DialogWindow(parent, "Preferences")
	{
		gPrefsLock.Lock();
		Prefs prefs = gPrefs;
		gPrefsLock.Unlock();

		fName = new BTextControl("name", "Your Name:", prefs.userName.c_str(),
			nullptr);
		static const char* kUnits[] = { "Inch", "Cm", "Points", "Picas" };
		BPopUpMenu* units = new BPopUpMenu("");
		for (int i = 0; i < 4; i++) {
			BMenuItem* item = new BMenuItem(kUnits[i], nullptr);
			item->SetMarked(i == prefs.unit);
			units->AddItem(item);
		}
		fUnit = new BMenuField("unit", "Measurement Unit:", units);
		fSmartQuotes = new BCheckBox("smartQuotes",
			"Use Smart Quotes (\xE2\x80\x9C\xE2\x80\x9D and "
			"\xE2\x80\x98\xE2\x80\x99)", nullptr);
		fSmartQuotes->SetValue(prefs.smartQuotes);
		BGroupView* general = new BGroupView("General", B_VERTICAL);
		BLayoutBuilder::Group<>(general)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.AddGrid()
				.AddTextControl(fName, 0, 0)
				.AddMenuField(fUnit, 0, 1)
			.End()
			.Add(fSmartQuotes)
			.AddGlue();

		fHidden = new BCheckBox("hidden", "Show Hidden Text", nullptr);
		fHidden->SetValue(prefs.showHidden);
		fBoundaries = new BCheckBox("boundaries",
			"Show Text Boundaries in Page Layout View", nullptr);
		fBoundaries->SetValue(prefs.textBoundaries);
		fGridlines = new BCheckBox("gridlines", "Show Table Gridlines",
			nullptr);
		fGridlines->SetValue(prefs.tableGridlines);
		fPageView = new BCheckBox("pageView",
			"Open Documents in Page Layout View", nullptr);
		fPageView->SetValue(prefs.openInPageView);
		BGroupView* view = new BGroupView("View", B_VERTICAL, 0);
		BLayoutBuilder::Group<>(view)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.Add(fHidden)
			.Add(fBoundaries)
			.Add(fGridlines)
			.Add(fPageView)
			.AddGlue();

		fBackup = new BCheckBox("backup", "Always Make Backup", nullptr);
		fBackup->SetValue(prefs.makeBackup);
		BPopUpMenu* fonts = new BPopUpMenu("");
		std::string defaultFont = prefs.defaultFont.empty() ? gDefaultSerif
			: prefs.defaultFont;
		for (const std::string& family : FontCache::Get().Families()) {
			BMenuItem* item = new BMenuItem(family.c_str(), nullptr);
			item->SetMarked(family == defaultFont);
			fonts->AddItem(item);
		}
		fFont = new BMenuField("font", "Default Font:", fonts);
		BString size;
		size << (int)prefs.defaultSize;
		fSize = new BTextControl("size", "Default Size:", size.String(),
			nullptr);
		BGroupView* save = new BGroupView("Open and Save", B_VERTICAL);
		BLayoutBuilder::Group<>(save)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.Add(fBackup)
			.AddGrid()
				.AddMenuField(fFont, 0, 0)
				.AddTextControl(fSize, 0, 1)
			.End()
			.AddGlue();

		fUppercase = new BCheckBox("uppercase", "Ignore Words in UPPERCASE",
			nullptr);
		fUppercase->SetValue(prefs.ignoreUppercase);
		fNumbers = new BCheckBox("numbers", "Ignore Words with Numbers",
			nullptr);
		fNumbers->SetValue(prefs.ignoreNumbers);
		fWords = new BListView("words");
		for (const std::string& word : prefs.customWords)
			fWords->AddItem(new BStringItem(word.c_str()));
		BScrollView* scroll = new BScrollView("scroll", fWords, 0, false,
			true);
		scroll->SetExplicitMinSize(BSize(200, 100));
		BGroupView* spelling = new BGroupView("Spelling", B_VERTICAL);
		BLayoutBuilder::Group<>(spelling)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.Add(fUppercase)
			.Add(fNumbers)
			.Add(new BStringView("custom", "Custom Dictionary:"))
			.Add(scroll)
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(new BButton("remove", "Remove Word",
					new BMessage(kMsgRemoveWord)))
			.End();

		BTabView* tabs = new BTabView("tabs", B_WIDTH_FROM_LABEL);
		tabs->AddTab(general);
		tabs->AddTab(view);
		tabs->AddTab(save);
		tabs->AddTab(spelling);

		BButton* ok = new BButton("ok", "OK", new BMessage(kMsgOK));
		BButton* cancel = new BButton("cancel", "Cancel",
			new BMessage(kMsgCancel));
		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(tabs)
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(cancel)
				.Add(ok)
			.End();
		SetDefaultButton(ok);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgRemoveWord: {
				int32 index = fWords->CurrentSelection();
				if (index >= 0)
					delete fWords->RemoveItem(index);
				break;
			}
			case kMsgOK: {
				gPrefsLock.Lock();
				Prefs& prefs = gPrefs;
				prefs.userName = fName->Text();
				BMenuItem* unit = fUnit->Menu()->FindMarked();
				if (unit != nullptr)
					prefs.unit = fUnit->Menu()->IndexOf(unit);
				prefs.smartQuotes = fSmartQuotes->Value() != 0;
				prefs.showHidden = fHidden->Value() != 0;
				prefs.textBoundaries = fBoundaries->Value() != 0;
				prefs.tableGridlines = fGridlines->Value() != 0;
				prefs.openInPageView = fPageView->Value() != 0;
				prefs.makeBackup = fBackup->Value() != 0;
				BMenuItem* font = fFont->Menu()->FindMarked();
				if (font != nullptr)
					prefs.defaultFont = font->Label();
				float size = (float)atof(fSize->Text());
				if (size >= 4 && size <= 144)
					prefs.defaultSize = size;
				prefs.ignoreUppercase = fUppercase->Value() != 0;
				prefs.ignoreNumbers = fNumbers->Value() != 0;
				std::vector<std::string> removed;
				std::vector<std::string> words;
				for (int32 i = 0; i < fWords->CountItems(); i++) {
					BStringItem* item = dynamic_cast<BStringItem*>(
						fWords->ItemAt(i));
					if (item != nullptr)
						words.push_back(item->Text());
				}
				for (const std::string& word : prefs.customWords) {
					if (std::find(words.begin(), words.end(), word)
						== words.end())
						removed.push_back(word);
				}
				prefs.customWords = words;
				prefs.Save();
				gPrefsLock.Unlock();
				if (!prefs.defaultFont.empty()
					&& FontCache::Get().IsInstalled(prefs.defaultFont))
					gDefaultSerif = prefs.defaultFont;
				if (!removed.empty()) {
					SpellChecker* speller = quillApp->LockSpeller();
					for (const std::string& word : removed)
						speller->RemoveWord(word);
					quillApp->UnlockSpeller();
				}
				// tell every document window
				for (const WindowEntry& entry : quillApp->Windows())
					entry.window->PostMessage(kMsgPrefsChanged);
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
	BTextControl*	fName;
	BMenuField*		fUnit;
	BCheckBox*		fSmartQuotes;
	BCheckBox*		fHidden;
	BCheckBox*		fBoundaries;
	BCheckBox*		fGridlines;
	BCheckBox*		fPageView;
	BCheckBox*		fBackup;
	BMenuField*		fFont;
	BTextControl*	fSize;
	BCheckBox*		fUppercase;
	BCheckBox*		fNumbers;
	BListView*		fWords;
};


void
ShowPreferencesDialog(BWindow* parent)
{
	(new PreferencesDialog(parent))->Show();
}


// #pragma mark - About, Help, Clipboard


class TextWindow : public BWindow {
public:
	TextWindow(const char* title, const char* text, BRect frame)
		:
		BWindow(frame, title, B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
			B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS)
	{
		fText = new BTextView("text");
		fText->MakeEditable(false);
		fText->SetWordWrap(true);
		fText->SetInsets(10, 10, 10, 10);
		fText->SetText(text);
		BScrollView* scroll = new BScrollView("scroll", fText, 0, false, true,
			B_NO_BORDER);
		BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
			.Add(scroll);
		CenterOnScreen();
	}

	void SetStyledText(const char* text, const text_run_array* runs)
	{
		fText->SetText(text, runs);
	}

private:
	BTextView*	fText;
};


void
ShowAboutWindow()
{
	BAlert* alert = new BAlert("About Quill",
		"Quill 1.0\n\n"
		"A word processor for Haiku in the style of a classic 1992 "
		"Macintosh word processor, with its menus, Ribbon, Ruler and "
		"Toolbar, styles, tables, footnotes, headers and footers, "
		"spelling and Print Preview.\n\n"
		"Documents are saved in Rich Text Format.",
		"OK");
	alert->Go(nullptr);
}


static const char* kHelpText =
	"Quill Help\n\n"
	"VIEWS\n"
	"Normal view shows the text in one continuous column; dotted lines "
	"mark page breaks (dense dots for a manual page break, a double line "
	"for a section break). Page Layout view shows pages as they print, "
	"with headers, footers, columns and footnotes. Print Preview shows one "
	"or two pages at a time.\n\n"
	"THE RIBBON\n"
	"Font and size, Bold, Italic, Underline, Superscript, Subscript, "
	"Insert Picture, Show \xC2\xB6 and one to three columns.\n\n"
	"THE RULER\n"
	"The style box, alignment (left, centered, right, justified), line "
	"spacing (single, 1\xC2\xBD, double), paragraph spacing (closed, open), "
	"tab types and the scale button (normal, page, table). Drag the "
	"triangles to set the first-line, left and right indents (Shift-drag "
	"the left indent to move it alone). Click in the ruler to set a tab "
	"of the selected type, drag a tab to move it, drag it off the ruler "
	"to remove it. Double-click the ruler for the Paragraph dialog.\n\n"
	"SELECTING\n"
	"Double-click selects a word, triple-click a paragraph, Alt-click a "
	"sentence. The selection bar is the strip left of the text: click "
	"selects a line, double-click a paragraph, Alt-click the whole "
	"document. Shift-click extends the selection.\n\n"
	"KEYBOARD (Alt is the Command key)\n"
	"Alt-N New, Alt-O Open, Alt-S Save, Alt-P Print, Alt-W Close\n"
	"Alt-Z Undo, Alt-Y Redo/Repeat, Alt-X Cut, Alt-C Copy, Alt-V Paste\n"
	"Alt-F Find, Alt-H Replace, Alt-G Go To, Alt-K Glossary\n"
	"Alt-D Character, Alt-M Paragraph, Alt-T Style, Alt-R Ruler\n"
	"Alt-J Show \xC2\xB6, Alt-L Spelling, Alt-E Footnote, Alt-= Calculate\n"
	"Alt-B / Alt-I / Alt-U or Shift-Alt-B / I / U  Bold, Italic, Underline\n"
	"Shift-Alt-Z Plain Text, Shift-Alt-Space Revert To Style\n"
	"Shift-Alt-]  Word underline, Shift-Alt-[  Double underline, "
	"Shift-Alt-\\  Dotted underline\n"
	"Shift-Alt-/ Strikethrough, Shift-Alt-D Outline, Shift-Alt-W Shadow\n"
	"Shift-Alt-H Small caps, Shift-Alt-K All caps, Shift-Alt-V Hidden\n"
	"Shift-Alt-= Superscript, Shift-Alt-- Subscript\n"
	"Shift-Alt-> / < or Alt-] / [  Font size up / down\n"
	"Shift-Alt-L / C / R / J  Align left, center, right, justify\n"
	"Shift-Alt-Y Double spacing, Shift-Alt-O Open spacing\n"
	"Shift-Alt-N / M  Indent / unindent, Shift-Alt-T Hanging indent\n"
	"Shift-Alt-P Normal style\n"
	"Shift-Return New line, Shift-Enter (keypad) Page break, "
	"Alt-Enter Section break\n"
	"Ctrl-Left / Right  Previous / next word, Ctrl-Up / Down  Previous / "
	"next paragraph, Ctrl-Home / End  Start / end of document\n"
	"Tab in a table moves to the next cell, Shift-Tab to the previous.\n\n"
	"TABLES\n"
	"Insert > Table inserts an empty table. With text selected the command "
	"becomes Text to Table: the selected paragraphs become a table, divided "
	"into cells at paragraph marks, tabs or commas (\"quoted, text\" stays "
	"in one cell). In a table it becomes Table to Text, which turns the "
	"rows back into paragraphs, tab or comma delimited lines.\n\n"
	"FIND AND REPLACE\n"
	"Special characters: ^t tab, ^p paragraph mark, ^n new line, ^d page "
	"break, ^s nonbreaking space, ^- optional hyphen, ^~ nonbreaking "
	"hyphen, ^w white space, ^? any character, ^# any digit, ^$ any "
	"letter, ^^ caret.\n\n"
	"TOOLS\n"
	"Calculate evaluates the selected arithmetic, shows the result in the "
	"page number box and puts it on the clipboard. Sort sorts the selected "
	"paragraphs. Renumber numbers paragraphs. Word Count counts characters, "
	"words, lines and paragraphs. Spelling uses Haiku's word list and your "
	"own custom dictionary.\n\n"
	"FILES\n"
	"Quill saves documents in Rich Text Format, which Word and most word "
	"processors read, or as Text Only.\n";


void
ShowHelpWindow()
{
	TextWindow* window = new TextWindow("Quill Help", kHelpText,
		BRect(80, 80, 640, 640));
	window->Show();
}


void
ShowClipboardWindow()
{
	BString text;
	if (be_clipboard->Lock()) {
		BMessage* data = be_clipboard->Data();
		const char* bytes;
		ssize_t size;
		if (data != nullptr && data->FindData("text/plain", B_MIME_TYPE,
				(const void**)&bytes, &size) == B_OK)
			text.SetTo(bytes, size);
		be_clipboard->Unlock();
	}
	if (text.Length() == 0)
		text = "(The clipboard is empty.)";
	TextWindow* window = new TextWindow("Clipboard", text.String(),
		BRect(120, 120, 520, 420));
	window->Show();
}
