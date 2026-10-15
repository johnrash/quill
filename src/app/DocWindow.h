/*
 * Quill - the document window with Word 5.1's menus, Toolbar, Ribbon,
 * Ruler, the document view and the page number box.
 */
#ifndef QUILL_DOCWINDOW_H
#define QUILL_DOCWINDOW_H

#include "DocView.h"
#include "Search.h"

#include <Entry.h>
#include <Messenger.h>
#include <Window.h>

#include <string>

class BFilePanel;
class BMenu;
class BMenuBar;
class BMenuField;
class BMenuItem;
class BScrollBar;
class BStringView;
class OutlineBar;
class PreviewView;
class Ribbon;
class Ruler;
class StatusView;
class StoryPane;
class Toolbar;

class DocWindow : public BWindow, public DocViewListener {
public:
							DocWindow(BRect frame, quill::Document* document,
								const entry_ref* ref, const char* title,
								int format);
	virtual					~DocWindow();

	virtual	bool			QuitRequested();
	virtual	void			MessageReceived(BMessage* message);
	virtual	void			MenusBeginning();
	virtual	void			FrameMoved(BPoint where);
	virtual	void			FrameResized(float width, float height);

	virtual	void			DocViewChanged(DocView* view, bool content);
	virtual	void			DocViewScrolled(DocView* view);
	virtual	void			DocViewContextMenu(DocView* view, BPoint where);

			bool			HasRef(const entry_ref& ref) const;
			bool			IsUntouched() const;
			bool			IsModified() const;

private:
			void			_BuildMenus();
			void			_BuildLayout();
			DocView*		_ActiveView();
			quill::Editor&	_Editor() { return _ActiveView()->GetEditor(); }
			void			_Changed(bool scroll = true);
			void			_UpdateBars();
			void			_UpdateStatus();
			void			_UpdateTitle();
			void			_UpdateRecentMenu();
			void			_UpdateWindowMenu();
			void			_UpdateFontMenu();

			void			_Save(bool saveAs);
			void			_ShowSavePanel();
			status_t		_SaveTo(const entry_ref& directory,
								const char* name, int format);
			void			_SaveDone(bool success);
			void			_Print();

			void			_SetMode(int mode);
			void			_SetPreview(bool preview);
			void			_ShowPane(int kind);
			void			_ClosePane();
			void			_SetBarVisible(BView* bar, bool visible);
			void			_ShowRulerOrOutlineBar();
			void			_OutlineCommand(BMessage* message);

			void			_ApplyCharacter(BMessage* message);
			void			_ApplyParagraph(BMessage* message);
			void			_ApplyDocument(BMessage* message);
			void			_SuperSub(bool super);
			void			_InsertSpecial(uint32 c, const char* name,
								bool pane);
			void			_InsertDateTime(bool date, bool pane);
			void			_InsertFootnote();
			void			_InsertTable(int32 rows, int32 columns,
								float columnWidth);
			void			_TableCommand();
			void			_TableLayout(BMessage* message);
			bool			_CurrentRow(int32* first, int32* end);
			void			_InsertPicture(const entry_ref& ref);
			void			_InsertFile(const entry_ref& ref);
			void			_OpenInsertPanel(bool picture);
			void			_Calculate();
			void			_WordCount();

			void			_Find(BMessage* message);
			bool			_FindNext(bool fromSelection, bool quiet);
			void			_ReplaceOne(BMessage* message);
			void			_ReplaceAll(BMessage* message);
			void			_ReadFindFields(BMessage* message);

			void			_SpellNext();
			void			_SpellCommand(BMessage* message);
			void			_GlossaryCommand(BMessage* message);
			void			_StyleCommand(BMessage* message);
			void			_Script(BMessage* message);

			quill::Document* fDocument;
			entry_ref		fRef;
			bool			fHasRef;
			int				fFormat;
			uint64			fSavedChanges;
			BString			fUntitledName;

			BMenuBar*		fMenuBar;
			BMenuItem*		fUndoItem;
			BMenuItem*		fRedoItem;
			BMenuItem*		fCutItem;
			BMenuItem*		fCopyItem;
			BMenuItem*		fClearItem;
			BMenuItem*		fPasteItem;
			BMenuItem*		fNormalItem;
			BMenuItem*		fOutlineItem;
			BMenuItem*		fPageItem;
			BMenuItem*		fToolbarItem;
			BMenuItem*		fRibbonItem;
			BMenuItem*		fRulerItem;
			BMenuItem*		fShowAllItem;
			BMenuItem*		fFirstHeaderItem;
			BMenuItem*		fFirstFooterItem;
			BMenuItem*		fBoldItem;
			BMenuItem*		fItalicItem;
			BMenuItem*		fUnderlineItem;
			BMenuItem*		fTableLayoutItem;
			BMenuItem*		fInsertTableItem;
			BMenuItem*		fPreviewItem;
			BMenu*			fFileMenu;
			BMenu*			fFontMenu;
			BMenu*			fWindowMenu;
			int32			fRecentStart;
			int32			fFontListStart;

			Toolbar*		fToolbar;
			Ribbon*			fRibbon;
			Ruler*			fRuler;
			OutlineBar*		fOutlineBar;
			int32			fOutlineLevel;
			bool			fOutlineFirstLine;
			bool			fOutlineFormatting;
			DocView*		fView;
			BScrollBar*		fHScroll;
			BScrollBar*		fVScroll;
			StatusView*		fStatus;
			BView*			fDocumentArea;
			StoryPane*		fPane;
			BView*			fPreviewArea;
			PreviewView*	fPreview;
			BStringView*	fPreviewPage;
			PictureCache	fPictures;

			int				fMode;
			bool			fPreviewMode;
			BFilePanel*		fSavePanel;
			BMenuField*		fFormatField;
			int				fPanelFormat;
			BFilePanel*		fInsertPanel;
			BMessage*		fPrintSettings;
			bool			fPrinting;
			bool			fQuitAfterSave;
			bool			fSaveAlertShowing;

			BMessenger		fFindDialog;
			std::string		fFindWhat;
			std::string		fReplaceWith;
			quill::SearchOptions fFindOptions;
			bool			fFindUp;

			BMessenger		fSpellDialog;
			quill::Pos		fSpellPos;
			std::string		fSpellWord;
			bool			fSpellFromStart;
			bool			fSpellWrapped;
};

#endif	// QUILL_DOCWINDOW_H
