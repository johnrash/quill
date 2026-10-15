/*
 * Quill - draws a whole page (paper, headers, footers, body text, table
 * grids, footnotes) for Page Layout view, Print Preview and printing.
 */
#ifndef QUILL_PAGE_RENDERER_H
#define QUILL_PAGE_RENDERER_H

#include "Layout.h"
#include "Painter.h"

class PageRenderer {
public:
							PageRenderer(quill::Document* document,
								PictureCache* pictures);
							~PageRenderer();

	struct Options {
		float	zoom = 1;
		bool	showAll = false;
		bool	printing = false;
		bool	boundaries = false;
		bool	paper = true;		// fill the paper and draw its edge
		bool	gridlines = true;
	};

			void			RenderPage(BView* view, quill::DocLayout& body,
								int32 page, BPoint origin, BRect update,
								const Options& options);
			void			Invalidate();

private:
			void			_UpdateHeaders(quill::DocLayout& body,
								int32 page, bool showHidden);

			quill::Document* fDocument;
			PictureCache*	fPictures;
			quill::DocLayout* fHeaders[4];
};

#endif	// QUILL_PAGE_RENDERER_H
