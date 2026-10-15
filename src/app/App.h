/*
 * Quill - the application: documents, open panel, shared dictionary.
 */
#ifndef QUILL_APP_H
#define QUILL_APP_H

#include "Document.h"
#include "Spell.h"

#include <Application.h>
#include <Locker.h>
#include <String.h>

#include <vector>

class BFilePanel;
class DocWindow;

struct WindowEntry {
	BWindow*	window;
	BString		title;
};

class QuillApp : public BApplication {
public:
							QuillApp();
	virtual					~QuillApp();

	virtual	void			ReadyToRun();
	virtual	void			ArgvReceived(int32 argc, char** argv);
	virtual	void			RefsReceived(BMessage* message);
	virtual	void			MessageReceived(BMessage* message);
	virtual	bool			QuitRequested();
	virtual	void			AboutRequested();

			void			NewDocument();
			status_t		OpenDocument(const entry_ref& ref);

	// Called by windows (from their threads).
			void			WindowOpened(BWindow* window, const char* title);
			void			WindowClosed(BWindow* window);
			void			TitleChanged(BWindow* window, const char* title);
			void			CancelQuit();
			bool			IsQuitting() const { return fQuitting; }
			std::vector<WindowEntry> Windows();

			quill::SpellChecker* LockSpeller();
			void			UnlockSpeller();

private:
			BRect			_NextFrame();
			DocWindow*		_FindWindow(const entry_ref& ref);
			DocWindow*		_UntouchedWindow();

			BFilePanel*		fOpenPanel;
			int32			fUntitled;
			bool			fQuitting;
			bool			fOpenedSomething;
			BLocker			fWindowLock;
			std::vector<WindowEntry> fWindows;
			BLocker			fSpellLock;
			quill::SpellChecker* fSpeller;
};

#define quillApp (static_cast<QuillApp*>(be_app))

// Reads RTF or text into a document; returns the format and an error.
status_t LoadDocument(const entry_ref& ref, quill::Document* document,
	int* format, BString* error);

#endif	// QUILL_APP_H
