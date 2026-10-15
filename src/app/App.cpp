/*
 * Quill - the application.
 */
#include "App.h"

#include "Constants.h"
#include "Dialogs.h"
#include "DocWindow.h"
#include "Fonts.h"
#include "Prefs.h"
#include "RTF.h"
#include "TextIO.h"

#include <Alert.h>
#include <Autolock.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Screen.h>

#include <algorithm>
#include <unistd.h>

using namespace quill;


status_t
LoadDocument(const entry_ref& ref, Document* document, int* format,
	BString* error)
{
	BFile file(&ref, B_READ_ONLY);
	status_t status = file.InitCheck();
	off_t size = 0;
	if (status == B_OK)
		status = file.GetSize(&size);
	if (status != B_OK) {
		error->SetToFormat("\"%s\" could not be opened: %s", ref.name,
			strerror(status));
		return status;
	}
	if (size > 256 * 1024 * 1024) {
		error->SetToFormat("\"%s\" is too large to open.", ref.name);
		return B_NO_MEMORY;
	}
	std::string data;
	data.resize(size);
	if (size > 0 && file.Read(&data[0], size) != size) {
		error->SetToFormat("\"%s\" could not be read.", ref.name);
		return B_IO_ERROR;
	}
	const unsigned char* bytes = (const unsigned char*)data.data();
	bool wordBinary = size >= 4 && ((bytes[0] == 0xFE && bytes[1] == 0x37)
		|| (bytes[0] == 0xFE && bytes[1] == 0x34)
		|| (bytes[0] == 0xD0 && bytes[1] == 0xCF && bytes[2] == 0x11
			&& bytes[3] == 0xE0));
	if (LooksLikeRTF(data)) {
		std::string message;
		size_t start = data.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
		if (!ReadRTF(data.substr(start), *document, &message)) {
			error->SetToFormat("\"%s\": %s", ref.name, message.c_str());
			return B_BAD_DATA;
		}
		*format = kFormatRTF;
	} else if (wordBinary) {
		error->SetToFormat("\"%s\" is a binary Word document. Quill reads "
			"Rich Text Format (RTF) and text files; save the document as "
			"RTF to open it here.", ref.name);
		return B_BAD_DATA;
	} else {
		ReadPlainText(data, *document);
		*format = kFormatText;
	}
	document->changes = 0;
	return B_OK;
}


QuillApp::QuillApp()
	:
	BApplication(APP_SIGNATURE),
	fOpenPanel(nullptr),
	fUntitled(0),
	fQuitting(false),
	fOpenedSomething(false),
	fWindowLock("Quill windows"),
	fSpellLock("Quill speller"),
	fSpeller(nullptr)
{
	gPrefs.Load();
	FontCache& fonts = FontCache::Get();
	font_family family;
	font_style style;
	be_plain_font->GetFamilyAndStyle(&family, &style);
	if (fonts.IsInstalled("Noto Serif"))
		gDefaultSerif = "Noto Serif";
	else
		gDefaultSerif = family;
	if (fonts.IsInstalled("Noto Sans"))
		gDefaultSans = "Noto Sans";
	else
		gDefaultSans = family;
	if (!gPrefs.defaultFont.empty() && fonts.IsInstalled(gPrefs.defaultFont))
		gDefaultSerif = gPrefs.defaultFont;
}


QuillApp::~QuillApp()
{
	delete fOpenPanel;
	delete fSpeller;
}


void
QuillApp::ReadyToRun()
{
	if (!fOpenedSomething)
		NewDocument();
}


void
QuillApp::ArgvReceived(int32 argc, char** argv)
{
	BMessage* message = CurrentMessage();
	const char* cwd = nullptr;
	if (message != nullptr)
		message->FindString("cwd", &cwd);
	char buffer[B_PATH_NAME_LENGTH];
	if (cwd == nullptr && getcwd(buffer, sizeof(buffer)) != nullptr)
		cwd = buffer;
	for (int32 i = 1; i < argc; i++) {
		BPath path;
		if (argv[i][0] == '/' || cwd == nullptr)
			path.SetTo(argv[i]);
		else
			path.SetTo(cwd, argv[i]);
		entry_ref ref;
		if (get_ref_for_path(path.Path(), &ref) != B_OK)
			continue;
		BEntry entry(&ref);
		if (!entry.Exists()) {
			// a new document that will be saved under this name
			continue;
		}
		OpenDocument(ref);
	}
}


void
QuillApp::RefsReceived(BMessage* message)
{
	entry_ref ref;
	for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
		OpenDocument(ref);
}


BRect
QuillApp::_NextFrame()
{
	gPrefsLock.Lock();
	BRect frame = gPrefs.windowFrame;
	gPrefsLock.Unlock();
	BRect screen = BScreen().Frame();
	if (!frame.IsValid() || frame.Width() < 300 || frame.Height() < 200)
		frame.Set(60, 50, 760, 650);
	int32 count;
	{
		BAutolock lock(fWindowLock);
		count = (int32)fWindows.size();
	}
	frame.OffsetBy(20 * (count % 8), 20 * (count % 8));
	if (frame.right > screen.right)
		frame.OffsetBy(screen.right - frame.right, 0);
	if (frame.bottom > screen.bottom)
		frame.OffsetBy(0, screen.bottom - frame.bottom);
	if (frame.left < screen.left + 4)
		frame.OffsetTo(screen.left + 4, frame.top);
	if (frame.top < screen.top + 24)
		frame.OffsetTo(frame.left, screen.top + 24);
	return frame;
}


void
QuillApp::NewDocument()
{
	fOpenedSomething = true;
	Document* document = new Document();
	gPrefsLock.Lock();
	float size = gPrefs.defaultSize;
	gPrefsLock.Unlock();
	document->Reset(gDefaultSerif, gDefaultSans);
	if (size >= 4 && size <= 144) {
		for (Style& style : document->styles.styles) {
			if (style.cf.size == 12)
				style.cf.size = size;
		}
		document->body = document->NewStory(0);
	}
	document->changes = 0;
	BString title;
	title << "Untitled" << ++fUntitled;
	DocWindow* window = new DocWindow(_NextFrame(), document, nullptr,
		title.String(), kFormatRTF);
	window->Show();
}


DocWindow*
QuillApp::_FindWindow(const entry_ref& ref)
{
	BAutolock lock(fWindowLock);
	for (const WindowEntry& entry : fWindows) {
		DocWindow* window = dynamic_cast<DocWindow*>(entry.window);
		if (window != nullptr && window->HasRef(ref))
			return window;
	}
	return nullptr;
}


DocWindow*
QuillApp::_UntouchedWindow()
{
	DocWindow* window;
	BMessenger messenger;
	{
		BAutolock lock(fWindowLock);
		if (fWindows.size() != 1)
			return nullptr;
		window = dynamic_cast<DocWindow*>(fWindows[0].window);
		if (window == nullptr)
			return nullptr;
		messenger = BMessenger(window);
	}
	// Lock through the messenger: the window may be closing right now.
	// (LockWithTimeout() returns a status_t, B_OK when it got the lock.)
	if (messenger.LockTargetWithTimeout(100000) != B_OK)
		return nullptr;
	bool untouched = window->IsUntouched();
	window->Unlock();
	return untouched ? window : nullptr;
}


status_t
QuillApp::OpenDocument(const entry_ref& original)
{
	fOpenedSomething = true;
	BEntry entry(&original, true);
	entry_ref ref;
	if (entry.GetRef(&ref) != B_OK)
		ref = original;
	DocWindow* existing = _FindWindow(ref);
	if (existing != nullptr) {
		existing->Activate();
		return B_OK;
	}

	Document* document = new Document();
	int format = kFormatRTF;
	BString error;
	status_t status = LoadDocument(ref, document, &format, &error);
	if (status != B_OK) {
		delete document;
		BAlert* alert = new BAlert("Open", error.String(), "OK", nullptr,
			nullptr, B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->Go(nullptr);
		return status;
	}

	BRect frame = _NextFrame();
	DocWindow* untouched = _UntouchedWindow();
	BMessenger untouchedMessenger;
	if (untouched != nullptr) {
		frame = untouched->Frame();
		untouchedMessenger = BMessenger(untouched);
	}
	// The new window registers itself before the empty one is closed, so
	// the application never sees its window list run empty.
	DocWindow* window = new DocWindow(frame, document, &ref, ref.name,
		format);
	window->Show();
	if (untouchedMessenger.IsValid())
		untouchedMessenger.SendMessage(B_QUIT_REQUESTED);

	BPath path(&entry);
	gPrefsLock.Lock();
	gPrefs.AddRecent(path.Path());
	gPrefsLock.Unlock();
	return B_OK;
}


void
QuillApp::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgNew:
			NewDocument();
			break;
		case kMsgOpen:
			if (fOpenPanel == nullptr)
				fOpenPanel = new BFilePanel(B_OPEN_PANEL);
			fOpenPanel->Show();
			break;
		case kMsgOpenRecent: {
			const char* path;
			entry_ref ref;
			if (message->FindString("path", &path) == B_OK
				&& get_ref_for_path(path, &ref) == B_OK)
				OpenDocument(ref);
			break;
		}
		case kMsgActivateWindow: {
			void* pointer;
			if (message->FindPointer("window", &pointer) == B_OK) {
				BAutolock lock(fWindowLock);
				for (const WindowEntry& entry : fWindows) {
					if (entry.window == pointer)
						entry.window->Activate();
				}
			}
			break;
		}
		case kMsgArrange: {
			std::vector<WindowEntry> windows = Windows();
			if (windows.empty())
				break;
			BRect screen = BScreen().Frame();
			screen.InsetBy(6, 6);
			screen.top += 20;
			float height = screen.Height() / windows.size();
			for (size_t i = 0; i < windows.size(); i++) {
				BWindow* window = windows[i].window;
				BMessenger messenger(window);
				if (messenger.LockTargetWithTimeout(500000) == B_OK) {
					window->MoveTo(screen.left, screen.top + i * height);
					window->ResizeTo(screen.Width(), height - 30);
					window->Unlock();
				}
			}
			break;
		}
		default:
			BApplication::MessageReceived(message);
			break;
	}
}


bool
QuillApp::QuitRequested()
{
	fQuitting = true;
	std::vector<WindowEntry> windows = Windows();
	for (const WindowEntry& entry : windows) {
		BWindow* window = entry.window;
		// the copy of the list may name a window that has closed since
		BMessenger messenger(window);
		if (!messenger.LockTarget())
			continue;
		if (window->QuitRequested()) {
			window->Quit();
		} else {
			window->Unlock();
			return false;
		}
	}
	gPrefsLock.Lock();
	gPrefs.Save();
	gPrefsLock.Unlock();
	return true;
}


void
QuillApp::AboutRequested()
{
	ShowAboutWindow();
}


void
QuillApp::WindowOpened(BWindow* window, const char* title)
{
	BAutolock lock(fWindowLock);
	WindowEntry entry;
	entry.window = window;
	entry.title = title;
	fWindows.push_back(entry);
}


void
QuillApp::WindowClosed(BWindow* window)
{
	bool empty;
	{
		BAutolock lock(fWindowLock);
		fWindows.erase(std::remove_if(fWindows.begin(), fWindows.end(),
			[window](const WindowEntry& entry) {
				return entry.window == window;
			}), fWindows.end());
		empty = fWindows.empty();
	}
	gPrefsLock.Lock();
	gPrefs.Save();
	gPrefsLock.Unlock();
	if (empty || fQuitting)
		PostMessage(B_QUIT_REQUESTED);
}


void
QuillApp::TitleChanged(BWindow* window, const char* title)
{
	BAutolock lock(fWindowLock);
	for (WindowEntry& entry : fWindows) {
		if (entry.window == window)
			entry.title = title;
	}
}


void
QuillApp::CancelQuit()
{
	fQuitting = false;
}


std::vector<WindowEntry>
QuillApp::Windows()
{
	BAutolock lock(fWindowLock);
	return fWindows;
}


SpellChecker*
QuillApp::LockSpeller()
{
	fSpellLock.Lock();
	if (fSpeller == nullptr) {
		fSpeller = new SpellChecker();
		BPath path;
		if (find_directory(B_SYSTEM_DATA_DIRECTORY, &path) == B_OK) {
			path.Append("spell_check/word_dictionary");
			BDirectory directory(path.Path());
			BEntry entry;
			while (directory.GetNextEntry(&entry) == B_OK) {
				BPath file(&entry);
				fSpeller->LoadWordList(file.Path());
			}
		}
		gPrefsLock.Lock();
		for (const std::string& word : gPrefs.customWords)
			fSpeller->AddWord(word);
		gPrefsLock.Unlock();
	}
	gPrefsLock.Lock();
	fSpeller->ignoreUppercase = gPrefs.ignoreUppercase;
	fSpeller->ignoreWithNumbers = gPrefs.ignoreNumbers;
	gPrefsLock.Unlock();
	return fSpeller;
}


void
QuillApp::UnlockSpeller()
{
	fSpellLock.Unlock();
}


int
main()
{
	QuillApp app;
	app.Run();
	return 0;
}
