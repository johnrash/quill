/*
 * Quill - font cache and metrics.
 */
#include "Fonts.h"

#include "RTF.h"
#include "Text.h"

#include <Autolock.h>

#include <algorithm>
#include <strings.h>

using namespace quill;

HaikuMetrics gMetrics;


FontCache&
FontCache::Get()
{
	static FontCache sCache;
	return sCache;
}


FontCache::FontCache()
	:
	fLock("Quill fonts")
{
	Rescan();
}


void
FontCache::Rescan()
{
	BAutolock lock(fLock);
	fFamilies.clear();
	fFamilyList.clear();
	fResolved.clear();
	int32 count = count_font_families();
	for (int32 i = 0; i < count; i++) {
		font_family family;
		uint32 flags;
		if (get_font_family(i, &family, &flags) == B_OK) {
			fFamilies.insert(family);
			fFamilyList.push_back(family);
		}
	}
	std::sort(fFamilyList.begin(), fFamilyList.end(),
		[](const std::string& a, const std::string& b) {
			return strcasecmp(a.c_str(), b.c_str()) < 0;
		});
}


std::vector<std::string>
FontCache::Families()
{
	BAutolock lock(fLock);
	return fFamilyList;
}


bool
FontCache::IsInstalled(const std::string& family)
{
	BAutolock lock(fLock);
	return fFamilies.count(family) > 0;
}


std::string
FontCache::Resolve(const std::string& name)
{
	BAutolock lock(fLock);
	auto it = fResolved.find(name);
	if (it != fResolved.end())
		return it->second;
	std::string family;
	if (fFamilies.count(name))
		family = name;
	else {
		// case-insensitive match
		for (const std::string& installed : fFamilyList) {
			if (strcasecmp(installed.c_str(), name.c_str()) == 0) {
				family = installed;
				break;
			}
		}
	}
	if (family.empty()) {
		std::string substitute = SubstituteFontName(name);
		if (!substitute.empty() && fFamilies.count(substitute))
			family = substitute;
	}
	if (family.empty() && fFamilies.count(gDefaultSerif))
		family = gDefaultSerif;
	if (family.empty()) {
		font_family plain;
		font_style style;
		be_plain_font->GetFamilyAndStyle(&plain, &style);
		family = plain;
	}
	fResolved[name] = family;
	return family;
}


uint16
FontCache::_Face(const CharFormat& format) const
{
	uint16 face = 0;
	if (format.bold)
		face |= B_BOLD_FACE;
	if (format.italic)
		face |= B_ITALIC_FACE;
	return face == 0 ? B_REGULAR_FACE : face;
}


std::string
FontCache::_Key(const std::string& family, uint16 face)
{
	return family + "\x01" + std::to_string(face);
}


BFont
FontCache::_MakeFont(const std::string& family, uint16 face)
{
	std::string key = _Key(family, face);
	auto it = fBaseFonts.find(key);
	if (it != fBaseFonts.end())
		return it->second;
	BFont font(be_plain_font);
	if (font.SetFamilyAndFace(family.c_str(), face) != B_OK)
		font.SetFace(face);
	font.SetSpacing(B_CHAR_SPACING);
	font.SetSize(12);
	fBaseFonts[key] = font;
	return font;
}


BFont
FontCache::FontFor(const CharFormat& format, float pixelSize)
{
	std::string family = Resolve(format.font);
	BAutolock lock(fLock);
	BFont font = _MakeFont(family, _Face(format));
	font.SetSize(std::max(1.0f, pixelSize));
	return font;
}


void
FontCache::Widths(const CharFormat& format, float size, const char* text,
	int32 bytes, int32 count, float* widths)
{
	std::string family = Resolve(format.font);
	BAutolock lock(fLock);
	uint16 face = _Face(format);
	std::string key = _Key(family, face);
	std::unordered_map<uint32, float>& cache = fWidths[key];

	// Find the characters whose widths are not known yet.
	std::vector<uint32> codes(count);
	std::string missing;
	int32 missingCount = 0;
	std::vector<uint32> missingCodes;
	int32 offset = 0;
	for (int32 i = 0; i < count && offset < bytes; i++) {
		int32 consumed;
		codes[i] = DecodeUtf8(text + offset, bytes - offset, &consumed);
		consumed = std::max<int32>(1, consumed);
		if (cache.find(codes[i]) == cache.end()
			&& std::find(missingCodes.begin(), missingCodes.end(), codes[i])
				== missingCodes.end()) {
			missing.append(text + offset, consumed);
			missingCodes.push_back(codes[i]);
			missingCount++;
		}
		offset += consumed;
	}
	if (missingCount > 0) {
		BFont font = _MakeFont(family, face);
		font.SetSize(12);
		std::vector<float> escapements(missingCount);
		font.GetEscapements(missing.c_str(), missingCount,
			escapements.data());
		for (int32 i = 0; i < missingCount; i++)
			cache[missingCodes[i]] = escapements[i];
	}
	for (int32 i = 0; i < count; i++)
		widths[i] = cache[codes[i]] * size;
}


void
FontCache::Heights(const CharFormat& format, float size, float* ascent,
	float* descent, float* leading)
{
	std::string family = Resolve(format.font);
	BAutolock lock(fLock);
	uint16 face = _Face(format);
	std::string key = _Key(family, face);
	auto it = fHeights.find(key);
	if (it == fHeights.end()) {
		BFont font = _MakeFont(family, face);
		font.SetSize(100);
		font_height height;
		font.GetHeight(&height);
		it = fHeights.insert(std::make_pair(key, height)).first;
	}
	*ascent = it->second.ascent * size / 100;
	*descent = it->second.descent * size / 100;
	*leading = it->second.leading * size / 100;
}


void
HaikuMetrics::Widths(const CharFormat& format, float size, const char* text,
	int32_t bytes, int32_t count, float* widths)
{
	FontCache::Get().Widths(format, size, text, bytes, count, widths);
}


void
HaikuMetrics::Heights(const CharFormat& format, float size, float* ascent,
	float* descent, float* leading)
{
	FontCache::Get().Heights(format, size, ascent, descent, leading);
}
