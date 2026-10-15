/*
 * Quill - fonts: resolving document font names to installed families,
 * caching BFonts and character widths, and the FontMetrics used by the
 * layout. Shared by all windows, so everything is locked.
 */
#ifndef QUILL_FONTS_H
#define QUILL_FONTS_H

#include "Layout.h"

#include <Font.h>
#include <Locker.h>

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class FontCache {
public:
	static	FontCache&		Get();

			BFont			FontFor(const quill::CharFormat& format,
								float pixelSize);
			std::string		Resolve(const std::string& name);
			bool			IsInstalled(const std::string& family);
			std::vector<std::string> Families();
			void			Rescan();

			void			Widths(const quill::CharFormat& format,
								float size, const char* text, int32 bytes,
								int32 count, float* widths);
			void			Heights(const quill::CharFormat& format,
								float size, float* ascent, float* descent,
								float* leading);

private:
							FontCache();
			std::string		_Key(const std::string& family, uint16 face);
			BFont			_MakeFont(const std::string& family, uint16 face);
			uint16			_Face(const quill::CharFormat& format) const;

			BLocker			fLock;
			std::set<std::string> fFamilies;
			std::vector<std::string> fFamilyList;
			std::map<std::string, std::string> fResolved;
			std::map<std::string, BFont> fBaseFonts;
			std::map<std::string,
				std::unordered_map<uint32, float> > fWidths;
			std::map<std::string, font_height> fHeights;
};


class HaikuMetrics : public quill::FontMetrics {
public:
	virtual	void			Widths(const quill::CharFormat& format,
								float size, const char* text, int32_t bytes,
								int32_t count, float* widths);
	virtual	void			Heights(const quill::CharFormat& format,
								float size, float* ascent, float* descent,
								float* leading);
};

extern HaikuMetrics gMetrics;

#endif	// QUILL_FONTS_H
