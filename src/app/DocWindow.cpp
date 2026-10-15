/*
 * Quill - the document window.
 */
#include "DocWindow.h"

#include "App.h"
#include "Bars.h"
#include "Constants.h"
#include "Controls.h"
#include "Dialogs.h"
#include "Fonts.h"
#include "FormatMessages.h"
#include "Icons.h"
#include "Panes.h"
#include "Prefs.h"
#include "RTF.h"
#include "Text.h"
#include "TextIO.h"
#include "Tools.h"

#include <Alert.h>
#include <Beep.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <Button.h>
#include <Clipboard.h>
#include <Directory.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <NodeInfo.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollBar.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <sstream>

using namespace quill;

enum {
	kMsgInsertPanelRefs = 'Iprf'
};


static BMenuItem*
Item(BMenu* menu, const char* label, uint32 what, char shortcut = 0,
	uint32 modifiers = 0)
{
	BMenuItem* item = new BMenuItem(label, new BMessage(what), shortcut,
		modifiers);
	menu->AddItem(item);
	return item;
}


static BMenuItem*
Item(BMenu* menu, const char* label, BMessage* message, char shortcut = 0,
	uint32 modifiers = 0)
{
	BMenuItem* item = new BMenuItem(label, message, shortcut, modifiers);
	menu->AddItem(item);
	return item;
}


static BMessage*
IntMessage(uint32 what, const char* name, int32 value)
{
	BMessage* message = new BMessage(what);
	message->AddInt32(name, value);
	return message;
}


DocWindow::DocWindow(BRect frame, Document* document, const entry_ref* ref,
	const char* title, int format)
	:
	BWindow(frame, title, B_DOCUMENT_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fDocument(document),
	fHasRef(ref != nullptr),
	fFormat(format),
	fUntitledName(title),
	fRecentStart(0),
	fFontListStart(0),
	fOutlineLevel(0),
	fOutlineFirstLine(false),
	fOutlineFormatting(true),
	fMode(kModeNormal),
	fPreviewMode(false),
	fSavePanel(nullptr),
	fFormatField(nullptr),
	fPanelFormat(kFormatRTF),
	fInsertPanel(nullptr),
	fPrintSettings(nullptr),
	fPrinting(false),
	fQuitAfterSave(false),
	fSaveAlertShowing(false),
	fFindUp(false),
	fSpellFromStart(false),
	fSpellWrapped(false)
{
	if (ref != nullptr)
		fRef = *ref;
	fSavedChanges = fDocument->changes;
	_BuildMenus();
	_BuildLayout();

	gPrefsLock.Lock();
	bool pageView = gPrefs.openInPageView;
	float zoom = gPrefs.zoom;
	gPrefsLock.Unlock();
	fView->SetZoom(zoom / 100.0f);
	if (pageView)
		_SetMode(kModePage);
	fView->MakeFocus(true);
	_UpdateBars();

	// Word's keyboard commands that have no menu item
	struct Shortcut {
		char		key;
		uint32		modifiers;
		uint32		what;
		const char*	name;
		int32		value;
	};
	const Shortcut kShortcuts[] = {
		{ 'b', 0, kMsgToggleChar, "field", kCFBold },
		{ 'i', 0, kMsgToggleChar, "field", kCFItalic },
		{ 'u', 0, kMsgToggleChar, "field", kCFUnderline },
		{ ']', B_SHIFT_KEY, kMsgUnderlineKind, "kind", kULWord },
		{ '[', B_SHIFT_KEY, kMsgUnderlineKind, "kind", kULDouble },
		{ '\\', B_SHIFT_KEY, kMsgUnderlineKind, "kind", kULDotted },
		{ '/', B_SHIFT_KEY, kMsgToggleChar, "field", kCFStrike },
		{ 'd', B_SHIFT_KEY, kMsgToggleChar, "field", kCFOutline },
		{ 'w', B_SHIFT_KEY, kMsgToggleChar, "field", kCFShadow },
		{ 'h', B_SHIFT_KEY, kMsgToggleChar, "field", kCFSmallCaps },
		{ 'k', B_SHIFT_KEY, kMsgToggleChar, "field", kCFAllCaps },
		{ 'v', B_SHIFT_KEY, kMsgToggleChar, "field", kCFHidden },
		{ '=', B_SHIFT_KEY, kMsgSuperscript, nullptr, 0 },
		{ '-', B_SHIFT_KEY, kMsgSubscript, nullptr, 0 },
		{ '.', B_SHIFT_KEY, kMsgFontUp, nullptr, 0 },
		{ ',', B_SHIFT_KEY, kMsgFontDown, nullptr, 0 },
		{ 'l', B_SHIFT_KEY, kMsgAlign, "align", kAlignLeft },
		{ 'c', B_SHIFT_KEY, kMsgAlign, "align", kAlignCenter },
		{ 'r', B_SHIFT_KEY, kMsgAlign, "align", kAlignRight },
		{ 'j', B_SHIFT_KEY, kMsgAlign, "align", kAlignJustify },
		{ 'n', B_SHIFT_KEY, kMsgNest, "in", 1 },
		{ 'm', B_SHIFT_KEY, kMsgNest, "in", 0 },
		{ 't', B_SHIFT_KEY, kMsgHanging, nullptr, 0 },
		{ 'o', B_SHIFT_KEY, kMsgParaSpacing, "toggle", 1 },
		{ 'y', B_SHIFT_KEY, kMsgLineSpacing, "toggle", 1 },
		{ 'p', B_SHIFT_KEY, kMsgApplyStyle, "index", 0 },
	};
	for (const Shortcut& shortcut : kShortcuts) {
		BMessage* message = new BMessage(shortcut.what);
		if (shortcut.name != nullptr)
			message->AddInt32(shortcut.name, shortcut.value);
		AddShortcut(shortcut.key, B_COMMAND_KEY | shortcut.modifiers, message);
	}
	AddShortcut(' ', B_COMMAND_KEY | B_SHIFT_KEY,
		new BMessage(kMsgRevertToStyle));

	quillApp->WindowOpened(this, Title());
}


DocWindow::~DocWindow()
{
	// The views refer to the document and to this window. BWindow deletes
	// its views only after this destructor, and detaching the focus view
	// calls back into it, so remove them while the document still exists.
	fView->SetListener(nullptr);
	if (fPane->View() != nullptr)
		fPane->View()->SetListener(nullptr);
	fDocumentArea->RemoveSelf();
	delete fDocumentArea;
	fPreviewArea->RemoveSelf();
	delete fPreviewArea;
	fView = nullptr;
	fPane = nullptr;
	fPreview = nullptr;

	delete fSavePanel;
	delete fInsertPanel;
	delete fPrintSettings;
	delete fDocument;
}


// #pragma mark - construction


void
DocWindow::_BuildMenus()
{
	fMenuBar = new BMenuBar("menubar");

	// File
	fFileMenu = new BMenu("File");
	Item(fFileMenu, "New", kMsgNew, 'N');
	Item(fFileMenu, "Open" B_UTF8_ELLIPSIS, kMsgOpen, 'O');
	Item(fFileMenu, "Close", kMsgClose, 'W');
	Item(fFileMenu, "Save", kMsgSave, 'S');
	Item(fFileMenu, "Save As" B_UTF8_ELLIPSIS, kMsgSaveAs, 'S', B_SHIFT_KEY);
	fFileMenu->AddSeparatorItem();
	Item(fFileMenu, "Summary Info" B_UTF8_ELLIPSIS, kMsgSummaryInfo);
	fFileMenu->AddSeparatorItem();
	fPreviewItem = Item(fFileMenu, "Print Preview", kMsgPrintPreview, 'I',
		B_OPTION_KEY);
	Item(fFileMenu, "Page Setup" B_UTF8_ELLIPSIS, kMsgPageSetup);
	Item(fFileMenu, "Print" B_UTF8_ELLIPSIS, kMsgPrint, 'P');
	fFileMenu->AddSeparatorItem();
	fRecentStart = fFileMenu->CountItems();
	fFileMenu->AddSeparatorItem();
	BMenuItem* quit = Item(fFileMenu, "Quit", B_QUIT_REQUESTED, 'Q');
	quit->SetTarget(be_app);
	fMenuBar->AddItem(fFileMenu);

	// Edit
	BMenu* edit = new BMenu("Edit");
	fUndoItem = Item(edit, "Undo", kMsgUndo, 'Z');
	fRedoItem = Item(edit, "Repeat", kMsgRedo, 'Y');
	edit->AddSeparatorItem();
	fCutItem = Item(edit, "Cut", B_CUT, 'X');
	fCopyItem = Item(edit, "Copy", B_COPY, 'C');
	fPasteItem = Item(edit, "Paste", B_PASTE, 'V');
	fClearItem = Item(edit, "Clear", kMsgClear);
	Item(edit, "Select All", B_SELECT_ALL, 'A');
	edit->AddSeparatorItem();
	Item(edit, "Find" B_UTF8_ELLIPSIS, kMsgFind, 'F');
	Item(edit, "Find Again", kMsgFindAgain, 'A', B_OPTION_KEY);
	Item(edit, "Replace" B_UTF8_ELLIPSIS, kMsgReplace, 'H');
	Item(edit, "Go To" B_UTF8_ELLIPSIS, kMsgGoTo, 'G');
	edit->AddSeparatorItem();
	Item(edit, "Glossary" B_UTF8_ELLIPSIS, kMsgGlossary, 'K');
	fMenuBar->AddItem(edit);

	// View
	BMenu* view = new BMenu("View");
	fNormalItem = Item(view, "Normal", kMsgViewNormal, 'N', B_OPTION_KEY);
	fOutlineItem = Item(view, "Outline", kMsgViewOutline, 'O', B_OPTION_KEY);
	fPageItem = Item(view, "Page Layout", kMsgViewPage, 'P', B_OPTION_KEY);
	view->AddSeparatorItem();
	fToolbarItem = Item(view, "Toolbar", kMsgToggleToolbar);
	fRibbonItem = Item(view, "Ribbon", kMsgToggleRibbon, 'R', B_OPTION_KEY);
	fRulerItem = Item(view, "Ruler", kMsgToggleRuler, 'R');
	view->AddSeparatorItem();
	Item(view, "Header", kMsgViewHeader);
	Item(view, "Footer", kMsgViewFooter);
	fFirstHeaderItem = Item(view, "First Header", kMsgViewFirstHeader);
	fFirstFooterItem = Item(view, "First Footer", kMsgViewFirstFooter);
	Item(view, "Footnotes", kMsgViewFootnotes, 'S',
		B_OPTION_KEY | B_SHIFT_KEY);
	view->AddSeparatorItem();
	fShowAllItem = Item(view, "Show \xC2\xB6", kMsgShowAll, 'J');
	Item(view, "Zoom" B_UTF8_ELLIPSIS, kMsgZoom);
	fMenuBar->AddItem(view);

	// Insert
	BMenu* insert = new BMenu("Insert");
	Item(insert, "Page Break", kMsgInsertPageBreak, B_ENTER, B_SHIFT_KEY);
	Item(insert, "Section Break", kMsgInsertSectionBreak, B_ENTER);
	insert->AddSeparatorItem();
	fInsertTableItem = Item(insert, "Table" B_UTF8_ELLIPSIS, kMsgInsertTable);
	Item(insert, "Footnote" B_UTF8_ELLIPSIS, kMsgInsertFootnote, 'E');
	insert->AddSeparatorItem();
	Item(insert, "Date", kMsgInsertDate);
	Item(insert, "Time", kMsgInsertTime);
	Item(insert, "Page Number", kMsgInsertPageNumber);
	Item(insert, "Symbol" B_UTF8_ELLIPSIS, kMsgInsertSymbol, 'Q',
		B_OPTION_KEY);
	insert->AddSeparatorItem();
	Item(insert, "Table of Contents", kMsgInsertTOC);
	insert->AddSeparatorItem();
	Item(insert, "Picture" B_UTF8_ELLIPSIS, kMsgInsertPicture);
	Item(insert, "File" B_UTF8_ELLIPSIS, kMsgInsertFile);
	fMenuBar->AddItem(insert);

	// Format
	BMenu* format = new BMenu("Format");
	Item(format, "Character" B_UTF8_ELLIPSIS, kMsgCharacter, 'D');
	Item(format, "Paragraph" B_UTF8_ELLIPSIS, kMsgParagraph, 'M');
	Item(format, "Tabs" B_UTF8_ELLIPSIS, kMsgTabs);
	Item(format, "Border" B_UTF8_ELLIPSIS, kMsgBorder);
	Item(format, "Section" B_UTF8_ELLIPSIS, kMsgSection);
	Item(format, "Document" B_UTF8_ELLIPSIS, kMsgDocument);
	fTableLayoutItem = Item(format, "Table Layout" B_UTF8_ELLIPSIS,
		kMsgTableLayout);
	format->AddSeparatorItem();
	Item(format, "Style" B_UTF8_ELLIPSIS, kMsgStyle, 'T');
	Item(format, "Revert To Style", kMsgRevertToStyle);
	Item(format, "Change Case" B_UTF8_ELLIPSIS, kMsgChangeCase);
	format->AddSeparatorItem();
	Item(format, "Plain Text", kMsgPlainText, 'Z', B_SHIFT_KEY);
	fBoldItem = Item(format, "Bold", IntMessage(kMsgToggleChar, "field",
		kCFBold), 'B', B_SHIFT_KEY);
	fItalicItem = Item(format, "Italic", IntMessage(kMsgToggleChar, "field",
		kCFItalic), 'I', B_SHIFT_KEY);
	fUnderlineItem = Item(format, "Underline", IntMessage(kMsgToggleChar,
		"field", kCFUnderline), 'U', B_SHIFT_KEY);
	fMenuBar->AddItem(format);

	// Font
	fFontMenu = new BMenu("Font");
	for (int i = 0; i < kStandardSizeCount; i++) {
		BMessage* message = new BMessage(kMsgFontSize);
		message->AddFloat("size", kStandardSizes[i]);
		BString label;
		label << (int)kStandardSizes[i] << " Point";
		fFontMenu->AddItem(new BMenuItem(label.String(), message));
	}
	fFontMenu->AddSeparatorItem();
	Item(fFontMenu, "Up", kMsgFontUp, ']');
	Item(fFontMenu, "Down", kMsgFontDown, '[');
	Item(fFontMenu, "Other" B_UTF8_ELLIPSIS, kMsgCharacter);
	fFontMenu->AddSeparatorItem();
	fFontListStart = fFontMenu->CountItems();
	for (const std::string& family : FontCache::Get().Families()) {
		BMessage* message = new BMessage(kMsgFontFamily);
		message->AddString("family", family.c_str());
		fFontMenu->AddItem(new BMenuItem(family.c_str(), message));
	}
	fMenuBar->AddItem(fFontMenu);

	// Tools
	BMenu* tools = new BMenu("Tools");
	Item(tools, "Spelling" B_UTF8_ELLIPSIS, kMsgSpelling, 'L');
	Item(tools, "Word Count" B_UTF8_ELLIPSIS, kMsgWordCount);
	Item(tools, "Renumber" B_UTF8_ELLIPSIS, kMsgRenumber);
	Item(tools, "Sort", kMsgSort);
	Item(tools, "Sort Descending", kMsgSortDescending);
	Item(tools, "Calculate", kMsgCalculate, '=');
	tools->AddSeparatorItem();
	Item(tools, "Repaginate Now", kMsgRepaginate);
	tools->AddSeparatorItem();
	Item(tools, "Preferences" B_UTF8_ELLIPSIS, kMsgPreferences);
	fMenuBar->AddItem(tools);

	// Window
	fWindowMenu = new BMenu("Window");
	Item(fWindowMenu, "Show Clipboard", kMsgShowClipboard);
	Item(fWindowMenu, "Arrange All", kMsgArrange);
	fWindowMenu->AddSeparatorItem();
	fMenuBar->AddItem(fWindowMenu);

	// Help
	BMenu* help = new BMenu("Help");
	Item(help, "Quill Help", kMsgHelp, '?');
	Item(help, "Keyboard Shortcuts", kMsgKeyboardHelp);
	help->AddSeparatorItem();
	Item(help, "About Quill" B_UTF8_ELLIPSIS, kMsgAbout);
	fMenuBar->AddItem(help);
}


void
DocWindow::_BuildLayout()
{
	fToolbar = new Toolbar();
	fRibbon = new Ribbon();
	fRuler = new Ruler();
	fOutlineBar = new OutlineBar();

	fView = new DocView("document", fDocument, &fDocument->body, true,
		&fPictures);
	fView->SetListener(this);
	fView->SetExplicitMinSize(BSize(120, 80));
	fHScroll = new BScrollBar("hscroll", fView, 0, 0, B_HORIZONTAL);
	fVScroll = new BScrollBar("vscroll", fView, 0, 0, B_VERTICAL);
	fView->SetScrollBars(fHScroll, fVScroll);
	fStatus = new StatusView("status");

	fPane = new StoryPane(fDocument, &fPictures);

	fPreview = new PreviewView(fDocument, fView->Renderer());
	fPreviewPage = new BStringView("previewPage", "Page 1");

	BGroupView* previewBar = new BGroupView(B_HORIZONTAL, 3);
	previewBar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BMessage* pages = new BMessage(kMsgPreviewCommand);
	pages->AddString("op", "pages");
	BMessage* margins = new BMessage(kMsgPreviewCommand);
	margins->AddString("op", "margins");
	BMessage* previous = new BMessage(kMsgPreviewCommand);
	previous->AddString("op", "previous");
	BMessage* next = new BMessage(kMsgPreviewCommand);
	next->AddString("op", "next");
	BLayoutBuilder::Group<>(previewBar)
		.SetInsets(4, 3, 4, 3)
		.Add(new IconButton("print", kIconPrint, new BMessage(kMsgPrint),
			"Print"))
		.Add(new IconButton("margins", kIconMargins, margins, "Margins"))
		.Add(new IconButton("pages", kIconTwoPages, pages,
			"One page / two pages"))
		.Add(new SeparatorBar())
		.Add(new BButton("previous", "\xE2\x97\x80", previous))
		.Add(new BButton("next", "\xE2\x96\xB6", next))
		.Add(fPreviewPage)
		.AddGlue()
		.Add(new BButton("close", "Close", new BMessage(kMsgPrintPreview)));

	fPreviewArea = new BGroupView(B_VERTICAL, 0);
	BLayoutBuilder::Group<>((BGroupView*)fPreviewArea)
		.Add(previewBar)
		.Add(fPreview);

	BGroupView* documentArea = new BGroupView(B_VERTICAL, 0);
	fDocumentArea = documentArea;
	BLayoutBuilder::Group<>(documentArea)
		.AddGrid(0.0f, 0.0f, 3.0f)
			.Add(fView, 0, 0)
			.Add(fVScroll, 1, 0)
			.AddGroup(B_HORIZONTAL, 0, 0, 1)
				.Add(fStatus)
				.Add(fHScroll)
			.End()
			.Add(BSpaceLayoutItem::CreateHorizontalStrut(B_V_SCROLL_BAR_WIDTH),
				1, 1)
		.End()
		.Add(fPane, 1.0f);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.Add(fToolbar)
		.Add(fRibbon)
		.Add(fRuler)
		.Add(fOutlineBar)
		.Add(documentArea)
		.Add(fPreviewArea);

	fPane->Hide();
	fPreviewArea->Hide();
	fOutlineBar->Hide();
	gPrefsLock.Lock();
	bool toolbar = gPrefs.showToolbar;
	bool ribbon = gPrefs.showRibbon;
	bool ruler = gPrefs.showRuler;
	gPrefsLock.Unlock();
	if (!toolbar)
		fToolbar->Hide();
	if (!ribbon)
		fRibbon->Hide();
	if (!ruler)
		fRuler->Hide();
}


// #pragma mark - state


bool
DocWindow::HasRef(const entry_ref& ref) const
{
	return fHasRef && fRef == ref;
}


bool
DocWindow::IsModified() const
{
	return fDocument->changes != fSavedChanges;
}


bool
DocWindow::IsUntouched() const
{
	return !fHasRef && !IsModified()
		&& Document::StoryEmpty(fDocument->body);
}


DocView*
DocWindow::_ActiveView()
{
	if (!fPane->IsHidden(fPane) && fPane->View() != nullptr
		&& fPane->View()->IsFocus())
		return fPane->View();
	return fView;
}


void
DocWindow::_Changed(bool scroll)
{
	DocView* view = _ActiveView();
	view->Changed(scroll);
	if (view != fView)
		fView->Changed(false);
}


void
DocWindow::DocViewChanged(DocView* view, bool content)
{
	if (content && view != fView && fView != nullptr)
		fView->Changed(false);
	_UpdateBars();
}


void
DocWindow::DocViewScrolled(DocView* view)
{
	(void)view;
	_UpdateBars();
}


void
DocWindow::DocViewContextMenu(DocView* view, BPoint where)
{
	BPopUpMenu* menu = new BPopUpMenu("context", false, false);
	bool selection = view->GetEditor().HasSelection();
	BMenuItem* cut = Item(menu, "Cut", B_CUT);
	BMenuItem* copy = Item(menu, "Copy", B_COPY);
	cut->SetEnabled(selection);
	copy->SetEnabled(selection);
	Item(menu, "Paste", B_PASTE);
	menu->AddSeparatorItem();
	Item(menu, "Character" B_UTF8_ELLIPSIS, kMsgCharacter);
	Item(menu, "Paragraph" B_UTF8_ELLIPSIS, kMsgParagraph);
	Item(menu, "Style" B_UTF8_ELLIPSIS, kMsgStyle);
	menu->SetTargetForItems(this);
	menu->SetAsyncAutoDestruct(true);
	menu->Go(view->ConvertToScreen(where), true, false, true);
}


void
DocWindow::_UpdateBars()
{
	if (fPreviewMode) {
		BString text;
		text << "Page " << fPreview->FirstPage() + 1;
		fPreviewPage->SetText(text.String());
		return;
	}
	DocView* view = _ActiveView();
	Editor& editor = view->GetEditor();
	uint32 charSame;
	uint32 paraSame;
	CharFormat charFormat = editor.SelectionCharFormat(&charSame);
	ParaFormat paraFormat = editor.SelectionParaFormat(&paraSame);
	bool showAll = fView->ShowAll();
	if (!fRibbon->IsHidden(fRibbon)) {
		fRibbon->SetState(charFormat, charSame, showAll,
			fDocument->settings.columns);
	}
	if (!fToolbar->IsHidden(fToolbar)) {
		fToolbar->SetState(editor.CanUndo(), editor.HasSelection(), showAll,
			fMode);
	}
	if (!fRuler->IsHidden(fRuler)) {
		RulerState state;
		state.format = paraFormat;
		state.same = paraSame;
		state.origin = view->ColumnOriginX();
		if (view != fView)
			state.origin = view->ColumnOriginX();
		state.zoom = view->Zoom();
		Pos caret = editor.Caret();
		DocLayout& layout = view->Layout();
		state.width = caret.para < layout.ParaCount()
			? layout.Para(caret.para).width : layout.LayoutWidth();
		gPrefsLock.Lock();
		state.unit = gPrefs.unit;
		gPrefsLock.Unlock();
		state.defaultTab = fDocument->settings.defaultTab;
		state.pageView = view == fView && fMode == kModePage;
		if (state.pageView) {
			int32 page = fView->CaretPage();
			state.pageLeft = fView->PageViewRect(page).left
				- fView->Bounds().left;
		}
		state.marginLeft = fDocument->settings.marginLeft;
		state.marginRight = fDocument->settings.marginRight;
		state.paperWidth = fDocument->settings.PaperWidth();
		const Story& story = editor.GetStory();
		if (story[caret.para].kind == kParaCell) {
			int32 end = caret.para;
			while (end < (int32)story.size() && story[end].kind == kParaCell)
				end++;
			int32 start = caret.para;
			while (start > 0 && story[start - 1].kind == kParaCell)
				start--;
			if (end < (int32)story.size() && story[end].kind == kParaRowEnd) {
				state.inTable = true;
				state.row = story[end].row;
				int32 cell = caret.para - start;
				state.rowOrigin = state.origin - (state.row.leftEdge
					+ state.row.CellLeft(cell) + state.row.gap) * state.zoom;
			}
		}
		for (const Style& style : fDocument->styles.styles)
			state.styleNames.push_back(style.name);
		state.style = (paraSame & kPFStyle) ? paraFormat.style : -1;
		fRuler->SetState(state);
	}
	_UpdateStatus();
}


void
DocWindow::_UpdateStatus()
{
	BString text;
	if (fMode == kModePage || _ActiveView() == fView) {
		int32 page = fView->CaretPage();
		const std::vector<PageInfo>& pages = fView->Layout().Pages();
		int32 number = page < (int32)pages.size() ? pages[page].number
			: page + 1;
		text << "Page " << number;
	} else
		text = "";
	fStatus->SetText(text.String());
	fStatus->SetMessage("");
}


void
DocWindow::_UpdateTitle()
{
	BString title = fUntitledName;
	if (fHasRef)
		title = fRef.name;
	SetTitle(title.String());
	quillApp->TitleChanged(this, title.String());
}


void
DocWindow::MenusBeginning()
{
	DocView* view = _ActiveView();
	Editor& editor = view->GetEditor();
	BString label;
	if (editor.CanUndo()) {
		label << "Undo " << editor.UndoName().c_str();
		fUndoItem->SetLabel(label.String());
		fUndoItem->SetEnabled(true);
	} else {
		fUndoItem->SetLabel("Can't Undo");
		fUndoItem->SetEnabled(false);
	}
	label = "";
	if (editor.CanRedo()) {
		label << "Redo " << editor.RedoName().c_str();
		fRedoItem->SetLabel(label.String());
		fRedoItem->SetEnabled(true);
	} else if (!editor.LastTyped().empty()) {
		fRedoItem->SetLabel("Repeat Typing");
		fRedoItem->SetEnabled(true);
	} else {
		fRedoItem->SetLabel("Can't Repeat");
		fRedoItem->SetEnabled(false);
	}
	bool selection = editor.HasSelection();
	fCutItem->SetEnabled(selection);
	fCopyItem->SetEnabled(selection);
	fClearItem->SetEnabled(selection);
	bool clipboard = false;
	if (be_clipboard->Lock()) {
		BMessage* data = be_clipboard->Data();
		clipboard = data != nullptr && (data->HasData("text/plain", B_MIME_TYPE)
			|| data->HasData("text/rtf", B_MIME_TYPE));
		be_clipboard->Unlock();
	}
	fPasteItem->SetEnabled(clipboard);

	fNormalItem->SetMarked(fMode == kModeNormal && !fPreviewMode);
	fOutlineItem->SetMarked(fMode == kModeOutline && !fPreviewMode);
	fPageItem->SetMarked(fMode == kModePage && !fPreviewMode);
	fPreviewItem->SetMarked(fPreviewMode);
	fToolbarItem->SetMarked(!fToolbar->IsHidden(fToolbar));
	fRibbonItem->SetMarked(!fRibbon->IsHidden(fRibbon));
	fRulerItem->SetMarked(gPrefs.showRuler);
	fShowAllItem->SetMarked(fView->ShowAll());
	fFirstHeaderItem->SetEnabled(fDocument->settings.titlePage);
	fFirstFooterItem->SetEnabled(fDocument->settings.titlePage);

	uint32 same;
	CharFormat format = editor.SelectionCharFormat(&same);
	fBoldItem->SetMarked((same & kCFBold) && format.bold);
	fItalicItem->SetMarked((same & kCFItalic) && format.italic);
	fUnderlineItem->SetMarked((same & kCFUnderline)
		&& format.underline == kULSingle);
	Pos caret = editor.Caret();
	bool inTable = editor.GetStory()[caret.para].IsTablePart();
	fTableLayoutItem->SetEnabled(inTable);
	// Like Word, the Table command converts selected text, and text back
	// from a table.
	if (inTable)
		fInsertTableItem->SetLabel("Table to Text" B_UTF8_ELLIPSIS);
	else if (selection)
		fInsertTableItem->SetLabel("Text to Table" B_UTF8_ELLIPSIS);
	else
		fInsertTableItem->SetLabel("Table" B_UTF8_ELLIPSIS);

	// Font menu
	for (int32 i = 0; i < fFontMenu->CountItems(); i++) {
		BMenuItem* item = fFontMenu->ItemAt(i);
		BMessage* message = item->Message();
		if (message == nullptr)
			continue;
		if (message->what == kMsgFontSize) {
			float size;
			message->FindFloat("size", &size);
			item->SetMarked((same & kCFSize) && size == format.size);
		} else if (message->what == kMsgFontFamily) {
			const char* family;
			message->FindString("family", &family);
			item->SetMarked((same & kCFFont) && format.font == family);
		}
	}
	_UpdateRecentMenu();
	_UpdateWindowMenu();
}


void
DocWindow::_UpdateRecentMenu()
{
	// remove the old entries between the separators
	while (true) {
		BMenuItem* item = fFileMenu->ItemAt(fRecentStart);
		if (item == nullptr || item->Message() == nullptr
			|| item->Message()->what != kMsgOpenRecent)
			break;
		delete fFileMenu->RemoveItem(fRecentStart);
	}
	gPrefsLock.Lock();
	std::vector<std::string> recent = gPrefs.recent;
	gPrefsLock.Unlock();
	int32 index = fRecentStart;
	int number = 1;
	for (const std::string& path : recent) {
		BMessage* message = new BMessage(kMsgOpenRecent);
		message->AddString("path", path.c_str());
		BString label;
		label << number++ << " " << BPath(path.c_str()).Leaf();
		BMenuItem* item = new BMenuItem(label.String(), message);
		item->SetTarget(be_app);
		fFileMenu->AddItem(item, index++);
	}
}


void
DocWindow::_UpdateWindowMenu()
{
	while (fWindowMenu->CountItems() > 3)
		delete fWindowMenu->RemoveItem(3);
	for (const WindowEntry& entry : quillApp->Windows()) {
		BMessage* message = new BMessage(kMsgActivateWindow);
		message->AddPointer("window", entry.window);
		BMenuItem* item = new BMenuItem(entry.title.String(), message);
		item->SetTarget(be_app);
		item->SetMarked(entry.window == this);
		fWindowMenu->AddItem(item);
	}
}


void
DocWindow::FrameMoved(BPoint where)
{
	BWindow::FrameMoved(where);
	gPrefsLock.Lock();
	gPrefs.windowFrame = Frame();
	gPrefsLock.Unlock();
}


void
DocWindow::FrameResized(float width, float height)
{
	BWindow::FrameResized(width, height);
	gPrefsLock.Lock();
	gPrefs.windowFrame = Frame();
	gPrefsLock.Unlock();
}


// #pragma mark - closing and saving


bool
DocWindow::QuitRequested()
{
	if (!IsModified()) {
		quillApp->WindowClosed(this);
		return true;
	}
	if (fSaveAlertShowing)
		return false;
	fSaveAlertShowing = true;
	BString text;
	text << "Save changes to \"" << Title() << "\"?";
	BAlert* alert = new BAlert("Save", text.String(), "Don't Save", "Cancel",
		"Save", B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_WARNING_ALERT);
	alert->SetShortcut(1, B_ESCAPE);
	alert->Go(new BInvoker(new BMessage(kMsgSaveChoice), this));
	return false;
}


void
DocWindow::_Save(bool saveAs)
{
	if (!saveAs && fHasRef) {
		BEntry entry(&fRef);
		BEntry parent;
		entry_ref directory;
		if (entry.GetParent(&parent) == B_OK
			&& parent.GetRef(&directory) == B_OK) {
			status_t status = _SaveTo(directory, fRef.name, fFormat);
			_SaveDone(status == B_OK);
			return;
		}
	}
	_ShowSavePanel();
}


void
DocWindow::_ShowSavePanel()
{
	if (fSavePanel == nullptr) {
		BMessenger messenger(this);
		fSavePanel = new BFilePanel(B_SAVE_PANEL, &messenger, nullptr,
			B_FILE_NODE, false);
		BWindow* window = fSavePanel->Window();
		if (window->Lock()) {
			BView* background = window->ChildAt(0);
			BView* text = window->FindView("text view");
			BPopUpMenu* menu = new BPopUpMenu("format");
			static const char* kFormats[] = { "Rich Text Format",
				"Text Only", "Text Only with Line Breaks" };
			for (int i = 0; i < 3; i++) {
				BMessage* message = new BMessage(kMsgSaveFormat);
				message->AddInt32("format", i);
				BMenuItem* item = new BMenuItem(kFormats[i], message);
				item->SetTarget(this);
				menu->AddItem(item);
			}
			if (background != nullptr && text != nullptr) {
				const char* label = "Save File as Type:";
				float divider = be_plain_font->StringWidth(label) + 10;
				float width = divider + be_plain_font->StringWidth(
					"Text Only with Line Breaks") + 40;
				// make room for the menu next to the name field
				window->ResizeBy(width + 10, 0);
				BRect frame = text->Frame();
				frame.left = frame.right + 10;
				frame.right = frame.left + width;
				fFormatField = new BMenuField(frame, "format", label, menu,
					B_FOLLOW_LEFT | B_FOLLOW_BOTTOM);
				fFormatField->SetDivider(divider);
				background->AddChild(fFormatField);
				BView* cancel = window->FindView("cancel button");
				if (cancel != nullptr && fFormatField->Frame().right
						> cancel->Frame().left - 8) {
					float overlap = fFormatField->Frame().right
						- (cancel->Frame().left - 8);
					text->ResizeBy(-overlap, 0);
					fFormatField->MoveBy(-overlap, 0);
				}
			}
			window->Unlock();
		}
	}
	fPanelFormat = fFormat;
	if (fFormatField != nullptr) {
		BWindow* window = fSavePanel->Window();
		if (window->Lock()) {
			BMenuItem* item = fFormatField->Menu()->ItemAt(fPanelFormat);
			if (item != nullptr)
				item->SetMarked(true);
			window->Unlock();
		}
	}
	if (fHasRef) {
		BEntry entry(&fRef);
		BEntry parent;
		if (entry.GetParent(&parent) == B_OK)
			fSavePanel->SetPanelDirectory(&parent);
		fSavePanel->SetSaveText(fRef.name);
	} else
		fSavePanel->SetSaveText(Title());
	fSavePanel->Show();
}


status_t
DocWindow::_SaveTo(const entry_ref& directoryRef, const char* name, int format)
{
	BDirectory directory(&directoryRef);
	status_t status = directory.InitCheck();
	if (status != B_OK)
		return status;

	time_t now = time(nullptr);
	if (fDocument->info.created == 0)
		fDocument->info.created = now;
	fDocument->info.revised = now;
	fDocument->info.revisions++;

	std::string data;
	switch (format) {
		case kFormatText:
			data = WritePlainText(*fDocument);
			break;
		case kFormatTextLineBreaks:
			data = WritePlainTextWithLineBreaks(*fDocument, fView->Layout());
			break;
		default:
			data = WriteRTF(*fDocument);
			break;
	}

	BEntry existing(&directory, name);
	gPrefsLock.Lock();
	bool backup = gPrefs.makeBackup;
	gPrefsLock.Unlock();
	if (backup && existing.Exists()) {
		BString backupName;
		backupName << "Backup of " << name;
		BEntry old(&directory, backupName.String());
		if (old.Exists())
			old.Remove();
		existing.Rename(backupName.String());
	}

	BFile file(&directory, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	status = file.InitCheck();
	if (status != B_OK)
		return status;
	ssize_t written = file.Write(data.data(), data.size());
	if (written < 0)
		return (status_t)written;
	if ((size_t)written != data.size())
		return B_IO_ERROR;
	BNodeInfo info(&file);
	info.SetType(format == kFormatRTF ? "text/rtf" : "text/plain");
	file.Unset();

	BEntry entry(&directory, name);
	entry_ref ref;
	if (entry.GetRef(&ref) == B_OK) {
		fRef = ref;
		fHasRef = true;
		BPath path(&entry);
		gPrefsLock.Lock();
		gPrefs.AddRecent(path.Path());
		gPrefsLock.Unlock();
	}
	fFormat = format;
	fSavedChanges = fDocument->changes;
	_UpdateTitle();
	return B_OK;
}


void
DocWindow::_SaveDone(bool success)
{
	if (!success) {
		BAlert* alert = new BAlert("Error",
			"The document could not be saved.", "OK", nullptr, nullptr,
			B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->Go(nullptr);
		fQuitAfterSave = false;
		quillApp->CancelQuit();
		return;
	}
	fStatus->SetMessage("Saved");
	if (fQuitAfterSave) {
		fQuitAfterSave = false;
		PostMessage(B_QUIT_REQUESTED);
	}
}


namespace {

struct PrintJobData {
	Document*	document;
	BMessage*	settings;
	BString		name;
	BMessenger	window;
};


status_t
PrintThread(void* data)
{
	PrintJobData* job = (PrintJobData*)data;
	DocLayout layout(&gMetrics);
	layout.SetStory(job->document, &job->document->body, true);
	LayoutOptions options;
	options.showHidden = job->document->settings.printHidden;
	layout.SetOptions(options);
	layout.Update();
	PictureCache pictures;
	status_t status = PrintDocument(job->document, layout, &pictures,
		&job->settings, job->name.String());
	BMessage done(kMsgPrintDone);
	done.AddInt32("status", status);
	if (job->settings != nullptr)
		done.AddMessage("settings", job->settings);
	job->window.SendMessage(&done);
	delete job->settings;
	delete job->document;
	delete job;
	return B_OK;
}

}	// namespace


void
DocWindow::_Print()
{
	// The print dialogs block the calling thread; print a copy of the
	// document from a thread of its own so that the window stays alive.
	if (fPrinting)
		return;
	PrintJobData* job = new PrintJobData;
	job->document = new Document(*fDocument);
	job->settings = fPrintSettings != nullptr
		? new BMessage(*fPrintSettings) : nullptr;
	job->name = Title();
	job->window = BMessenger(this);
	thread_id thread = spawn_thread(PrintThread, "Quill print",
		B_NORMAL_PRIORITY, job);
	if (thread < 0) {
		delete job->document;
		delete job->settings;
		delete job;
		return;
	}
	fPrinting = true;
	fStatus->SetMessage("Printing" B_UTF8_ELLIPSIS);
	resume_thread(thread);
}


// #pragma mark - views and panes


void
DocWindow::_SetBarVisible(BView* bar, bool visible)
{
	if (visible && bar->IsHidden(bar))
		bar->Show();
	else if (!visible && !bar->IsHidden(bar))
		bar->Hide();
}


void
DocWindow::_ShowRulerOrOutlineBar()
{
	gPrefsLock.Lock();
	bool ruler = gPrefs.showRuler;
	gPrefsLock.Unlock();
	bool outline = fMode == kModeOutline && !fPreviewMode;
	_SetBarVisible(fRuler, ruler && !outline && !fPreviewMode);
	_SetBarVisible(fOutlineBar, outline);
}


void
DocWindow::_SetMode(int mode)
{
	if (fPreviewMode)
		_SetPreview(false);
	fMode = mode;
	if (mode == kModeOutline) {
		fView->SetOutlineOptions(fOutlineLevel, fOutlineFirstLine,
			fOutlineFormatting);
	}
	fView->SetMode(mode);
	_ShowRulerOrOutlineBar();
	fOutlineBar->SetState(fOutlineLevel, fOutlineFirstLine,
		fOutlineFormatting);
	_UpdateBars();
}


void
DocWindow::_OutlineCommand(BMessage* message)
{
	if (fMode != kModeOutline)
		_SetMode(kModeOutline);
	const char* op = "";
	message->FindString("op", &op);
	BString operation(op);
	Editor& editor = fView->GetEditor();
	if (operation == "promote")
		OutlineChangeLevel(editor, -1);
	else if (operation == "demote")
		OutlineChangeLevel(editor, 1);
	else if (operation == "body")
		OutlineToBody(editor);
	else if (operation == "up" || operation == "down")
		OutlineMove(editor, operation == "up", fView->OutlineVisibility());
	else if (operation == "expand" || operation == "collapse")
		OutlineSetCollapsed(editor, operation == "collapse");
	else if (operation == "level") {
		int32 level = 0;
		message->FindInt32("level", &level);
		fOutlineLevel = level;
		// showing levels expands what was collapsed, as in Word
		for (Paragraph& paragraph : fDocument->body)
			paragraph.collapsed = false;
	} else if (operation == "firstLine")
		fOutlineFirstLine = !fOutlineFirstLine;
	else if (operation == "formatting")
		fOutlineFormatting = !fOutlineFormatting;
	fView->SetOutlineOptions(fOutlineLevel, fOutlineFirstLine,
		fOutlineFormatting);
	fOutlineBar->SetState(fOutlineLevel, fOutlineFirstLine,
		fOutlineFormatting);
	fView->MakeFocus(true);
	_UpdateBars();
}


void
DocWindow::_SetPreview(bool preview)
{
	if (preview == fPreviewMode)
		return;
	fPreviewMode = preview;
	if (preview) {
		fView->Layout().Update();
		_SetBarVisible(fToolbar, false);
		_SetBarVisible(fRibbon, false);
		_SetBarVisible(fRuler, false);
		_SetBarVisible(fOutlineBar, false);
		_SetBarVisible(fDocumentArea, false);
		fPreview->SetLayout(&fView->Layout());
		int32 page = fView->CaretPage();
		if (fPreview->TwoPages())
			page -= page % 2;
		fPreview->SetFirstPage(page);
		_SetBarVisible(fPreviewArea, true);
		fPreview->MakeFocus(true);
		BString title;
		title << "Print Preview: " << (fHasRef ? fRef.name
			: fUntitledName.String());
		SetTitle(title.String());
	} else {
		_SetBarVisible(fPreviewArea, false);
		gPrefsLock.Lock();
		bool toolbar = gPrefs.showToolbar;
		bool ribbon = gPrefs.showRibbon;
		bool ruler = gPrefs.showRuler;
		gPrefsLock.Unlock();
		_SetBarVisible(fToolbar, toolbar);
		_SetBarVisible(fRibbon, ribbon);
		(void)ruler;
		_SetBarVisible(fDocumentArea, true);
		_ShowRulerOrOutlineBar();
		fView->MakeFocus(true);
		_UpdateTitle();
	}
	_UpdateBars();
}


void
DocWindow::_ShowPane(int kind)
{
	Story* story = nullptr;
	BString title;
	switch (kind) {
		case kPaneHeader:
			story = &fDocument->header;
			title = "Header";
			break;
		case kPaneFooter:
			story = &fDocument->footer;
			title = "Footer";
			break;
		case kPaneFirstHeader:
			story = &fDocument->firstHeader;
			title = "First Header";
			break;
		case kPaneFirstFooter:
			story = &fDocument->firstFooter;
			title = "First Footer";
			break;
		case kPaneFootnote: {
			// the footnote at or after the insertion point
			std::vector<int32> order = fDocument->FootnoteOrder();
			if (order.empty()) {
				fStatus->SetMessage("No footnotes");
				return;
			}
			int32 id = order.front();
			Pos caret = fView->GetEditor().Caret();
			const Story& body = fDocument->body;
			bool found = false;
			for (int32 p = caret.para; p < (int32)body.size() && !found; p++) {
				const Paragraph& paragraph = body[p];
				for (const quill::Run& run : paragraph.runs) {
					if (run.format.footnote != 0 && (p > caret.para
							|| run.start >= std::max(0, caret.offset - 3))) {
						id = run.format.footnote;
						found = true;
						break;
					}
				}
			}
			story = fDocument->StoryFor(kStoryFootnote, id);
			auto numbers = fDocument->FootnoteNumbers();
			title << "Footnote " << numbers[id];
			break;
		}
	}
	if (story == nullptr)
		return;
	if (fPreviewMode)
		_SetPreview(false);
	fPane->SetStory(story, title.String(), kind, this);
	_SetBarVisible(fPane, true);
	DocView* paneView = fPane->View();
	paneView->SetZoom(fView->Zoom());
	paneView->SetShowAll(fView->ShowAll());
	paneView->GetEditor().SetCaret(paneView->GetEditor().End());
	paneView->MakeFocus(true);
	paneView->Changed(true);
}


void
DocWindow::_ClosePane()
{
	if (fPane->IsHidden(fPane))
		return;
	_SetBarVisible(fPane, false);
	fPane->SetStory(nullptr, "", kPaneNone, this);
	fView->MakeFocus(true);
	fView->Changed(false);
}


// #pragma mark - formatting commands


void
DocWindow::_ApplyCharacter(BMessage* message)
{
	CharFormat format;
	FindCharFormat(message, &format);
	uint32 mask = 0;
	message->FindUInt32("mask", &mask);
	_Editor().ApplyCharFields(format, mask);
	_Changed();
}


void
DocWindow::_ApplyParagraph(BMessage* message)
{
	ParaFormat format;
	FindParaFormat(message, &format);
	uint32 mask = 0;
	message->FindUInt32("mask", &mask);
	_Editor().ApplyParaFormat([&format, mask](ParaFormat& target) {
		CopyParaFields(target, format, mask);
	});
	_Changed();
}


void
DocWindow::_ApplyDocument(BMessage* message)
{
	DocSettings old = fDocument->settings;
	DocSettings settings = old;
	FindSettings(message, &settings);
	if (settings == old)
		return;
	fDocument->settings = settings;
	fView->GetEditor().DocumentChanged("Document Format", old);
	fView->Relayout();
	if (fPane->View() != nullptr)
		fPane->View()->Relayout();
	if (fPreviewMode)
		fPreview->Invalidate();
	_UpdateBars();
}


void
DocWindow::_SuperSub(bool super)
{
	Editor& editor = _Editor();
	CharFormat format = editor.SelectionCharFormat(nullptr);
	bool on = super ? format.position > 0 : format.position < 0;
	// Word 5: raise or lower by 3 points and use the next smaller size;
	// turning it off restores the next larger size.
	editor.ApplyCharFormat([on, super](CharFormat& f) {
		if (on) {
			f.position = 0;
			f.size = NextLargerSize(f.size);
		} else {
			if (f.position == 0)
				f.size = NextSmallerSize(f.size);
			f.position = super ? 3 : -2;
		}
	});
	_Changed();
}


void
DocWindow::_InsertSpecial(uint32 c, const char* name, bool pane)
{
	DocView* view = pane && fPane->View() != nullptr ? fPane->View()
		: _ActiveView();
	view->GetEditor().InsertSpecial(c, nullptr, name);
	view->Changed();
	if (view != fView)
		fView->Changed(false);
	view->MakeFocus(true);
}


void
DocWindow::_InsertDateTime(bool date, bool pane)
{
	DocView* view = pane && fPane->View() != nullptr ? fPane->View()
		: _ActiveView();
	if (view != fView) {
		// headers and footers get fields that are updated when printed
		_InsertSpecial(date ? kFieldDateChar : kFieldTimeChar,
			date ? "Insert Date" : "Insert Time", pane);
		return;
	}
	time_t now = time(nullptr);
	std::string text = date ? FormatDate(now, false) : FormatTime(now);
	Editor& editor = view->GetEditor();
	Story fragment = FragmentFromText(text, editor.InsertionFormat(),
		editor.GetStory()[editor.SelStart().para].format);
	editor.ReplaceSelection(fragment, date ? "Insert Date" : "Insert Time");
	view->Changed();
}


void
DocWindow::_InsertFootnote()
{
	if (_ActiveView() != fView) {
		fView->MakeFocus(true);
	}
	Editor& editor = fView->GetEditor();
	if (fDocument->body[editor.Caret().para].kind == kParaRowEnd)
		return;
	int32 id = fDocument->nextFootnoteId++;
	CharFormat reference = editor.InsertionFormat();
	reference.footnote = id;
	reference.position = 3;
	reference.size = NextSmallerSize(reference.size);

	int32 style = std::max<int32>(0, fDocument->styles.Find("footnote text"));
	Story note = fDocument->NewStory(style);
	CharFormat noteText = note[0].FormatAt(0);
	CharFormat noteReference = noteText;
	noteReference.footnote = id;
	noteReference.position = 3;
	noteReference.size = NextSmallerSize(noteText.size);
	note[0].Insert(0, Utf8(kFootnoteChar), noteReference);
	note[0].Insert(note[0].Length(), " ", noteText);
	fDocument->footnotes[id] = note;

	editor.InsertSpecial(kFootnoteChar, &reference, "Footnote");
	fView->Changed();

	fPane->SetStory(fDocument->StoryFor(kStoryFootnote, id), "Footnotes",
		kPaneFootnote, this);
	_SetBarVisible(fPane, true);
	DocView* paneView = fPane->View();
	paneView->SetZoom(fView->Zoom());
	paneView->SetShowAll(fView->ShowAll());
	paneView->GetEditor().SetCaret(paneView->GetEditor().End());
	paneView->GetEditor().SetTypingFormat(noteText);
	paneView->MakeFocus(true);
	paneView->Changed(true);
}


void
DocWindow::_TableCommand()
{
	Editor& editor = _Editor();
	gPrefsLock.Lock();
	int unit = gPrefs.unit;
	gPrefsLock.Unlock();
	if (editor.GetStory()[editor.Caret().para].IsTablePart()) {
		ShowTableToTextDialog(this);
		return;
	}
	if (editor.HasSelection()) {
		TextTableAnalysis analysis = AnalyzeTextForTable(editor);
		if (analysis.convertible) {
			ShowTextToTableDialog(this, analysis, unit);
			return;
		}
	}
	ShowInsertTableDialog(this, unit);
}


void
DocWindow::_InsertTable(int32 rows, int32 columns, float columnWidth)
{
	rows = std::max<int32>(1, std::min<int32>(rows, 200));
	columns = std::max<int32>(1, std::min<int32>(columns, 31));
	DocView* view = _ActiveView();
	Editor& editor = view->GetEditor();
	if (editor.GetStory()[editor.Caret().para].IsTablePart())
		return;
	editor.BeginBulk("Insert Table");
	if (editor.HasSelection())
		editor.DeleteSelection("Clear");
	if (editor.Caret().offset > 0)
		editor.InsertParagraphBreak(true);

	float width = columnWidth > 0 ? columnWidth
		: view->Layout().LayoutWidth() / columns;
	RowFormat row;
	row.gap = 5.4f;
	row.leftEdge = -row.gap;
	for (int32 c = 0; c < columns; c++) {
		CellFormat cell;
		cell.right = (c + 1) * width + row.gap;
		row.cells.push_back(cell);
	}
	Story fragment;
	int32 style = editor.GetStory()[editor.Caret().para].format.style;
	for (int32 r = 0; r < rows; r++) {
		for (int32 c = 0; c < columns; c++) {
			Paragraph cell = fDocument->NewParagraph(style);
			cell.kind = kParaCell;
			cell.format.inTable = true;
			fragment.push_back(cell);
		}
		Paragraph end = fDocument->NewParagraph(style);
		end.kind = kParaRowEnd;
		end.format.inTable = true;
		end.row = row;
		fragment.push_back(end);
	}
	fragment.push_back(fDocument->NewParagraph(style));
	Pos start = editor.Caret();
	editor.ReplaceSelection(fragment, "Insert Table");
	editor.Select(start, start);
	editor.EndBulk();
	view->Changed();
}


bool
DocWindow::_CurrentRow(int32* first, int32* end)
{
	Editor& editor = _Editor();
	const Story& story = editor.GetStory();
	int32 para = editor.Caret().para;
	if (!story[para].IsTablePart())
		return false;
	int32 start = para;
	if (story[start].kind == kParaRowEnd)
		start--;
	while (start > 0 && story[start - 1].kind == kParaCell)
		start--;
	int32 stop = para;
	while (stop < (int32)story.size() && story[stop].kind == kParaCell)
		stop++;
	if (stop >= (int32)story.size() || story[stop].kind != kParaRowEnd)
		return false;
	*first = start;
	*end = stop;
	return true;
}


void
DocWindow::_TableLayout(BMessage* message)
{
	int32 first, end;
	if (!_CurrentRow(&first, &end))
		return;
	DocView* view = _ActiveView();
	Editor& editor = view->GetEditor();
	Story& story = editor.GetStory();
	int32 column = std::min<int32>(editor.Caret().para - first,
		end - first - 1);
	const char* op = "";
	message->FindString("op", &op);

	// the whole table: consecutive rows
	int32 tableStart = first;
	while (tableStart > 0 && story[tableStart - 1].kind == kParaRowEnd) {
		int32 previous = tableStart - 1;
		while (previous > 0 && story[previous - 1].kind == kParaCell)
			previous--;
		tableStart = previous;
	}
	int32 tableEnd = end;
	while (tableEnd + 1 < (int32)story.size()
		&& story[tableEnd + 1].kind == kParaCell) {
		int32 next = tableEnd + 1;
		while (next < (int32)story.size() && story[next].kind == kParaCell)
			next++;
		if (next >= (int32)story.size() || story[next].kind != kParaRowEnd)
			break;
		tableEnd = next;
	}

	editor.BeginBulk("Table Layout");
	BString operation(op);
	if (operation == "insertRow" || operation == "deleteRow") {
		if (operation == "insertRow") {
			std::vector<Paragraph> copy;
			for (int32 p = first; p <= end; p++) {
				Paragraph paragraph = story[p];
				if (paragraph.kind == kParaCell)
					paragraph.Erase(0, paragraph.Length());
				paragraph.Touch();
				copy.push_back(paragraph);
			}
			story.insert(story.begin() + first, copy.begin(), copy.end());
			editor.Select(Pos(first, 0), Pos(first, 0));
		} else {
			story.erase(story.begin() + first, story.begin() + end + 1);
			if (story.empty())
				story.push_back(fDocument->NewParagraph(0));
			int32 caret = std::min<int32>(first, (int32)story.size() - 1);
			editor.Select(Pos(caret, 0), Pos(caret, 0));
		}
	} else if (operation == "insertColumn" || operation == "deleteColumn") {
		bool insert = operation == "insertColumn";
		// walk the rows backwards so that indices stay valid
		std::vector<std::pair<int32, int32> > rows;
		for (int32 p = tableStart; p <= tableEnd; p++) {
			int32 rowStart = p;
			while (p <= tableEnd && story[p].kind == kParaCell)
				p++;
			rows.push_back(std::make_pair(rowStart, p));
		}
		for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
			int32 rowStart = it->first;
			int32 rowEnd = it->second;
			RowFormat& row = story[rowEnd].row;
			int32 cells = rowEnd - rowStart;
			int32 index = std::min<int32>(column, cells - 1);
			if (insert) {
				float width = row.cells[index].right - row.CellLeft(index);
				Paragraph cell = story[rowStart + index];
				cell.Erase(0, cell.Length());
				cell.Touch();
				story.insert(story.begin() + rowStart + index, cell);
				RowFormat& updated = story[rowEnd + 1].row;
				CellFormat format = updated.cells[index];
				updated.cells.insert(updated.cells.begin() + index, format);
				for (size_t c = index + 1; c < updated.cells.size(); c++)
					updated.cells[c].right += width;
				story[rowEnd + 1].Touch();
			} else if (cells > 1) {
				float width = row.cells[index].right - row.CellLeft(index);
				row.cells.erase(row.cells.begin() + index);
				for (size_t c = index; c < row.cells.size(); c++)
					row.cells[c].right -= width;
				story[rowEnd].Touch();
				story.erase(story.begin() + rowStart + index);
			}
		}
		editor.Select(Pos(std::min<int32>(first, (int32)story.size() - 1), 0),
			Pos(std::min<int32>(first, (int32)story.size() - 1), 0));
	} else if (operation == "set") {
		float width = -1;
		float gap = -1;
		float indent = 0;
		int32 align = -1;
		bool hasIndent = message->FindFloat("indent", &indent) == B_OK;
		message->FindFloat("width", &width);
		message->FindFloat("gap", &gap);
		message->FindInt32("align", &align);
		for (int32 p = tableStart; p <= tableEnd; p++) {
			if (story[p].kind != kParaRowEnd)
				continue;
			RowFormat& row = story[p].row;
			if (width > 0 && column < (int32)row.cells.size()) {
				float old = row.cells[column].right - row.CellLeft(column);
				float delta = width - old;
				for (size_t c = column; c < row.cells.size(); c++)
					row.cells[c].right += delta;
			}
			if (gap >= 0) {
				float leftText = row.leftEdge + row.gap;
				row.gap = gap / 2;
				row.leftEdge = leftText - row.gap;
			}
			if (hasIndent)
				row.leftEdge = indent - row.gap;
			if (align >= 0)
				row.align = (uint8)align;
			story[p].Touch();
		}
	} else if (operation == "cells") {
		// widths from the ruler for the current row
		RowFormat& row = story[end].row;
		float right;
		for (int32 i = 0; message->FindFloat("right", i, &right) == B_OK
				&& i < (int32)row.cells.size(); i++)
			row.cells[i].right = right;
		story[end].Touch();
	}
	editor.EndBulk();
	view->Changed();
}


void
DocWindow::_OpenInsertPanel(bool picture)
{
	BMessenger messenger(this);
	BMessage message(kMsgInsertPanelRefs);
	message.AddBool("picture", picture);
	delete fInsertPanel;
	fInsertPanel = new BFilePanel(B_OPEN_PANEL, &messenger, nullptr,
		B_FILE_NODE, false, &message);
	fInsertPanel->Window()->SetTitle(picture ? "Insert Picture"
		: "Insert File");
	fInsertPanel->Show();
}


void
DocWindow::_InsertPicture(const entry_ref& ref)
{
	BFile file(&ref, B_READ_ONLY);
	off_t size = 0;
	if (file.InitCheck() != B_OK || file.GetSize(&size) != B_OK
		|| size <= 0 || size > 64 * 1024 * 1024)
		return;
	std::string data;
	data.resize(size);
	if (file.Read(&data[0], size) != size)
		return;

	auto picture = std::make_shared<Picture>();
	bool png = data.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0;
	bool jpeg = data.compare(0, 2, "\xFF\xD8") == 0;
	BBitmap* bitmap = BTranslationUtils::GetBitmap(&ref);
	if (bitmap == nullptr) {
		(new BAlert("Picture", "Quill cannot read this picture.", "OK"))
			->Go(nullptr);
		return;
	}
	if (png || jpeg) {
		picture->type = png ? "png" : "jpeg";
		picture->data = data;
	} else {
		// convert anything else to PNG with the Translation Kit
		BBitmapStream stream(bitmap);
		BMallocIO output;
		BTranslatorRoster* roster = BTranslatorRoster::Default();
		if (roster->Translate(&stream, nullptr, nullptr, &output,
				B_PNG_FORMAT) != B_OK) {
			BBitmap* detached;
			stream.DetachBitmap(&detached);
			delete detached;
			return;
		}
		BBitmap* detached = nullptr;
		stream.DetachBitmap(&detached);
		bitmap = detached;
		picture->type = "png";
		picture->data.assign((const char*)output.Buffer(),
			output.BufferLength());
	}
	BRect bounds = bitmap->Bounds();
	picture->pixelWidth = bounds.IntegerWidth() + 1;
	picture->pixelHeight = bounds.IntegerHeight() + 1;
	delete bitmap;
	// 72 pixels per inch, shrunk to fit the column
	float width = picture->pixelWidth;
	float height = picture->pixelHeight;
	float maximum = fView->Layout().LayoutWidth();
	if (width > maximum) {
		height = height * maximum / width;
		width = maximum;
	}
	picture->width = width;
	picture->height = height;

	DocView* view = _ActiveView();
	Editor& editor = view->GetEditor();
	CharFormat format = editor.InsertionFormat();
	format.picture = fDocument->AddPicture(picture);
	editor.InsertSpecial(kPictureChar, &format, "Insert Picture");
	_Changed();
}


void
DocWindow::_InsertFile(const entry_ref& ref)
{
	Document source;
	int format;
	BString error;
	if (LoadDocument(ref, &source, &format, &error) != B_OK) {
		(new BAlert("Insert File", error.String(), "OK"))->Go(nullptr);
		return;
	}
	Story fragment = ImportStory(*fDocument, source, source.body);
	fragment.push_back(fDocument->NewParagraph(0));
	DocView* view = _ActiveView();
	view->GetEditor().ReplaceSelection(fragment, "Insert File");
	_Changed();
}


void
DocWindow::_Calculate()
{
	Editor& editor = _Editor();
	std::string text = editor.SelectedText();
	double value;
	std::string shown;
	if (!Calculate(text, &value, &shown)) {
		fStatus->SetMessage("Calculate: no numbers");
		beep();
		return;
	}
	if (be_clipboard->Lock()) {
		be_clipboard->Clear();
		be_clipboard->Data()->AddData("text/plain", B_MIME_TYPE,
			shown.c_str(), shown.size());
		be_clipboard->Commit();
		be_clipboard->Unlock();
	}
	BString message;
	message << "= " << shown.c_str();
	fStatus->SetMessage(message.String());
}


void
DocWindow::_WordCount()
{
	Editor& editor = fView->GetEditor();
	bool selection = editor.HasSelection();
	WordCount count = CountWords(fDocument->body, editor.SelStart(),
		editor.SelEnd());
	DocLayout& layout = fView->Layout();
	Pos start = selection ? editor.SelStart() : Pos(0, 0);
	Pos end = selection ? editor.SelEnd() : editor.End();
	int64 lines = 0;
	for (int32 p = start.para; p <= end.para && p < layout.ParaCount(); p++) {
		if (fDocument->body[p].kind == kParaRowEnd)
			continue;
		const ParaLayout& paraLayout = layout.Para(p);
		for (const LineBox& line : paraLayout.lines) {
			int32 from = paraLayout.cpByte[line.startCp];
			int32 to = paraLayout.cpByte[line.endCp];
			if (p == start.para && to <= start.offset && line.endCp > 0
				&& !(to == start.offset && line.startCp == line.endCp))
				continue;
			if (p == end.para && from >= end.offset && from > 0)
				continue;
			lines++;
		}
	}
	count.lines = lines;
	if (!selection) {
		for (const auto& entry : fDocument->footnotes) {
			WordCount note = CountWords(entry.second, Pos(0, 0), Pos(0, 0));
			count.footnoteCharacters += note.characters;
			count.footnoteWords += note.words;
		}
	}
	ShowWordCountDialog(this, count, selection);
}


// #pragma mark - find and replace


void
DocWindow::_ReadFindFields(BMessage* message)
{
	const char* text;
	if (message->FindString("what", &text) == B_OK)
		fFindWhat = text;
	if (message->FindString("with", &text) == B_OK)
		fReplaceWith = text;
	bool flag;
	if (message->FindBool("case", &flag) == B_OK)
		fFindOptions.matchCase = flag;
	if (message->FindBool("word", &flag) == B_OK)
		fFindOptions.wholeWord = flag;
	if (message->FindBool("up", &flag) == B_OK)
		fFindUp = flag;
}


bool
DocWindow::_FindNext(bool fromSelection, bool quiet)
{
	if (fFindWhat.empty())
		return false;
	DocView* view = fView;
	Editor& editor = view->GetEditor();
	SearchPattern pattern(fFindWhat, fFindOptions);
	if (!pattern.IsValid())
		return false;
	Pos start, end;
	bool forward = !fFindUp;
	Pos from = forward ? (fromSelection ? editor.SelEnd() : editor.SelStart())
		: editor.SelStart();
	Pos limit = forward ? editor.End() : Pos(0, 0);
	bool found = pattern.Find(editor.GetStory(), from, forward, limit, &start,
		&end);
	if (!found) {
		// continue from the other end, like Word after its question
		Pos wrapFrom = forward ? Pos(0, 0) : editor.End();
		found = pattern.Find(editor.GetStory(), wrapFrom, forward, forward
			? editor.End() : Pos(0, 0), &start, &end);
		if (found && !quiet)
			fStatus->SetMessage("Search continued from the other end");
	}
	if (!found) {
		if (!quiet) {
			BAlert* alert = new BAlert("Find",
				"The search text was not found.", "OK");
			alert->Go(nullptr);
		}
		return false;
	}
	view->Select(start, end);
	return true;
}


void
DocWindow::_Find(BMessage* message)
{
	_ReadFindFields(message);
	_FindNext(true, false);
}


void
DocWindow::_ReplaceOne(BMessage* message)
{
	_ReadFindFields(message);
	Editor& editor = fView->GetEditor();
	if (editor.HasSelection()) {
		SearchPattern pattern(fFindWhat, fFindOptions);
		Pos start, end;
		if (pattern.Find(editor.GetStory(), editor.SelStart(), true,
				editor.SelEnd(), &start, &end)
			&& start == editor.SelStart() && end == editor.SelEnd()) {
			const Paragraph& paragraph = editor.GetStory()[start.para];
			Story fragment = FragmentFromText(ExpandReplacement(fReplaceWith),
				paragraph.FormatAt(start.offset), paragraph.format);
			editor.ReplaceSelection(fragment, "Replace");
			fView->Changed();
		}
	}
	_FindNext(false, false);
}


void
DocWindow::_ReplaceAll(BMessage* message)
{
	_ReadFindFields(message);
	if (fFindWhat.empty())
		return;
	Editor& editor = fView->GetEditor();
	SearchPattern pattern(fFindWhat, fFindOptions);
	std::string replacement = ExpandReplacement(fReplaceWith);
	bool inSelection = editor.HasSelection();
	Pos from = inSelection ? editor.SelStart() : Pos(0, 0);
	Pos limit = inSelection ? editor.SelEnd() : editor.End();
	int32 count = 0;
	editor.BeginBulk("Replace All");
	Pos start, end;
	while (count < 100000 && pattern.Find(editor.GetStory(), from, true,
			limit, &start, &end)) {
		const Paragraph& paragraph = editor.GetStory()[start.para];
		Story fragment = FragmentFromText(replacement,
			paragraph.FormatAt(start.offset), paragraph.format);
		int32 paragraphsBefore = (int32)editor.GetStory().size();
		int32 limitTail = editor.GetStory()[limit.para].Length()
			- limit.offset;
		Pos after = editor.ReplaceRange(start, end, fragment, "Replace");
		// keep the limit at the same place relative to the end
		int32 delta = (int32)editor.GetStory().size() - paragraphsBefore;
		limit.para += delta;
		limit.para = std::min<int32>(limit.para,
			(int32)editor.GetStory().size() - 1);
		limit.offset = std::max<int32>(0,
			editor.GetStory()[limit.para].Length() - limitTail);
		count++;
		if (after == from && start == end)
			break;
		from = after;
	}
	editor.EndBulk();
	fView->Changed();
	BString text;
	text << count << (count == 1 ? " replacement" : " replacements")
		<< " made";
	fStatus->SetMessage(text.String());
	if (count == 0) {
		(new BAlert("Replace", "The search text was not found.", "OK"))
			->Go(nullptr);
	}
}


// #pragma mark - spelling


void
DocWindow::_SpellNext()
{
	Editor& editor = fView->GetEditor();
	SpellChecker* speller = quillApp->LockSpeller();
	Pos start, end;
	std::string word;
	bool found = speller->NextMisspelling(fDocument->body, fSpellPos,
		editor.End(), &start, &end, &word);
	if (!found && !fSpellFromStart && !fSpellWrapped) {
		fSpellWrapped = true;
		found = speller->NextMisspelling(fDocument->body, Pos(0, 0),
			editor.End(), &start, &end, &word);
	}
	std::vector<std::string> suggestions;
	if (found)
		suggestions = speller->Suggest(word);
	quillApp->UnlockSpeller();

	BMessage state(found ? kMsgSpellState : kMsgSpellDone);
	if (found) {
		fSpellPos = start;
		fSpellWord = word;
		fView->Select(start, end);
		state.AddString("word", word.c_str());
		for (const std::string& suggestion : suggestions)
			state.AddString("suggestion", suggestion.c_str());
	}
	fSpellDialog.SendMessage(&state);
}


void
DocWindow::_SpellCommand(BMessage* message)
{
	Editor& editor = fView->GetEditor();
	const char* word = "";
	message->FindString("word", &word);
	switch (message->what) {
		case kMsgSpellStart:
			fSpellPos = editor.HasSelection() ? editor.SelStart()
				: editor.Caret();
			fSpellFromStart = fSpellPos == Pos(0, 0);
			fSpellWrapped = false;
			_SpellNext();
			break;
		case kMsgSpellIgnore:
			fSpellPos = editor.SelEnd();
			_SpellNext();
			break;
		case kMsgSpellIgnoreAll:
			quillApp->LockSpeller()->IgnoreAll(fSpellWord);
			quillApp->UnlockSpeller();
			fSpellPos = editor.SelEnd();
			_SpellNext();
			break;
		case kMsgSpellAdd: {
			quillApp->LockSpeller()->AddWord(fSpellWord);
			quillApp->UnlockSpeller();
			gPrefsLock.Lock();
			gPrefs.customWords.push_back(fSpellWord);
			gPrefsLock.Unlock();
			fSpellPos = editor.SelEnd();
			_SpellNext();
			break;
		}
		case kMsgSpellChange:
		case kMsgSpellChangeAll: {
			std::string replacement = word;
			if (editor.HasSelection()) {
				Pos start = editor.SelStart();
				const Paragraph& paragraph = editor.GetStory()[start.para];
				Story fragment = FragmentFromText(replacement,
					paragraph.FormatAt(start.offset), paragraph.format);
				editor.ReplaceSelection(fragment, "Spelling");
				fSpellPos = editor.Caret();
			}
			if (message->what == kMsgSpellChangeAll && !fSpellWord.empty()) {
				SearchOptions options;
				options.matchCase = true;
				options.wholeWord = true;
				SearchPattern pattern(fSpellWord, options);
				Pos start, end;
				Pos from(0, 0);
				editor.BeginBulk("Spelling");
				while (pattern.Find(editor.GetStory(), from, true,
						editor.End(), &start, &end)) {
					const Paragraph& paragraph = editor.GetStory()[start.para];
					Story fragment = FragmentFromText(replacement,
						paragraph.FormatAt(start.offset), paragraph.format);
					from = editor.ReplaceRange(start, end, fragment,
						"Spelling");
				}
				editor.EndBulk();
			}
			fView->Changed();
			_SpellNext();
			break;
		}
		case kMsgSpellSuggest: {
			SpellChecker* speller = quillApp->LockSpeller();
			std::vector<std::string> suggestions = speller->Suggest(word);
			bool correct = speller->IsCorrect(word);
			quillApp->UnlockSpeller();
			BMessage state(kMsgSpellState);
			state.AddString("word", fSpellWord.c_str());
			state.AddBool("correct", correct);
			state.AddBool("suggestOnly", true);
			for (const std::string& suggestion : suggestions)
				state.AddString("suggestion", suggestion.c_str());
			fSpellDialog.SendMessage(&state);
			break;
		}
		case kMsgSpellClosed:
			fSpellDialog = BMessenger();
			break;
	}
}


// #pragma mark - glossary and styles


void
DocWindow::_GlossaryCommand(BMessage* message)
{
	const char* name = "";
	message->FindString("name", &name);
	DocView* view = _ActiveView();
	Editor& editor = view->GetEditor();
	switch (message->what) {
		case kMsgGlossaryInsert: {
			BString entry(name);
			time_t now = time(nullptr);
			if (entry == "date" || entry == "time") {
				std::string text = entry == "date" ? FormatDate(now, false)
					: FormatTime(now);
				Story fragment = FragmentFromText(text,
					editor.InsertionFormat(),
					editor.GetStory()[editor.SelStart().para].format);
				editor.ReplaceSelection(fragment, "Glossary");
			} else if (entry == "page number") {
				editor.InsertSpecial(kFieldPageChar, nullptr, "Glossary");
			} else if (entry == "print date") {
				editor.InsertSpecial(kFieldDateChar, nullptr, "Glossary");
			} else if (entry == "print time") {
				editor.InsertSpecial(kFieldTimeChar, nullptr, "Glossary");
			} else {
				gPrefsLock.Lock();
				auto it = gPrefs.glossary.find(name);
				std::string rtf = it != gPrefs.glossary.end() ? it->second
					: std::string();
				gPrefsLock.Unlock();
				if (rtf.empty())
					break;
				Document scratch;
				Story fragment;
				if (ReadRTFFragment(rtf, scratch, fragment)) {
					fragment = ImportStory(*fDocument, scratch, fragment);
					editor.ReplaceSelection(fragment, "Glossary");
				}
			}
			view->Changed();
			if (view != fView)
				fView->Changed(false);
			break;
		}
		case kMsgGlossaryDefine: {
			if (!editor.HasSelection() || name[0] == '\0')
				break;
			std::string rtf = WriteRTFFragment(*fDocument,
				editor.CopySelection());
			gPrefsLock.Lock();
			gPrefs.glossary[name] = rtf;
			gPrefsLock.Unlock();
			break;
		}
		case kMsgGlossaryDelete:
			gPrefsLock.Lock();
			gPrefs.glossary.erase(name);
			gPrefsLock.Unlock();
			break;
	}
}


void
DocWindow::_StyleCommand(BMessage* message)
{
	DocView* view = _ActiveView();
	Editor& editor = view->GetEditor();
	int32 index = -1;
	message->FindInt32("index", &index);
	switch (message->what) {
		case kMsgStyleApply:
			editor.ApplyStyle(index);
			break;
		case kMsgStyleDefine: {
			Style style;
			if (index >= 0 && index < fDocument->styles.Count())
				style = fDocument->styles.Get(index);
			const char* name;
			if (message->FindString("name", &name) == B_OK)
				style.name = name;
			message->FindInt32("basedOn", &style.basedOn);
			message->FindInt32("next", &style.next);
			FindCharFormat(message, &style.cf);
			FindParaFormat(message, &style.pf);
			if (index < 0) {
				editor.BeginBulk("Define Style", true);
				style.pf.style = fDocument->styles.Count();
				int32 added = editor.AddStyle(style);
				bool apply = false;
				message->FindBool("apply", &apply);
				if (apply)
					editor.ApplyStyle(added);
				editor.EndBulk();
			} else {
				style.pf.style = index;
				editor.RedefineStyle(index, style, "Define Style");
				bool apply = false;
				message->FindBool("apply", &apply);
				if (apply)
					editor.ApplyStyle(index);
			}
			break;
		}
		case kMsgStyleDelete:
			editor.DeleteStyle(index);
			break;
	}
	fView->Relayout();
	if (fPane->View() != nullptr)
		fPane->View()->Relayout();
	_UpdateBars();
}


// #pragma mark - scripting for tests


static std::string
Unescape(const char* text)
{
	std::string result;
	for (const char* c = text; *c; c++) {
		if (*c == '\\' && c[1] != '\0') {
			c++;
			switch (*c) {
				case 'n': result += '\n'; break;
				case 't': result += '\t'; break;
				case 'v': result += '\v'; break;
				case 'f': result += '\f'; break;
				default: result += *c; break;
			}
		} else
			result += *c;
	}
	return result;
}


void
DocWindow::_Script(BMessage* message)
{
	const char* command = "";
	message->FindString("cmd", &command);
	std::istringstream input(command);
	std::string verb;
	input >> verb;
	std::string rest;
	std::getline(input, rest);
	if (!rest.empty() && rest[0] == ' ')
		rest.erase(0, 1);
	BString result;
	DocView* view = _ActiveView();
	Editor& editor = view->GetEditor();

	if (verb == "text") {
		result = Document::PlainText(editor.GetStory()).c_str();
	} else if (verb == "type") {
		view->TypeText(Unescape(rest.c_str()));
		result = "ok";
	} else if (verb == "select") {
		int a = 0, b = 0, c = 0, d = 0;
		sscanf(rest.c_str(), "%d %d %d %d", &a, &b, &c, &d);
		view->Select(Pos(a, b), Pos(c, d));
		result = "ok";
	} else if (verb == "selectall") {
		view->Select(Pos(0, 0), editor.End());
		result = "ok";
	} else if (verb == "selection") {
		result = editor.SelectedText().c_str();
	} else if (verb == "info") {
		DocLayout& layout = fView->Layout();
		Pos caret = editor.Caret();
		result.SetToFormat("pages=%d paras=%d caret=%d,%d anchor=%d,%d "
			"mode=%d zoom=%d modified=%d footnotes=%d styles=%d columns=%d",
			(int)layout.PageCount(), (int)editor.GetStory().size(),
			(int)caret.para, (int)caret.offset, (int)editor.Anchor().para,
			(int)editor.Anchor().offset, fPreviewMode ? kModePreview : fMode,
			(int)roundf(fView->Zoom() * 100), IsModified() ? 1 : 0,
			(int)fDocument->footnotes.size(), (int)fDocument->styles.Count(),
			(int)fDocument->settings.columns);
	} else if (verb == "format") {
		uint32 same;
		CharFormat f = editor.SelectionCharFormat(&same);
		result.SetToFormat("font=%s size=%g bold=%d italic=%d underline=%d "
			"strike=%d outline=%d shadow=%d smallcaps=%d allcaps=%d "
			"hidden=%d position=%g color=%06x",
			f.font.c_str(), f.size, f.bold, f.italic, f.underline, f.strike,
			f.outline, f.shadow, f.smallCaps, f.allCaps, f.hidden,
			f.position, f.color == kColorAuto ? 0xFFFFFFu : f.color);
	} else if (verb == "para") {
		ParaFormat f = editor.SelectionParaFormat(nullptr);
		result.SetToFormat("style=%s align=%d left=%g first=%g right=%g "
			"before=%g after=%g line=%d/%g tabs=%d keepnext=%d break=%d",
			fDocument->styles.Get(f.style).name.c_str(), f.align,
			f.leftIndent, f.firstIndent, f.rightIndent, f.spaceBefore,
			f.spaceAfter, f.lineRule, f.lineValue, (int)f.tabs.size(),
			f.keepWithNext, f.pageBreakBefore);
	} else if (verb == "save") {
		BPath path(rest.c_str());
		BPath parent;
		path.GetParent(&parent);
		entry_ref directory;
		int format = fFormat;
		if (path.Path() != nullptr && strstr(path.Leaf(), ".txt") != nullptr)
			format = kFormatText;
		status_t status = get_ref_for_path(parent.Path(), &directory);
		if (status == B_OK)
			status = _SaveTo(directory, path.Leaf(), format);
		result = status == B_OK ? "ok" : strerror(status);
	} else if (verb == "undo") {
		editor.Undo();
		view->Changed();
		result = "ok";
	} else if (verb == "redo") {
		editor.Redo();
		view->Changed();
		result = "ok";
	} else if (verb == "post") {
		// post a menu command: "post Obld" or "post Oaln align=1"
		std::istringstream words(rest);
		std::string code;
		words >> code;
		if (code.size() == 4) {
			uint32 what = ((uint32)code[0] << 24) | ((uint32)code[1] << 16)
				| ((uint32)code[2] << 8) | (uint32)code[3];
			BMessage post(what);
			std::string argument;
			while (words >> argument) {
				size_t equals = argument.find('=');
				if (equals == std::string::npos)
					continue;
				std::string name = argument.substr(0, equals);
				std::string value = argument.substr(equals + 1);
				if (value.find('.') != std::string::npos)
					post.AddFloat(name.c_str(), (float)atof(value.c_str()));
				else if (!value.empty() && (isdigit((unsigned char)value[0])
						|| value[0] == '-'))
					post.AddInt32(name.c_str(), atoi(value.c_str()));
				else
					post.AddString(name.c_str(), value.c_str());
			}
			PostMessage(&post);
			result = "ok";
		} else
			result = "bad code";
	} else if (verb == "key" || verb == "keys") {
		// synthetic key presses: "key <code> [modifiers] [raw key]" or
		// "keys <text>"; they go through shortcut handling like real ones
		std::vector<std::pair<std::string, int32> > presses;
		int32 modifiers = 0;
		int32 rawKey = 0;
		if (verb == "key") {
			int code = 0;
			int mods = 0;
			int raw = 0;
			sscanf(rest.c_str(), "%d %d %d", &code, &mods, &raw);
			modifiers = mods;
			rawKey = raw;
			presses.push_back(std::make_pair(std::string(1, (char)code),
				code));
		} else {
			std::string text = Unescape(rest.c_str());
			for (size_t i = 0; i < text.size();) {
				int32 length = Utf8CharLength((unsigned char)text[i]);
				presses.push_back(std::make_pair(text.substr(i, length),
					(int32)(unsigned char)text[i]));
				i += length;
			}
		}
		for (const auto& press : presses) {
			BMessage key(B_KEY_DOWN);
			key.AddString("bytes", press.first.c_str());
			key.AddInt32("modifiers", modifiers);
			key.AddInt32("key", rawKey);
			key.AddInt32("raw_char", press.second);
			key.AddInt64("when", system_time());
			// to the preferred handler (the focus view), so that shortcuts
			// are handled exactly as for real key presses
			PostMessage(&key, nullptr);
		}
		result = "ok";
	} else if (verb == "where") {
		// view coordinates (relative to the visible area) of a position
		int para = 0, offset = 0;
		sscanf(rest.c_str(), "%d %d", &para, &offset);
		CaretInfo info;
		DocLayout& layout = view->Layout();
		if (layout.Caret(Pos(para, offset), view->Mode() == kModePage
				? kGeomPage : kGeomNormal, false, &info)) {
			BPoint point;
			if (view->Mode() == kModePage) {
				BRect page = view->PageViewRect(info.page);
				point.Set(page.left + info.x * view->Zoom(),
					page.top + (info.top + info.bottom) / 2 * view->Zoom());
			} else {
				point.Set(DocView::kNormalLeft + info.x * view->Zoom(),
					6 + (info.top + info.bottom) / 2 * view->Zoom());
			}
			point -= view->Bounds().LeftTop();
			result.SetToFormat("%d %d", (int)roundf(point.x),
				(int)roundf(point.y));
		} else
			result = "none";
	} else if (verb == "visible") {
		int para = 0;
		sscanf(rest.c_str(), "%d", &para);
		const OutlineInfo* info = view->Layout().Outline(para);
		result = info == nullptr || info->visible ? "1" : "0";
	} else if (verb == "click" || verb == "drag") {
		// "click x y [clicks] [modifiers]", "drag x1 y1 x2 y2 [modifiers]"
		float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
		int clicks = 1, mods = 0;
		if (verb == "click") {
			sscanf(rest.c_str(), "%f %f %d %d", &x1, &y1, &clicks, &mods);
			x2 = x1;
			y2 = y1;
		} else
			sscanf(rest.c_str(), "%f %f %f %f %d", &x1, &y1, &x2, &y2, &mods);
		BPoint origin = view->Bounds().LeftTop();
		BPoint from = view->ConvertToScreen(BPoint(x1, y1) + origin);
		BPoint to = view->ConvertToScreen(BPoint(x2, y2) + origin);
		auto mouse = [&](uint32 what, BPoint where, int32 buttons) {
			BMessage event(what);
			event.AddPoint("screen_where", where);
			event.AddInt32("buttons", buttons);
			event.AddInt32("modifiers", mods);
			event.AddInt32("clicks", clicks);
			event.AddInt64("when", system_time());
			if (what == B_MOUSE_MOVED)
				event.AddInt32("be:transit", B_INSIDE_VIEW);
			PostMessage(&event, view);
		};
		mouse(B_MOUSE_DOWN, from, B_PRIMARY_MOUSE_BUTTON);
		if (verb == "drag") {
			for (int step = 1; step <= 4; step++) {
				BPoint between = from + BPoint((to.x - from.x) * step / 4,
					(to.y - from.y) * step / 4);
				mouse(B_MOUSE_MOVED, between, B_PRIMARY_MOUSE_BUTTON);
			}
		}
		mouse(B_MOUSE_UP, to, 0);
		result = "ok";
	} else if (verb == "status") {
		result = fStatus->Message()[0] != '\0' ? fStatus->Message()
			: fStatus->Text();
	} else if (verb == "page") {
		result.SetToFormat("%d", (int)fView->CaretPage() + 1);
	} else if (verb == "dump") {
		result = WriteRTF(*fDocument).c_str();
	} else
		result = "unknown command";

	if (message->IsSourceWaiting()) {
		// one line for shell scripts
		result.ReplaceAll("\\", "\\\\");
		result.ReplaceAll("\n", "\\n");
		result.ReplaceAll("\v", "\\v");
		result.ReplaceAll("\t", "\\t");
		result.ReplaceAll("\f", "\\f");
		BMessage reply(B_REPLY);
		reply.AddString("result", result.String());
		message->SendReply(&reply);
	}
}


// #pragma mark - messages


void
DocWindow::MessageReceived(BMessage* message)
{
	if (message->WasDropped() && message->HasRef("refs")) {
		entry_ref ref;
		for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
			BMessage open(B_REFS_RECEIVED);
			open.AddRef("refs", &ref);
			be_app->PostMessage(&open);
		}
		return;
	}

	bool pane = false;
	message->FindBool("pane", &pane);
	DocView* view = _ActiveView();
	Editor& editor = view->GetEditor();

	switch (message->what) {
		// File
		case kMsgNew:
		case kMsgOpen:
			be_app->PostMessage(message);
			break;
		case kMsgClose:
			PostMessage(B_QUIT_REQUESTED);
			break;
		case kMsgSave:
			_Save(false);
			break;
		case kMsgSaveAs:
			_Save(true);
			break;
		case kMsgSaveFormat:
			message->FindInt32("format", (int32*)&fPanelFormat);
			break;
		case B_SAVE_REQUESTED: {
			entry_ref directory;
			const char* name;
			if (message->FindRef("directory", &directory) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				status_t status = _SaveTo(directory, name, fPanelFormat);
				_SaveDone(status == B_OK);
			}
			break;
		}
		case B_CANCEL:
			if (fQuitAfterSave) {
				fQuitAfterSave = false;
				quillApp->CancelQuit();
			}
			break;
		case kMsgSaveChoice: {
			fSaveAlertShowing = false;
			int32 which = 1;
			message->FindInt32("which", &which);
			if (which == 0) {
				fSavedChanges = fDocument->changes;
				PostMessage(B_QUIT_REQUESTED);
			} else if (which == 2) {
				fQuitAfterSave = true;
				_Save(false);
			} else
				quillApp->CancelQuit();
			break;
		}
		case kMsgSummaryInfo:
			ShowSummaryInfoDialog(this, fDocument->info,
				fHasRef ? fRef.name : fUntitledName.String());
			break;
		case kMsgSetSummary: {
			DocInfo& info = fDocument->info;
			const char* text;
			if (message->FindString("title", &text) == B_OK) info.title = text;
			if (message->FindString("subject", &text) == B_OK)
				info.subject = text;
			if (message->FindString("author", &text) == B_OK)
				info.author = text;
			if (message->FindString("version", &text) == B_OK)
				info.version = text;
			if (message->FindString("keywords", &text) == B_OK)
				info.keywords = text;
			if (message->FindString("comments", &text) == B_OK)
				info.comments = text;
			fView->GetEditor().MarkChanged();
			break;
		}
		case kMsgPrintPreview:
			_SetPreview(!fPreviewMode);
			break;
		case kMsgPreviewCommand: {
			const char* op = "";
			message->FindString("op", &op);
			BString operation(op);
			int32 step = fPreview->TwoPages() ? 2 : 1;
			if (operation == "pages")
				fPreview->SetTwoPages(!fPreview->TwoPages());
			else if (operation == "margins")
				fPreview->SetMargins(!fPreview->Margins());
			else if (operation == "next")
				fPreview->SetFirstPage(fPreview->FirstPage() + step);
			else if (operation == "previous")
				fPreview->SetFirstPage(fPreview->FirstPage() - step);
			_UpdateBars();
			break;
		}
		case kMsgSelectionChanged:
			_UpdateBars();
			break;
		case kMsgPageSetup:
			ShowPageSetupDialog(this, fDocument->settings);
			break;
		case kMsgPrint:
			_Print();
			break;
		case kMsgPrintDone: {
			fPrinting = false;
			BMessage settings;
			if (message->FindMessage("settings", &settings) == B_OK) {
				delete fPrintSettings;
				fPrintSettings = new BMessage(settings);
			}
			int32 status = B_OK;
			message->FindInt32("status", &status);
			fStatus->SetMessage(status == B_OK ? "Printed" : "");
			if (status != B_OK && status != B_CANCELED && status != B_ERROR) {
				BString text;
				text << "The document could not be printed: "
					<< strerror(status);
				(new BAlert("Print", text.String(), "OK"))->Go(nullptr);
			}
			break;
		}

		// Edit
		case kMsgUndo:
			editor.Undo();
			_Changed();
			break;
		case kMsgRedo:
		case kMsgRepeat:
			if (editor.CanRedo())
				editor.Redo();
			else if (!editor.LastTyped().empty()) {
				std::string text = editor.LastTyped();
				editor.BreakTypingSequence();
				view->TypeText(text);
			}
			_Changed();
			break;
		case B_CUT:
			view->Cut();
			break;
		case B_COPY:
			view->Copy();
			break;
		case B_PASTE:
			view->Paste();
			break;
		case kMsgClear:
			view->Clear();
			break;
		case B_SELECT_ALL:
			if (fPreviewMode)
				break;
			view->Select(Pos(0, 0), editor.End());
			break;
		case kMsgFind:
		case kMsgReplace: {
			if (fFindDialog.IsValid()) {
				BMessage mode('Fmod');
				mode.AddBool("replace", message->what == kMsgReplace);
				fFindDialog.SendMessage(&mode);
			} else {
				std::string what = fFindWhat;
				if (editor.HasSelection()
					&& editor.SelStart().para == editor.SelEnd().para)
					what = editor.SelectedText();
				BWindow* dialog = ShowFindDialog(this,
					message->what == kMsgReplace, what.c_str(),
					fReplaceWith.c_str(), fFindOptions.matchCase,
					fFindOptions.wholeWord);
				fFindDialog = BMessenger(dialog);
			}
			break;
		}
		case kMsgFindAgain:
			_FindNext(true, false);
			break;
		case kMsgFindNext:
			_Find(message);
			break;
		case kMsgReplaceOne:
			_ReplaceOne(message);
			break;
		case kMsgReplaceAll:
			_ReplaceAll(message);
			break;
		case kMsgFindClosed:
			fFindDialog = BMessenger();
			break;
		case kMsgGoTo:
			ShowGoToDialog(this, fView->Layout().PageCount());
			break;
		case kMsgGoToPage: {
			int32 page = 1;
			message->FindInt32("page", &page);
			const std::vector<PageInfo>& pages = fView->Layout().Pages();
			int32 index = page - 1;
			for (size_t i = 0; i < pages.size(); i++) {
				if (pages[i].number == page) {
					index = (int32)i;
					break;
				}
			}
			if (fPreviewMode)
				fPreview->SetFirstPage(index);
			else
				fView->GoToPage(index);
			_UpdateBars();
			break;
		}
		case kMsgGlossary:
			ShowGlossaryDialog(this, editor.HasSelection());
			break;
		case kMsgGlossaryInsert:
		case kMsgGlossaryDefine:
		case kMsgGlossaryDelete:
			_GlossaryCommand(message);
			break;

		// View
		case kMsgViewNormal:
			_SetMode(kModeNormal);
			break;
		case kMsgViewPage:
			_SetMode(kModePage);
			break;
		case kMsgViewOutline:
			_SetMode(kModeOutline);
			break;
		case kMsgOutline:
			_OutlineCommand(message);
			break;
		case kMsgToggleToolbar:
		case kMsgToggleRibbon:
		case kMsgToggleRuler: {
			BView* bar = message->what == kMsgToggleToolbar ? (BView*)fToolbar
				: message->what == kMsgToggleRibbon ? (BView*)fRibbon
				: (BView*)fRuler;
			bool visible = bar->IsHidden(bar);
			if (bar == fRuler)
				visible = !gPrefs.showRuler;
			if (!fPreviewMode && !(bar == fRuler && fMode == kModeOutline))
				_SetBarVisible(bar, visible);
			gPrefsLock.Lock();
			if (message->what == kMsgToggleToolbar)
				gPrefs.showToolbar = visible;
			else if (message->what == kMsgToggleRibbon)
				gPrefs.showRibbon = visible;
			else
				gPrefs.showRuler = visible;
			gPrefsLock.Unlock();
			_UpdateBars();
			break;
		}
		case kMsgViewHeader:
			_ShowPane(kPaneHeader);
			break;
		case kMsgViewFooter:
			_ShowPane(kPaneFooter);
			break;
		case kMsgViewFirstHeader:
			_ShowPane(kPaneFirstHeader);
			break;
		case kMsgViewFirstFooter:
			_ShowPane(kPaneFirstFooter);
			break;
		case kMsgViewFootnotes:
			_ShowPane(kPaneFootnote);
			break;
		case kMsgClosePane:
			_ClosePane();
			break;
		case kMsgShowAll: {
			bool show = !fView->ShowAll();
			fView->SetShowAll(show);
			if (fPane->View() != nullptr)
				fPane->View()->SetShowAll(show);
			_UpdateBars();
			break;
		}
		case kMsgZoom:
			ShowZoomDialog(this, (int32)roundf(fView->Zoom() * 100));
			break;
		case kMsgSetZoom: {
			int32 percent = 100;
			message->FindInt32("percent", &percent);
			fView->SetZoom(percent / 100.0f);
			if (fPane->View() != nullptr)
				fPane->View()->SetZoom(percent / 100.0f);
			gPrefsLock.Lock();
			gPrefs.zoom = percent;
			gPrefsLock.Unlock();
			_UpdateBars();
			break;
		}

		// Insert
		case kMsgInsertPageBreak:
			if (view != fView)
				break;
			editor.InsertSpecial(kPageBreakChar, nullptr, "Page Break");
			editor.InsertParagraphBreak(true);
			view->Changed();
			break;
		case kMsgInsertSectionBreak:
			if (view != fView)
				break;
			editor.InsertSpecial(kSectionBreakChar, nullptr, "Section Break");
			editor.InsertParagraphBreak(true);
			view->Changed();
			break;
		case kMsgInsertTable:
			_TableCommand();
			break;
		case kMsgCreateTable: {
			int32 rows = 2;
			int32 columns = 2;
			float width = 0;
			message->FindInt32("rows", &rows);
			message->FindInt32("columns", &columns);
			message->FindFloat("width", &width);
			_InsertTable(rows, columns, width);
			break;
		}
		case kMsgTextToTable: {
			int32 format = kTableTabs;
			int32 columns = 1;
			float width = 0;
			message->FindInt32("format", &format);
			message->FindInt32("columns", &columns);
			message->FindFloat("width", &width);
			if (ConvertTextToTable(editor, format, columns, width,
					view->Layout().LayoutWidth()))
				_Changed();
			break;
		}
		case kMsgTableToText: {
			int32 format = kTableTabs;
			message->FindInt32("format", &format);
			if (ConvertTableToText(editor, format))
				_Changed();
			break;
		}
		case kMsgInsertFootnote:
			_InsertFootnote();
			break;
		case kMsgInsertDate:
			_InsertDateTime(true, pane);
			break;
		case kMsgInsertTime:
			_InsertDateTime(false, pane);
			break;
		case kMsgInsertPageNumber:
			_InsertSpecial(kFieldPageChar, "Insert Page Number", pane);
			break;
		case kMsgInsertSymbol: {
			CharFormat format = editor.InsertionFormat();
			ShowSymbolDialog(this, format.font.c_str());
			break;
		}
		case kMsgInsertChar: {
			const char* text;
			if (message->FindString("text", &text) == B_OK) {
				view->TypeText(text);
				if (view != fView)
					fView->Changed(false);
			}
			break;
		}
		case kMsgInsertTOC: {
			if (view != fView)
				break;
			DocLayout& layout = fView->Layout();
			layout.Update();
			std::vector<int32> pages;
			const std::vector<PageInfo>& info = layout.Pages();
			for (int32 p = 0; p < layout.ParaCount(); p++) {
				int32 page = layout.Placed(p, 0).page;
				pages.push_back(page < (int32)info.size() ? info[page].number
					: page + 1);
			}
			editor.BeginBulk("Table of Contents", true);
			Story fragment = BuildTableOfContents(*fDocument, pages, 0);
			if (fragment.empty()) {
				editor.EndBulk();
				(new BAlert("Table of Contents", "The document has no "
					"headings. Apply the heading styles (or use Outline "
					"view) to the headings first.", "OK"))->Go(nullptr);
				break;
			}
			if (editor.HasSelection())
				editor.DeleteSelection("Clear");
			if (editor.Caret().offset > 0)
				editor.InsertParagraphBreak(true);
			fragment.push_back(fDocument->NewParagraph(0));
			editor.ReplaceSelection(fragment, "Table of Contents");
			editor.EndBulk();
			fView->Relayout();
			break;
		}
		case kMsgInsertPicture:
			_OpenInsertPanel(true);
			break;
		case kMsgInsertFile:
			_OpenInsertPanel(false);
			break;
		case kMsgInsertPanelRefs: {
			entry_ref ref;
			bool picture = false;
			message->FindBool("picture", &picture);
			if (message->FindRef("refs", &ref) == B_OK) {
				if (picture)
					_InsertPicture(ref);
				else
					_InsertFile(ref);
			}
			break;
		}

		// Format
		case kMsgCharacter: {
			uint32 same;
			CharFormat format = editor.SelectionCharFormat(&same);
			ShowCharacterDialog(this, format, same);
			break;
		}
		case kMsgApplyCharacter:
			_ApplyCharacter(message);
			break;
		case kMsgParagraph: {
			uint32 same;
			ParaFormat format = editor.SelectionParaFormat(&same);
			gPrefsLock.Lock();
			int unit = gPrefs.unit;
			gPrefsLock.Unlock();
			ShowParagraphDialog(this, format, same, unit);
			break;
		}
		case kMsgApplyParagraph:
			_ApplyParagraph(message);
			break;
		case kMsgTabs: {
			gPrefsLock.Lock();
			int unit = gPrefs.unit;
			gPrefsLock.Unlock();
			ShowTabsDialog(this, editor.SelectionParaFormat(nullptr), unit);
			break;
		}
		case kMsgApplyTabs: {
			std::vector<TabStop> tabs;
			FindTabs(message, &tabs);
			editor.ApplyParaFormat([&tabs](ParaFormat& format) {
				format.tabs = tabs;
			});
			_Changed();
			break;
		}
		case kMsgBorder:
			ShowBorderDialog(this, editor.SelectionParaFormat(nullptr).border);
			break;
		case kMsgApplyBorder: {
			ParaBorder border;
			FindBorder(message, &border);
			editor.ApplyParaFormat([&border](ParaFormat& format) {
				format.border = border;
			});
			_Changed();
			break;
		}
		case kMsgSection:
		case kMsgDocument: {
			gPrefsLock.Lock();
			int unit = gPrefs.unit;
			gPrefsLock.Unlock();
			if (message->what == kMsgSection)
				ShowSectionDialog(this, fDocument->settings, unit);
			else
				ShowDocumentDialog(this, fDocument->settings, unit);
			break;
		}
		case kMsgApplyDocument:
			_ApplyDocument(message);
			break;
		case kMsgApplyMargins: {
			BMessage settings;
			DocSettings s = fDocument->settings;
			message->FindFloat("left", &s.marginLeft);
			message->FindFloat("right", &s.marginRight);
			AddSettings(&settings, s);
			_ApplyDocument(&settings);
			break;
		}
		case kMsgColumns: {
			int32 columns = 1;
			message->FindInt32("columns", &columns);
			BMessage settings;
			DocSettings s = fDocument->settings;
			s.columns = columns;
			AddSettings(&settings, s);
			_ApplyDocument(&settings);
			break;
		}
		case kMsgTableLayout: {
			int32 first, end;
			if (!_CurrentRow(&first, &end))
				break;
			gPrefsLock.Lock();
			int unit = gPrefs.unit;
			gPrefsLock.Unlock();
			ShowTableLayoutDialog(this, editor.GetStory()[end].row,
				editor.Caret().para - first, unit);
			break;
		}
		case kMsgSetTableLayout:
			_TableLayout(message);
			break;
		case kMsgApplyRow: {
			BMessage cells(*message);
			cells.AddString("op", "cells");
			_TableLayout(&cells);
			break;
		}
		case kMsgStyle: {
			gPrefsLock.Lock();
			int unit = gPrefs.unit;
			gPrefsLock.Unlock();
			uint32 same;
			ParaFormat paragraph = editor.SelectionParaFormat(&same);
			ShowStyleDialog(this, fDocument->styles,
				(same & kPFStyle) ? paragraph.style : -1,
				editor.SelectionCharFormat(nullptr), paragraph, unit);
			break;
		}
		case kMsgStyleApply:
		case kMsgStyleDefine:
		case kMsgStyleDelete:
			_StyleCommand(message);
			break;
		case kMsgApplyStyle: {
			int32 index = -1;
			const char* name;
			if (message->FindString("name", &name) == B_OK)
				index = fDocument->styles.Find(name);
			else
				message->FindInt32("index", &index);
			if (index >= 0) {
				editor.ApplyStyle(index);
				_Changed();
			}
			view->MakeFocus(true);
			break;
		}
		case kMsgRevertToStyle:
			editor.RevertToStyle();
			_Changed();
			break;
		case kMsgChangeCase:
			ShowChangeCaseDialog(this);
			break;
		case kMsgDoChangeCase: {
			int32 mode = kCaseUpper;
			message->FindInt32("mode", &mode);
			editor.ChangeCase(mode);
			_Changed();
			break;
		}
		case kMsgPlainText:
			editor.PlainText();
			_Changed();
			break;
		case kMsgToggleChar: {
			int32 field = 0;
			message->FindInt32("field", &field);
			editor.ToggleCharField(field);
			_Changed();
			break;
		}
		case kMsgUnderlineKind: {
			int32 kind = kULSingle;
			message->FindInt32("kind", &kind);
			CharFormat current = editor.SelectionCharFormat(nullptr);
			CharFormat values;
			values.underline = current.underline == kind ? kULNone : kind;
			editor.ApplyCharFields(values, kCFUnderline);
			_Changed();
			break;
		}
		case kMsgSuperscript:
			_SuperSub(true);
			break;
		case kMsgSubscript:
			_SuperSub(false);
			break;
		case kMsgColor: {
			uint32 color = kColorAuto;
			message->FindUInt32("color", &color);
			CharFormat values;
			values.color = color;
			editor.ApplyCharFields(values, kCFColor);
			_Changed();
			break;
		}
		case kMsgAlign: {
			int32 align = kAlignLeft;
			message->FindInt32("align", &align);
			editor.ApplyParaFormat([align](ParaFormat& format) {
				format.align = (uint8)align;
			});
			_Changed();
			view->MakeFocus(true);
			break;
		}
		case kMsgLineSpacing: {
			float value = 1;
			if (message->HasInt32("toggle")) {
				ParaFormat current = editor.SelectionParaFormat(nullptr);
				value = current.lineRule == kLineAuto
					&& current.lineValue == 2 ? 1 : 2;
			} else
				message->FindFloat("value", &value);
			editor.ApplyParaFormat([value](ParaFormat& format) {
				format.lineRule = kLineAuto;
				format.lineValue = value;
			});
			_Changed();
			view->MakeFocus(true);
			break;
		}
		case kMsgParaSpacing: {
			float before = 0;
			if (message->HasInt32("toggle")) {
				ParaFormat current = editor.SelectionParaFormat(nullptr);
				before = current.spaceBefore == 12 ? 0 : 12;
			} else
				message->FindFloat("before", &before);
			editor.ApplyParaFormat([before](ParaFormat& format) {
				format.spaceBefore = before;
			});
			_Changed();
			view->MakeFocus(true);
			break;
		}
		case kMsgApplyIndents: {
			float left = 0, first = 0, right = 0;
			message->FindFloat("left", &left);
			message->FindFloat("first", &first);
			message->FindFloat("right", &right);
			int32 which = 0;
			message->FindInt32("which", &which);
			// the strip reports all three values of the first paragraph;
			// change only the one that was dragged in every paragraph
			editor.ApplyParaFormat([=](ParaFormat& format) {
				switch (which) {
					case 1:	// first line
						format.firstIndent = first + left - format.leftIndent;
						break;
					case 3:	// right
						format.rightIndent = right;
						break;
					default:	// left (with the first line unless Shift)
						format.firstIndent = first;
						format.leftIndent = left;
						break;
				}
			});
			_Changed();
			break;
		}
		case kMsgNest: {
			int32 in = 1;
			message->FindInt32("in", &in);
			Nest(editor, in != 0);
			_Changed();
			break;
		}
		case kMsgHanging:
			SetHangingIndent(editor);
			_Changed();
			break;

		// Font
		case kMsgFontSize: {
			float size = 12;
			if (message->FindFloat("size", &size) == B_OK && size >= 1) {
				CharFormat values;
				values.size = size;
				editor.ApplyCharFields(values, kCFSize);
				_Changed();
			}
			if (message->HasBool("refocus"))
				view->MakeFocus(true);
			break;
		}
		case kMsgFontUp:
		case kMsgFontDown: {
			bool up = message->what == kMsgFontUp;
			editor.ApplyCharFormat([up](CharFormat& format) {
				format.size = up ? NextLargerSize(format.size)
					: NextSmallerSize(format.size);
			});
			_Changed();
			break;
		}
		case kMsgFontFamily: {
			const char* family;
			if (message->FindString("family", &family) == B_OK) {
				CharFormat values;
				values.font = family;
				editor.ApplyCharFields(values, kCFFont);
				_Changed();
			}
			view->MakeFocus(true);
			break;
		}

		// Tools
		case kMsgSpelling: {
			if (fSpellDialog.IsValid()) {
				BMessage activate(B_WINDOW_ACTIVATED);
				fSpellDialog.SendMessage(&activate);
				break;
			}
			BWindow* dialog = ShowSpellingDialog(this);
			fSpellDialog = BMessenger(dialog);
			break;
		}
		case kMsgSpellStart:
		case kMsgSpellIgnore:
		case kMsgSpellIgnoreAll:
		case kMsgSpellChange:
		case kMsgSpellChangeAll:
		case kMsgSpellAdd:
		case kMsgSpellSuggest:
		case kMsgSpellClosed:
			_SpellCommand(message);
			break;
		case kMsgWordCount:
			_WordCount();
			break;
		case kMsgRenumber:
			ShowRenumberDialog(this);
			break;
		case kMsgDoRenumber: {
			RenumberOptions options;
			message->FindBool("onlyNumbered", &options.onlyNumbered);
			message->FindInt32("start", &options.start);
			message->FindInt32("format", (int32*)&options.format);
			message->FindBool("remove", &options.remove);
			Renumber(editor, options);
			_Changed();
			break;
		}
		case kMsgBullets:
			ToggleBullets(editor);
			_Changed();
			break;
		case kMsgNumbers:
			ToggleNumbers(editor);
			_Changed();
			break;
		case kMsgSort:
		case kMsgSortDescending:
			SortParagraphs(editor, message->what == kMsgSortDescending);
			_Changed();
			break;
		case kMsgCalculate:
			_Calculate();
			break;
		case kMsgRepaginate:
			fView->Relayout();
			break;
		case kMsgPreferences:
			ShowPreferencesDialog(this);
			break;
		case kMsgPrefsChanged:
			fView->Relayout();
			if (fPane->View() != nullptr)
				fPane->View()->Relayout();
			_UpdateBars();
			break;

		// Window and Help
		case kMsgShowClipboard:
			ShowClipboardWindow();
			break;
		case kMsgArrange:
			be_app->PostMessage(message);
			break;
		case kMsgAbout:
			ShowAboutWindow();
			break;
		case kMsgHelp:
		case kMsgKeyboardHelp:
			ShowHelpWindow();
			break;

		case kMsgScript:
			_Script(message);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}
