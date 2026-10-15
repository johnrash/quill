/*
 * Quill - button bitmaps. The pictures are drawn here from pixel art and
 * simple shapes, in the spirit of early 1990s Macintosh toolbars.
 */
#include "Icons.h"

#include <Font.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

struct Pixel {
	uint8	r, g, b, a;
};

class Canvas {
public:
	Canvas() : fPixels(16 * 16, Pixel{ 0, 0, 0, 0 }) {}

	void Set(int x, int y, Pixel color)
	{
		if (x >= 0 && x < 16 && y >= 0 && y < 16)
			fPixels[y * 16 + x] = color;
	}

	void HLine(int x0, int x1, int y, Pixel color)
	{
		for (int x = x0; x <= x1; x++)
			Set(x, y, color);
	}

	void VLine(int x, int y0, int y1, Pixel color)
	{
		for (int y = y0; y <= y1; y++)
			Set(x, y, color);
	}

	void Rect(int x0, int y0, int x1, int y1, Pixel color)
	{
		HLine(x0, x1, y0, color);
		HLine(x0, x1, y1, color);
		VLine(x0, y0, y1, color);
		VLine(x1, y0, y1, color);
	}

	void Fill(int x0, int y0, int x1, int y1, Pixel color)
	{
		for (int y = y0; y <= y1; y++)
			HLine(x0, x1, y, color);
	}

	void Art(const char* const rows[16])
	{
		for (int y = 0; y < 16; y++) {
			const char* row = rows[y];
			if (row == nullptr)
				break;
			int length = (int)strlen(row);
			for (int x = 0; x < 16 && x < length; x++) {
				Pixel color;
				if (Color(row[x], &color))
					Set(x, y, color);
			}
		}
	}

	static bool Color(char c, Pixel* color)
	{
		switch (c) {
			case 'k': *color = Pixel{ 0, 0, 0, 255 }; return true;
			case 'd': *color = Pixel{ 96, 96, 96, 255 }; return true;
			case 'g': *color = Pixel{ 160, 160, 160, 255 }; return true;
			case 'l': *color = Pixel{ 210, 210, 210, 255 }; return true;
			case 'w': *color = Pixel{ 255, 255, 255, 255 }; return true;
			case 'b': *color = Pixel{ 0, 64, 192, 255 }; return true;
			case 'B': *color = Pixel{ 150, 190, 255, 255 }; return true;
			case 'r': *color = Pixel{ 200, 0, 0, 255 }; return true;
			case 'y': *color = Pixel{ 255, 215, 60, 255 }; return true;
			case 'o': *color = Pixel{ 190, 120, 50, 255 }; return true;
			case 'G': *color = Pixel{ 0, 150, 0, 255 }; return true;
			case 'c': *color = Pixel{ 0, 170, 200, 255 }; return true;
			default: return false;
		}
	}

	BBitmap* Make(int scale) const
	{
		int size = 16 * scale;
		BBitmap* bitmap = new BBitmap(BRect(0, 0, size - 1, size - 1),
			B_RGBA32);
		uint8* bits = (uint8*)bitmap->Bits();
		int32 stride = bitmap->BytesPerRow();
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				const Pixel& p = fPixels[(y / scale) * 16 + x / scale];
				uint8* out = bits + y * stride + x * 4;
				out[0] = p.b;
				out[1] = p.g;
				out[2] = p.r;
				out[3] = p.a;
			}
		}
		return bitmap;
	}

private:
	std::vector<Pixel> fPixels;
};

const Pixel kBlack = { 0, 0, 0, 255 };
const Pixel kWhite = { 255, 255, 255, 255 };
const Pixel kGray = { 128, 128, 128, 255 };


const char* const kNewArt[16] = {
	"..kkkkkkkk......",
	"..kwwwwwwkk.....",
	"..kwwwwwwkwk....",
	"..kwwwwwwkwwk...",
	"..kwwwwwwkkkkk..",
	"..kwwwwwwwwwwk..",
	"..kwwwwwwwwwwk..",
	"..kwwwwwwwwwwk..",
	"..kwwwwwwwwwwk..",
	"..kwwwwwwwwwwk..",
	"..kwwwwwwwwwwk..",
	"..kwwwwwwwwwwk..",
	"..kwwwwwwwwwwk..",
	"..kwwwwwwwwwwk..",
	"..kkkkkkkkkkkk..",
	"................"
};

const char* const kOpenArt[16] = {
	"................",
	"..........kkk...",
	".........k...k.k",
	"..............kk",
	".kkk.........kkk",
	"kyyykkkkkkk.....",
	"kyyyyyyyyyk.....",
	"kyyykkkkkkkkkkk.",
	"kyykyyyyyyyyyyk.",
	"kyykyyyyyyyyyk..",
	"kykyyyyyyyyyyk..",
	"kykyyyyyyyyyk...",
	"kkyyyyyyyyyyk...",
	"kkkkkkkkkkkk....",
	"................",
	"................"
};

const char* const kSaveArt[16] = {
	"................",
	".kkkkkkkkkkkkkk.",
	".kdkwwwwwwwwkdk.",
	".kdkwwwwwwwwkdk.",
	".kdkwwwwwwwwkdk.",
	".kdkwwwwwwwwkdk.",
	".kddkkkkkkkkddk.",
	".kddddddddddddk.",
	".kddkkkkkkkkddk.",
	".kddklldkkkkddk.",
	".kddklldkkkkddk.",
	".kddklldkkkkddk.",
	".kkkkkkkkkkkkkk.",
	"................",
	"................",
	"................"
};

const char* const kPrintArt[16] = {
	"................",
	"....kkkkkkkk....",
	"....kwwwwwwk....",
	"....kwkkkkwk....",
	"....kwwwwwwk....",
	"..kkkkkkkkkkkk..",
	".kllllllllllllk.",
	".kllllllllllGlk.",
	".kllllllllllllk.",
	".kddddddddddddk.",
	".kkkkkkkkkkkkkk.",
	"...kwwwwwwwwk...",
	"...kwkkkkkkwk...",
	"...kwwwwwwwwk...",
	"...kkkkkkkkkk...",
	"................"
};

const char* const kCutArt[16] = {
	"................",
	"...k.......k....",
	"...k.......k....",
	"...kk.....kk....",
	"....k.....k.....",
	"....kk...kk.....",
	".....k...k......",
	".....kk.kk......",
	"......kkk.......",
	"......kkk.......",
	".....kk.kk......",
	"..kkkk...kkkk...",
	".k..k.....k..k..",
	".k..k.....k..k..",
	"..kk.......kk...",
	"................"
};

const char* const kCopyArt[16] = {
	"kkkkkkk.........",
	"kwwwwwkk........",
	"kwkkkwkwk.......",
	"kwwwwwkkkk......",
	"kwkkkkkkkkkk....",
	"kwwwwkwwwwwkk...",
	"kwkkkkwkkkwkwk..",
	"kwwwwkwwwwwkkkk.",
	"kwkkkkwkkkkkkwk.",
	"kwwwwkwwwwwwwwk.",
	"kkkkkkwkkkkkkwk.",
	".....kwwwwwwwwk.",
	".....kwkkkkkkwk.",
	".....kwwwwwwwwk.",
	".....kkkkkkkkkk.",
	"................"
};

const char* const kPasteArt[16] = {
	"......kkkk......",
	"..kkkkwggwkkkk..",
	".kooookkkkooook.",
	".koowwwwwwwwook.",
	".koowkkkkkkwook.",
	".koowwwwwwwwook.",
	".koowkkkkkkwook.",
	".koowwwwwwwwook.",
	".koowkkkkkkwook.",
	".koowwwwwwwwook.",
	".koowkkkwwwwook.",
	".koowwwwwwwwook.",
	".kooooooooooook.",
	".kkkkkkkkkkkkkk.",
	"................",
	"................"
};

const char* const kUndoArt[16] = {
	"................",
	"................",
	"....k...........",
	"...kk...........",
	"..kkkkkkkkk.....",
	".kkkkkkkkkkk....",
	"..kkk......kk...",
	"...kk.......kk..",
	"....k........k..",
	".............k..",
	".............k..",
	"............kk..",
	"..........kkk...",
	"......kkkkk.....",
	"................",
	"................"
};

const char* const kBulletsArt[16] = {
	"................",
	"................",
	".kk..kkkkkkkkk..",
	".kk.............",
	"................",
	".kk..kkkkkkkkk..",
	".kk.............",
	"................",
	".kk..kkkkkkkkk..",
	".kk.............",
	"................",
	".kk..kkkkkkkkk..",
	".kk.............",
	"................",
	"................",
	"................"
};

const char* const kNumbersArt[16] = {
	"................",
	"..k.............",
	".kk...kkkkkkkk..",
	"..k.............",
	".kkk............",
	"................",
	".kk.............",
	"...k..kkkkkkkk..",
	"..k.............",
	".kkk............",
	"................",
	".kkk............",
	"..kk..kkkkkkkk..",
	"...k............",
	".kk.............",
	"................"
};

const char* const kUnindentArt[16] = {
	"................",
	"kkkkkkkkkkkkkkk.",
	"................",
	"......kkkkkkkkk.",
	"..k.............",
	".kk...kkkkkkkkk.",
	"kkkkk...........",
	".kk...kkkkkkkkk.",
	"..k.............",
	"......kkkkkkkkk.",
	"................",
	"kkkkkkkkkkkkkkk.",
	"................",
	"................",
	"................",
	"................"
};

const char* const kIndentArt[16] = {
	"................",
	"kkkkkkkkkkkkkkk.",
	"................",
	"......kkkkkkkkk.",
	"..k.............",
	"..kk..kkkkkkkkk.",
	"kkkkk...........",
	"..kk..kkkkkkkkk.",
	"..k.............",
	"......kkkkkkkkk.",
	"................",
	"kkkkkkkkkkkkkkk.",
	"................",
	"................",
	"................",
	"................"
};

const char* const kTableArt[16] = {
	"................",
	"................",
	".kkkkkkkkkkkkkk.",
	".kwwwkwwwwkwwwk.",
	".kwwwkwwwwkwwwk.",
	".kkkkkkkkkkkkkk.",
	".kwwwkwwwwkwwwk.",
	".kwwwkwwwwkwwwk.",
	".kkkkkkkkkkkkkk.",
	".kwwwkwwwwkwwwk.",
	".kwwwkwwwwkwwwk.",
	".kkkkkkkkkkkkkk.",
	"................",
	"................",
	"................",
	"................"
};

const char* const kColumnsArt[16] = {
	"................",
	".kkkkkkkkkkkkkk.",
	".kwwwwwwwwwwwwk.",
	".kwkkkkwwkkkkwk.",
	".kwwwwwwwwwwwwk.",
	".kwkkkkwwkkkkwk.",
	".kwwwwwwwwwwwwk.",
	".kwkkkkwwkkkkwk.",
	".kwwwwwwwwwwwwk.",
	".kwkkkkwwkkkkwk.",
	".kwwwwwwwwwwwwk.",
	".kwkkkwwwkkkwwk.",
	".kwwwwwwwwwwwwk.",
	".kkkkkkkkkkkkkk.",
	"................",
	"................"
};

const char* const kPictureArt[16] = {
	"................",
	"................",
	".kkkkkkkkkkkkkk.",
	".kBBBBBBBBByyBk.",
	".kBBBBBBBByyyyk.",
	".kBBBBBBBBByyBk.",
	".kBBBBkBBBBBBBk.",
	".kBBBkGkBBBBBBk.",
	".kBBkGGGkBBkBBk.",
	".kBkGGGGGkkGkBk.",
	".kkGGGGGGGGGGkk.",
	".kGGGGGGGGGGGGk.",
	".kkkkkkkkkkkkkk.",
	"................",
	"................",
	"................"
};

const char* const kSpellingArt[16] = {
	"................",
	".kk..kkk...kkk..",
	"k..k.k..k.k.....",
	"kkkk.kkk..k.....",
	"k..k.k..k.k.....",
	"k..k.kkk...kkk..",
	"................",
	"...........G....",
	"..........GG....",
	"..G......GG.....",
	"..GG....GG......",
	"...GG..GG.......",
	"....GGGG........",
	".....GG.........",
	"................",
	"................"
};

const char* const kShowAllArt[16] = {
	"................",
	"....kkkkkkkk....",
	"...kkkkkk.k.....",
	"..kkkkkkk.k.....",
	"..kkkkkkk.k.....",
	"..kkkkkkk.k.....",
	"...kkkkkk.k.....",
	"....kkkkk.k.....",
	"........k.k.....",
	"........k.k.....",
	"........k.k.....",
	"........k.k.....",
	"........k.k.....",
	"........k.k.....",
	"................",
	"................"
};

const char* const kPageViewArt[16] = {
	".kkkkkkkkkkkk...",
	".kwwwwwwwwwwk...",
	".kwkkkkkkkkwk...",
	".kwwwwwwwwwwk...",
	".kwkkkkkkkkwk...",
	".kwkkkkkkkkwk...",
	".kwkkkkkkkkwk...",
	".kwkkkkkkwwwk...",
	".kwwwwwwwwwwk...",
	".kwkkkkkkkkwk...",
	".kwkkkkkkkkwk...",
	".kwkkkkkwwwwk...",
	".kwwwwwwwwwwkd..",
	".kkkkkkkkkkkkd..",
	"..ddddddddddddd.",
	"................"
};

const char* const kPreviewArt[16] = {
	".kkkkkkkkkk.....",
	".kwwwwwwwwk.....",
	".kwkkkkkkwk.....",
	".kwwwwwwwwk.....",
	".kwkkkkkkwk.....",
	".kwwwwkkkkk.....",
	".kwkkkwwwwkk....",
	".kwwkwwwwwwk....",
	".kwwkwwwwwwk....",
	".kwwkwwwwwwk....",
	".kkkkwwwwwwk....",
	".....kwwwwk.....",
	"......kkkkkk....",
	"..........kkk...",
	"...........kkk..",
	"............kk.."
};

// The lens has an odd width (interior columns 2 to 10) so that the plus
// sits exactly in its middle, at column 6 and row 6.
const char* const kZoomArt[16] = {
	"................",
	"....kkkkk.......",
	"...kwwwwwk......",
	"..kwwwwwwwk.....",
	".kwwwwkwwwwk....",
	".kwwwwkwwwwk....",
	".kwwkkkkkwwk....",
	".kwwwwkwwwwk....",
	".kwwwwkwwwwk....",
	"..kwwwwwwwk.....",
	"...kwwwwwkkk....",
	"....kkkkkkkkk...",
	"...........kkk..",
	"............kkk.",
	".............kk.",
	"................"
};

const char* const kFootnoteArt[16] = {
	"................",
	"............k...",
	"...........kk...",
	"............k...",
	"............k...",
	"...........kkk..",
	".kkkkkkkkk......",
	"................",
	".kkkkkkkkkkkkk..",
	"................",
	"....kkkk........",
	"...........k....",
	"..k.kkkkkkk.....",
	".kk.............",
	"..k.kkkkkkkk....",
	"................"
};

const char* const kSuperscriptArt[16] = {
	"................",
	"...........kk...",
	"..........k..k..",
	".............k..",
	"............k...",
	"...........kkkk.",
	"................",
	".kk....kk.......",
	"..kk..kk........",
	"...kkkk.........",
	"....kk..........",
	"...kkkk.........",
	"..kk..kk........",
	".kk....kk.......",
	"................",
	"................"
};

const char* const kSubscriptArt[16] = {
	"................",
	"................",
	".kk....kk.......",
	"..kk..kk........",
	"...kkkk.........",
	"....kk..........",
	"...kkkk.........",
	"..kk..kk........",
	".kk....kk.......",
	"................",
	"...........kk...",
	"..........k..k..",
	".............k..",
	"............k...",
	"...........kkkk.",
	"................"
};

const char* const kPageNumberArt[16] = {
	"................",
	"..kkkkkkkkkkk...",
	"..kwwwwwwwwwk...",
	"..kwwkwwkwwwk...",
	"..kwkkkkkkwwk...",
	"..kwwkwwkwwwk...",
	"..kwkkkkkkwwk...",
	"..kwwkwwkwwwk...",
	"..kwwwwwwwwwk...",
	"..kwwwwwwwwwk...",
	"..kwwwwwwwwwk...",
	"..kwwwwwwwwwk...",
	"..kkkkkkkkkkk...",
	"................",
	"................",
	"................"
};

const char* const kDateArt[16] = {
	"................",
	"..k...k...k.....",
	".kkkkkkkkkkkkk..",
	".krrrrrrrrrrrk..",
	".kkkkkkkkkkkkk..",
	".kwkwkwkwkwkwk..",
	".kwwwwwwwwwwwk..",
	".kwkwkwkwkwkwk..",
	".kwwwwwwwwwwwk..",
	".kwkwkwkwkwkwk..",
	".kwwwwwwwwwwwk..",
	".kkkkkkkkkkkkk..",
	"................",
	"................",
	"................",
	"................"
};

const char* const kTimeArt[16] = {
	"................",
	".....kkkkk......",
	"...kkwwwwwkk....",
	"..kwwwwkwwwwk...",
	"..kwwwwkwwwwk...",
	".kwwwwwkwwwwwk..",
	".kwwwwwkwwwwwk..",
	".kwwwwwkkkkwwk..",
	".kwwwwwwwwwwwk..",
	".kwwwwwwwwwwwk..",
	"..kwwwwwwwwwk...",
	"..kwwwwwwwwwk...",
	"...kkwwwwwkk....",
	".....kkkkk......",
	"................",
	"................"
};

const char* const kMarginsArt[16] = {
	".kkkkkkkkkkkk...",
	".kwwwwwwwwwwk...",
	".kwkwwwwwwkwk...",
	".kkkkkkkkkkkk...",
	".kwkwwwwwwkwk...",
	".kwkwkkkkwkwk...",
	".kwkwwwwwwkwk...",
	".kwkwkkkkwkwk...",
	".kwkwwwwwwkwk...",
	".kwkwkkkwwkwk...",
	".kwkwwwwwwkwk...",
	".kkkkkkkkkkkk...",
	".kwkwwwwwwkwk...",
	".kwwwwwwwwwwk...",
	".kkkkkkkkkkkk...",
	"................"
};

const char* const kOnePageArt[16] = {
	"................",
	"....kkkkkkk.....",
	"....kwwwwwkk....",
	"....kwkkkwkwk...",
	"....kwwwwwkkkk..",
	"....kwkkkkkkwk..",
	"....kwwwwwwwwk..",
	"....kwkkkkkkwk..",
	"....kwwwwwwwwk..",
	"....kwkkkkkkwk..",
	"....kwwwwwwwwk..",
	"....kwkkkkwwwk..",
	"....kwwwwwwwwk..",
	"....kkkkkkkkkk..",
	"................",
	"................"
};

const char* const kTwoPagesArt[16] = {
	"................",
	"................",
	"kkkkkkk.kkkkkkk.",
	"kwwwwwk.kwwwwwk.",
	"kwkkkwk.kwkkkwk.",
	"kwwwwwk.kwwwwwk.",
	"kwkkkwk.kwkkkwk.",
	"kwwwwwk.kwwwwwk.",
	"kwkkkwk.kwkkkwk.",
	"kwwwwwk.kwwwwwk.",
	"kwkkwwk.kwkwwwk.",
	"kwwwwwk.kwwwwwk.",
	"kkkkkkk.kkkkkkk.",
	"................",
	"................",
	"................"
};

const char* const kCloseArt[16] = {
	"................",
	"................",
	"..kk.......kk...",
	"...kk.....kk....",
	"....kk...kk.....",
	".....kk.kk......",
	"......kkk.......",
	"......kkk.......",
	".....kk.kk......",
	"....kk...kk.....",
	"...kk.....kk....",
	"..kk.......kk...",
	"................",
	"................",
	"................",
	"................"
};


const char* const kPromoteArt[16] = {
	"................",
	"................",
	"................",
	"......k.........",
	".....kk.........",
	"....kkk.........",
	"...kkkkkkkkkk...",
	"..kkkkkkkkkkk...",
	"...kkkkkkkkkk...",
	"....kkk.........",
	".....kk.........",
	"......k.........",
	"................",
	"................",
	"................",
	"................"
};

const char* const kDemoteArt[16] = {
	"................",
	"................",
	"................",
	".........k......",
	".........kk.....",
	".........kkk....",
	"...kkkkkkkkkk...",
	"...kkkkkkkkkkk..",
	"...kkkkkkkkkk...",
	".........kkk....",
	".........kk.....",
	".........k......",
	"................",
	"................",
	"................",
	"................"
};

const char* const kToBodyArt[16] = {
	"................",
	"................",
	"........k.......",
	"........kk......",
	"kkkkkkkkkkk.....",
	"........kkkk....",
	"kkkkkkkkkkkkk...",
	"........kkkk....",
	"kkkkkkkkkkk.....",
	"........kk......",
	"........k.......",
	"................",
	"................",
	"................",
	"................",
	"................"
};

const char* const kMoveUpArt[16] = {
	"................",
	"................",
	".......k........",
	"......kkk.......",
	".....kkkkk......",
	"....kkkkkkk.....",
	"...kkkkkkkkk....",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"................",
	"................",
	"................"
};

const char* const kMoveDownArt[16] = {
	"................",
	"................",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"...kkkkkkkkk....",
	"....kkkkkkk.....",
	".....kkkkk......",
	"......kkk.......",
	".......k........",
	"................",
	"................",
	"................"
};

const char* const kExpandArt[16] = {
	"................",
	"................",
	"................",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"...kkkkkkkkk....",
	"...kkkkkkkkk....",
	"...kkkkkkkkk....",
	"......kkk.......",
	"......kkk.......",
	"......kkk.......",
	"................",
	"................",
	"................",
	"................"
};

const char* const kCollapseArt[16] = {
	"................",
	"................",
	"................",
	"................",
	"................",
	"................",
	"...kkkkkkkkk....",
	"...kkkkkkkkk....",
	"...kkkkkkkkk....",
	"................",
	"................",
	"................",
	"................",
	"................",
	"................",
	"................"
};

const char* const kFirstLineArt[16] = {
	"................",
	"................",
	"..kkkkkkkkkkkk..",
	"................",
	"..gggggggggggg..",
	"................",
	"..gggggggggggg..",
	"................",
	"..gggggggg......",
	"................",
	"..kkkkkkkkkkkk..",
	"................",
	"..gggggggggg....",
	"................",
	"................",
	"................"
};

const char* const kFormattingArt[16] = {
	"................",
	"................",
	"....kk..........",
	"....kk..........",
	"...kkkk.........",
	"...k..k.........",
	"..kk..kk........",
	"..kkkkkk...kk...",
	"..k....k..k..k..",
	".kk....kk...kk..",
	".k......k..k.k..",
	"kkk....kkk.kkkk.",
	"................",
	"................",
	"................",
	"................"
};


void
AlignmentLines(Canvas& canvas, int align)
{
	static const int kLengths[4] = { 12, 8, 12, 6 };
	for (int i = 0; i < 4; i++) {
		int y = 3 + i * 3;
		int length = kLengths[i];
		if (align == 3)
			length = i == 3 ? 7 : 12;
		int x0;
		switch (align) {
			case 1: x0 = 2 + (12 - length) / 2; break;
			case 2: x0 = 14 - length; break;
			default: x0 = 2; break;
		}
		canvas.HLine(x0, x0 + length - 1, y, kBlack);
	}
}


void
SpacingLines(Canvas& canvas, int gap)
{
	for (int y = 2; y <= 14; y += gap)
		canvas.HLine(2, 13, y, kBlack);
}


void
TabShape(Canvas& canvas, int kind)
{
	switch (kind) {
		case 0:	// left: L
			canvas.VLine(5, 5, 12, kBlack);
			canvas.VLine(6, 5, 12, kBlack);
			canvas.HLine(5, 12, 11, kBlack);
			canvas.HLine(5, 12, 12, kBlack);
			break;
		case 1:	// center
			canvas.VLine(7, 4, 12, kBlack);
			canvas.VLine(8, 4, 12, kBlack);
			canvas.HLine(3, 12, 11, kBlack);
			canvas.HLine(3, 12, 12, kBlack);
			break;
		case 2:	// right
			canvas.VLine(9, 5, 12, kBlack);
			canvas.VLine(10, 5, 12, kBlack);
			canvas.HLine(3, 10, 11, kBlack);
			canvas.HLine(3, 10, 12, kBlack);
			break;
		case 3:	// decimal
			canvas.VLine(7, 4, 12, kBlack);
			canvas.VLine(8, 4, 12, kBlack);
			canvas.HLine(3, 12, 11, kBlack);
			canvas.HLine(3, 12, 12, kBlack);
			canvas.Fill(11, 6, 12, 7, kBlack);
			break;
		default:	// vertical bar
			canvas.VLine(7, 2, 13, kBlack);
			canvas.VLine(8, 2, 13, kBlack);
			break;
	}
}


void
ColumnPage(Canvas& canvas, int columns)
{
	canvas.Fill(2, 1, 13, 14, kWhite);
	canvas.Rect(2, 1, 13, 14, kBlack);
	int inner = 8;
	int width = (inner - (columns - 1)) / columns;
	if (columns == 1)
		width = 8;
	for (int c = 0; c < columns; c++) {
		int x0 = 4 + c * (width + 1);
		int x1 = x0 + width - 1;
		for (int y = 3; y <= 12; y += 2)
			canvas.HLine(x0, x1, y, kGray);
	}
}


BBitmap* sIcons[kIconCount];
bool sInitialized = false;

}	// namespace


int
IconScale()
{
	float size = be_plain_font->Size();
	return std::max(1, (int)floorf(size / 12.0f + 0.25f));
}


BBitmap*
GetIcon(int id)
{
	if (id < 0 || id >= kIconCount)
		return nullptr;
	if (!sInitialized) {
		memset(sIcons, 0, sizeof(sIcons));
		sInitialized = true;
	}
	if (sIcons[id] != nullptr)
		return sIcons[id];

	Canvas canvas;
	switch (id) {
		case kIconNew: canvas.Art(kNewArt); break;
		case kIconOpen: canvas.Art(kOpenArt); break;
		case kIconSave: canvas.Art(kSaveArt); break;
		case kIconPrint: canvas.Art(kPrintArt); break;
		case kIconCut: canvas.Art(kCutArt); break;
		case kIconCopy: canvas.Art(kCopyArt); break;
		case kIconPaste: canvas.Art(kPasteArt); break;
		case kIconUndo: canvas.Art(kUndoArt); break;
		case kIconBullets: canvas.Art(kBulletsArt); break;
		case kIconNumbers: canvas.Art(kNumbersArt); break;
		case kIconUnindent: canvas.Art(kUnindentArt); break;
		case kIconIndent: canvas.Art(kIndentArt); break;
		case kIconTable: canvas.Art(kTableArt); break;
		case kIconColumns: canvas.Art(kColumnsArt); break;
		case kIconPicture: canvas.Art(kPictureArt); break;
		case kIconSpelling: canvas.Art(kSpellingArt); break;
		case kIconShowAll: canvas.Art(kShowAllArt); break;
		case kIconNormalView: AlignmentLines(canvas, 0); break;
		case kIconPageView: canvas.Art(kPageViewArt); break;
		case kIconPreview: canvas.Art(kPreviewArt); break;
		case kIconZoom: canvas.Art(kZoomArt); break;
		case kIconFootnote: canvas.Art(kFootnoteArt); break;
		case kIconAlignLeft: AlignmentLines(canvas, 0); break;
		case kIconAlignCenter: AlignmentLines(canvas, 1); break;
		case kIconAlignRight: AlignmentLines(canvas, 2); break;
		case kIconAlignJustify: AlignmentLines(canvas, 3); break;
		case kIconSpaceSingle: SpacingLines(canvas, 2); break;
		case kIconSpaceOneHalf: SpacingLines(canvas, 3); break;
		case kIconSpaceDouble: SpacingLines(canvas, 4); break;
		case kIconParaClosed:
			for (int y = 2; y <= 12; y += 2)
				canvas.HLine(2, y == 6 || y == 12 ? 9 : 13, y, kBlack);
			break;
		case kIconParaOpen:
			for (int y = 1; y <= 5; y += 2)
				canvas.HLine(2, y == 5 ? 9 : 13, y, kBlack);
			for (int y = 10; y <= 14; y += 2)
				canvas.HLine(2, y == 14 ? 9 : 13, y, kBlack);
			break;
		case kIconTabLeft: TabShape(canvas, 0); break;
		case kIconTabCenter: TabShape(canvas, 1); break;
		case kIconTabRight: TabShape(canvas, 2); break;
		case kIconTabDecimal: TabShape(canvas, 3); break;
		case kIconTabBar: TabShape(canvas, 4); break;
		case kIconScale:
			canvas.VLine(2, 3, 12, kBlack);
			canvas.HLine(2, 4, 3, kBlack);
			canvas.HLine(2, 4, 12, kBlack);
			canvas.VLine(13, 3, 12, kBlack);
			canvas.HLine(11, 13, 3, kBlack);
			canvas.HLine(11, 13, 12, kBlack);
			for (int x = 5; x <= 10; x += 2)
				canvas.VLine(x, 9, 12, kBlack);
			break;
		case kIconColumns1: ColumnPage(canvas, 1); break;
		case kIconColumns2: ColumnPage(canvas, 2); break;
		case kIconColumns3: ColumnPage(canvas, 3); break;
		case kIconSuperscript: canvas.Art(kSuperscriptArt); break;
		case kIconSubscript: canvas.Art(kSubscriptArt); break;
		case kIconPageNumber: canvas.Art(kPageNumberArt); break;
		case kIconDate: canvas.Art(kDateArt); break;
		case kIconTime: canvas.Art(kTimeArt); break;
		case kIconMargins: canvas.Art(kMarginsArt); break;
		case kIconOnePage: canvas.Art(kOnePageArt); break;
		case kIconTwoPages: canvas.Art(kTwoPagesArt); break;
		case kIconClose: canvas.Art(kCloseArt); break;
		case kIconPromote: canvas.Art(kPromoteArt); break;
		case kIconDemote: canvas.Art(kDemoteArt); break;
		case kIconToBody: canvas.Art(kToBodyArt); break;
		case kIconMoveUp: canvas.Art(kMoveUpArt); break;
		case kIconMoveDown: canvas.Art(kMoveDownArt); break;
		case kIconExpand: canvas.Art(kExpandArt); break;
		case kIconCollapse: canvas.Art(kCollapseArt); break;
		case kIconFirstLine: canvas.Art(kFirstLineArt); break;
		case kIconFormatting: canvas.Art(kFormattingArt); break;
	}
	sIcons[id] = canvas.Make(IconScale());
	return sIcons[id];
}
