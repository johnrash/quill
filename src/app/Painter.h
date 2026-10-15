/*
 * Quill - drawing laid out text. Used for the screen, Print Preview and
 * printing, so it only depends on a BView and a zoom factor.
 */
#ifndef QUILL_PAINTER_H
#define QUILL_PAINTER_H

#include "Layout.h"

#include <Bitmap.h>
#include <View.h>

#include <map>

class PictureCache {
public:
							~PictureCache();
			BBitmap*		Get(const quill::Picture* picture);
			void			Clear();

	static	BBitmap*		Decode(const quill::Picture* picture);

private:
			std::map<const quill::Picture*, BBitmap*> fBitmaps;
};

struct PaintContext {
	BView*					view = nullptr;
	const quill::Document*	document = nullptr;
	float					zoom = 1.0f;
	bool					showAll = false;
	bool					printing = false;
	PictureCache*			pictures = nullptr;
};

rgb_color ColorFor(uint32 color);

// Draws one line of a paragraph. origin is the view position of the
// paragraph frame's x origin and of the line's top.
void PaintLine(PaintContext& context, const quill::Paragraph& paragraph,
	const quill::ParaLayout& layout, int32 line, BPoint origin);

// Draws the borders and gridlines of a table row. left and top are the
// view position of the frame origin (the text column's left edge) and of
// the row's top; height is in view pixels.
void PaintRowGrid(PaintContext& context, const quill::RowFormat& row,
	float left, float top, float height, float layoutWidth, bool gridlines);

void StrokeDotted(BView* view, BPoint from, BPoint to, float step);

#endif	// QUILL_PAINTER_H
