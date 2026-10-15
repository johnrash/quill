/*
 * Quill - preferences.
 */
#include "Prefs.h"

#include "Format.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

Prefs gPrefs;
BLocker gPrefsLock("Quill prefs");


Prefs::Prefs()
	:
	unit(quill::kUnitInch),
	smartQuotes(true),
	showToolbar(true),
	showRibbon(true),
	showRuler(true),
	showHidden(false),
	textBoundaries(true),
	tableGridlines(true),
	openInPageView(false),
	makeBackup(false),
	ignoreUppercase(true),
	ignoreNumbers(true),
	defaultSize(12),
	zoom(100),
	windowFrame(60, 50, 760, 650)
{
}


static status_t
SettingsPath(BPath& path)
{
	status_t status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (status != B_OK)
		return status;
	path.Append("Quill");
	create_directory(path.Path(), 0755);
	// tests use a settings file of their own
	const char* name = getenv("QUILL_SETTINGS");
	return path.Append(name != nullptr && name[0] != '\0' && strchr(name, '/')
		== nullptr ? name : "Quill settings");
}


void
Prefs::Load()
{
	BPath path;
	if (SettingsPath(path) != B_OK)
		return;
	BFile file(path.Path(), B_READ_ONLY);
	BMessage settings;
	if (file.InitCheck() != B_OK || settings.Unflatten(&file) != B_OK)
		return;
	int32 value;
	bool flag;
	float number;
	BRect rect;
	const char* text;
	if (settings.FindInt32("unit", &value) == B_OK) unit = value;
	if (settings.FindBool("smartQuotes", &flag) == B_OK) smartQuotes = flag;
	if (settings.FindBool("showToolbar", &flag) == B_OK) showToolbar = flag;
	if (settings.FindBool("showRibbon", &flag) == B_OK) showRibbon = flag;
	if (settings.FindBool("showRuler", &flag) == B_OK) showRuler = flag;
	if (settings.FindBool("showHidden", &flag) == B_OK) showHidden = flag;
	if (settings.FindBool("textBoundaries", &flag) == B_OK)
		textBoundaries = flag;
	if (settings.FindBool("tableGridlines", &flag) == B_OK)
		tableGridlines = flag;
	if (settings.FindBool("openInPageView", &flag) == B_OK)
		openInPageView = flag;
	if (settings.FindBool("makeBackup", &flag) == B_OK) makeBackup = flag;
	if (settings.FindBool("ignoreUppercase", &flag) == B_OK)
		ignoreUppercase = flag;
	if (settings.FindBool("ignoreNumbers", &flag) == B_OK)
		ignoreNumbers = flag;
	if (settings.FindString("userName", &text) == B_OK) userName = text;
	if (settings.FindString("defaultFont", &text) == B_OK) defaultFont = text;
	if (settings.FindFloat("defaultSize", &number) == B_OK)
		defaultSize = number;
	if (settings.FindFloat("zoom", &number) == B_OK) zoom = number;
	if (settings.FindRect("windowFrame", &rect) == B_OK) windowFrame = rect;
	recent.clear();
	for (int32 i = 0; settings.FindString("recent", i, &text) == B_OK; i++)
		recent.push_back(text);
	glossary.clear();
	const char* name;
	for (int32 i = 0; settings.FindString("glossaryName", i, &name) == B_OK
			&& settings.FindString("glossaryText", i, &text) == B_OK; i++)
		glossary[name] = text;
	customWords.clear();
	for (int32 i = 0; settings.FindString("word", i, &text) == B_OK; i++)
		customWords.push_back(text);
}


void
Prefs::Save()
{
	BMessage settings;
	settings.AddInt32("unit", unit);
	settings.AddBool("smartQuotes", smartQuotes);
	settings.AddBool("showToolbar", showToolbar);
	settings.AddBool("showRibbon", showRibbon);
	settings.AddBool("showRuler", showRuler);
	settings.AddBool("showHidden", showHidden);
	settings.AddBool("textBoundaries", textBoundaries);
	settings.AddBool("tableGridlines", tableGridlines);
	settings.AddBool("openInPageView", openInPageView);
	settings.AddBool("makeBackup", makeBackup);
	settings.AddBool("ignoreUppercase", ignoreUppercase);
	settings.AddBool("ignoreNumbers", ignoreNumbers);
	settings.AddString("userName", userName.c_str());
	settings.AddString("defaultFont", defaultFont.c_str());
	settings.AddFloat("defaultSize", defaultSize);
	settings.AddFloat("zoom", zoom);
	settings.AddRect("windowFrame", windowFrame);
	for (const std::string& path : recent)
		settings.AddString("recent", path.c_str());
	for (const auto& entry : glossary) {
		settings.AddString("glossaryName", entry.first.c_str());
		settings.AddString("glossaryText", entry.second.c_str());
	}
	for (const std::string& word : customWords)
		settings.AddString("word", word.c_str());

	BPath path;
	if (SettingsPath(path) != B_OK)
		return;
	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() == B_OK)
		settings.Flatten(&file);
}


void
Prefs::AddRecent(const std::string& path)
{
	recent.erase(std::remove(recent.begin(), recent.end(), path),
		recent.end());
	recent.insert(recent.begin(), path);
	if (recent.size() > 4)
		recent.resize(4);
}
