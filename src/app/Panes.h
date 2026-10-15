/*
 * Quill - the header/footer/footnote pane at the bottom of the document
 * window, Print Preview, and printing.
 */
#ifndef QUILL_PANES_H
#define QUILL_PANES_H

#include "DocView.h"

#include <GroupView.h>

class BScrollBar;
class BStringView;
class IconButton;

enum PaneKind {
	kPaneNone = 0,
	kPaneHeader,
	kPaneFooter,
	kPaneFirstHeader,
	kPaneFirstFooter,
	kPaneFootnote
};

class StoryPane : public BGroupView {
public:
							StoryPane(quill::Document* document,
								PictureCache* pictures);

			void			SetStory(quill::Story* story, const char* title,
								int kind, DocViewListener* listener);
			DocView*		View() const { return fView; }
			int				Kind() const { return fKind; }
	virtual	void			Draw(BRect updateRect);

private:
			quill::Document* fDocument;
			PictureCache*	fPictures;
			BStringView*	fTitle;
			IconButton*		fPageNumber;
			IconButton*		fDate;
			IconButton*		fTime;
			BGroupView*		fViewGroup;
			DocView*		fView;
			BScrollBar*		fScroll;
			int				fKind;
};

class PreviewView : public BView {
public:
							PreviewView(quill::Document* document,
								PageRenderer* renderer);

	virtual	void			Draw(BRect updateRect);
	virtual	void			FrameResized(float width, float height);
	virtual	void			KeyDown(const char* bytes, int32 numBytes);
	virtual	void			MouseDown(BPoint where);

			void			SetLayout(quill::DocLayout* layout);
			void			SetFirstPage(int32 page);
			int32			FirstPage() const { return fFirstPage; }
			void			SetTwoPages(bool two);
			bool			TwoPages() const { return fTwoPages; }
			void			SetMargins(bool show);
			bool			Margins() const { return fMargins; }
			void			SetMagnified(bool magnified);

private:
			float			_Zoom();

			quill::Document* fDocument;
			PageRenderer*	fRenderer;
			quill::DocLayout* fLayout;
			int32			fFirstPage;
			bool			fTwoPages;
			bool			fMargins;
			bool			fMagnified;
};

status_t PrintDocument(quill::Document* document, quill::DocLayout& body,
	PictureCache* pictures, BMessage** settings, const char* name);

#endif	// QUILL_PANES_H
