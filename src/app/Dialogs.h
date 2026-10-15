/*
 * Quill - the dialogs of Word's menus. Every dialog is a window of its own
 * that sends its result to the document window as a message, so the
 * document is only ever touched by the document window's thread.
 */
#ifndef QUILL_DIALOGS_H
#define QUILL_DIALOGS_H

#include "Format.h"
#include "Document.h"
#include "Tools.h"

#include <Messenger.h>
#include <Window.h>

#include <string>
#include <vector>

class DialogWindow : public BWindow {
public:
							DialogWindow(BWindow* parent, const char* title,
								uint32 flags = 0);
	virtual	void			Show();
			void			SendToParent(BMessage* message);
			BWindow*		Parent() const { return fParent; }
	virtual	bool			QuitRequested();

protected:
			BWindow*		fParent;
			BMessenger		fMessenger;
};

// Format menu
void ShowCharacterDialog(BWindow* parent, const quill::CharFormat& format,
	uint32 same);
void ShowParagraphDialog(BWindow* parent, const quill::ParaFormat& format,
	uint32 same, int unit);
void ShowTabsDialog(BWindow* parent, const quill::ParaFormat& format,
	int unit);
void ShowBorderDialog(BWindow* parent, const quill::ParaBorder& border);
void ShowDocumentDialog(BWindow* parent, const quill::DocSettings& settings,
	int unit);
void ShowSectionDialog(BWindow* parent, const quill::DocSettings& settings,
	int unit);
void ShowPageSetupDialog(BWindow* parent, const quill::DocSettings& settings);
void ShowStyleDialog(BWindow* parent, const quill::StyleSheet& styles,
	int32 current, const quill::CharFormat& selectionChar,
	const quill::ParaFormat& selectionPara, int unit);
void ShowChangeCaseDialog(BWindow* parent);
void ShowTableLayoutDialog(BWindow* parent, const quill::RowFormat& row,
	int32 cell, int unit);

// Edit, View, Insert, Tools menus
BWindow* ShowFindDialog(BWindow* parent, bool replace, const char* what,
	const char* with, bool matchCase, bool wholeWord);
void ShowGoToDialog(BWindow* parent, int32 pageCount);
void ShowZoomDialog(BWindow* parent, int32 percent);
void ShowInsertTableDialog(BWindow* parent, int unit);
void ShowTextToTableDialog(BWindow* parent,
	const quill::TextTableAnalysis& analysis, int unit);
void ShowTableToTextDialog(BWindow* parent);
void ShowSymbolDialog(BWindow* parent, const char* font);
void ShowWordCountDialog(BWindow* parent, const quill::WordCount& count,
	bool selection);
void ShowRenumberDialog(BWindow* parent);
void ShowSummaryInfoDialog(BWindow* parent, const quill::DocInfo& info,
	const char* fileName);
void ShowGlossaryDialog(BWindow* parent, bool hasSelection);
BWindow* ShowSpellingDialog(BWindow* parent);
void ShowPreferencesDialog(BWindow* parent);
void ShowAboutWindow();
void ShowHelpWindow();
void ShowClipboardWindow();

// Messages the dialogs send
enum {
	kMsgGoToPage			= 'Ggto',	// "page"
	kMsgSetZoom				= 'Gzom',	// "percent"
	kMsgCreateTable			= 'Gtbl',	// "rows", "columns"
	kMsgDoChangeCase		= 'Gcas',	// "mode"
	kMsgDoRenumber			= 'Gren',	// RenumberOptions fields
	kMsgSetSummary			= 'Gsum',	// DocInfo fields
	kMsgStyleApply			= 'Gsap',	// "index"
	kMsgStyleDefine			= 'Gsdf',	// "index" (-1 new), "name", ...
	kMsgStyleDelete			= 'Gsdl',	// "index"
	kMsgGlossaryInsert		= 'Ggli',	// "name"
	kMsgGlossaryDefine		= 'Ggld',	// "name"
	kMsgGlossaryDelete		= 'Ggdl',	// "name"
	kMsgSpellStart			= 'Gsst',
	kMsgSpellIgnore			= 'Gsig',
	kMsgSpellIgnoreAll		= 'Gsia',
	kMsgSpellChange			= 'Gsch',	// "word"
	kMsgSpellChangeAll		= 'Gsca',	// "word"
	kMsgSpellAdd			= 'Gsad',
	kMsgSpellSuggest		= 'Gssg',	// "word" -> reply with suggestions
	kMsgSpellState			= 'Gsts',	// to the dialog: "word", "suggestion"
	kMsgSpellDone			= 'Gsdn',
	kMsgSpellClosed			= 'Gscl',
	kMsgFindClosed			= 'Gfcl',
	kMsgSetTableLayout		= 'Gtly',
	kMsgTextToTable			= 'Gt2t',	// "format", "columns", "width"
	kMsgTableToText			= 'Gtt2'	// "format"
};

#endif	// QUILL_DIALOGS_H
