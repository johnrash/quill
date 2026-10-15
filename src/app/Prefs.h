/*
 * Quill - preferences, shared by all windows.
 */
#ifndef QUILL_PREFS_H
#define QUILL_PREFS_H

#include <Locker.h>
#include <Rect.h>

#include <map>
#include <string>
#include <vector>

struct Prefs {
	int			unit;
	bool		smartQuotes;
	bool		showToolbar;
	bool		showRibbon;
	bool		showRuler;
	bool		showHidden;
	bool		textBoundaries;
	bool		tableGridlines;
	bool		openInPageView;
	bool		makeBackup;
	bool		ignoreUppercase;
	bool		ignoreNumbers;
	std::string	userName;
	std::string	defaultFont;
	float		defaultSize;
	float		zoom;
	BRect		windowFrame;
	std::vector<std::string> recent;
	std::map<std::string, std::string> glossary;	// name -> RTF fragment
	std::vector<std::string> customWords;

				Prefs();
	void		Load();
	void		Save();
	void		AddRecent(const std::string& path);
};

extern Prefs gPrefs;
extern BLocker gPrefsLock;

#endif	// QUILL_PREFS_H
