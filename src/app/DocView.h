/*
 * Quill - the document view: draws a story in Normal or Page Layout view
 * and turns mouse and keyboard input into Editor commands.
 */
#ifndef QUILL_DOCVIEW_H
#define QUILL_DOCVIEW_H

#include "Editor.h"
#include "Layout.h"
#include "Painter.h"
#include "PageRenderer.h"

#include <View.h>

class BBitmap;
class BMessageRunner;
class BScrollBar;
class DocView;

class DocViewListener {
public:
	virtual					~DocViewListener() {}
	virtual	void			DocViewChanged(DocView* view, bool content) = 0;
	virtual	void			DocViewScrolled(DocView* view) {}
	virtual	void			DocViewContextMenu(DocView* view, BPoint where)
								{}
};

class DocView : public BView {
public:
							DocView(const char* name,
								quill::Document* document,
								quill::Story* story, bool paginate,
								PictureCache* pictures);
	virtual					~DocView();

	virtual	void			AttachedToWindow();
	virtual	void			Draw(BRect updateRect);
	virtual	void			FrameResized(float width, float height);
	virtual	void			MouseDown(BPoint where);
	virtual	void			MouseMoved(BPoint where, uint32 transit,
								const BMessage* dragMessage);
	virtual	void			MouseUp(BPoint where);
	virtual	void			KeyDown(const char* bytes, int32 numBytes);
	virtual	void			MakeFocus(bool focus = true);
	virtual	void			Pulse();
	virtual	void			ScrollTo(BPoint where);
	virtual	void			MessageReceived(BMessage* message);
	virtual	void			WindowActivated(bool active);

			void			SetListener(DocViewListener* listener)
								{ fListener = listener; }
			void			SetScrollBars(BScrollBar* horizontal,
								BScrollBar* vertical);

			quill::Editor&	GetEditor() { return fEditor; }
			quill::DocLayout& Layout() { return fLayout; }
			quill::Document* GetDocument() const { return fDocument; }
			quill::Story*	GetStory() const { return fStory; }
			bool			IsBody() const
								{ return fStory == &fDocument->body; }

			void			SetMode(int mode);
			int				Mode() const { return fMode; }
			void			SetZoom(float zoom);
			float			Zoom() const { return fZoom; }
			void			SetShowAll(bool show);
			bool			ShowAll() const { return fShowAll; }
			void			SetFieldPage(int32 page) { fFieldPage = page; }
			void			SetOutlineOptions(int32 showLevel, bool firstLine,
								bool formatting);
			std::vector<bool> OutlineVisibility();
			void			SelectOutlineFamily(int32 para);

	// Call after changing the story or the selection through the Editor.
			void			Changed(bool scroll = true);
	// Call after changing settings that affect every paragraph.
			void			Relayout();
			void			ScrollToCaret();
			void			Select(quill::Pos anchor, quill::Pos caret);

			void			Cut();
			void			Copy();
			void			Paste();
			void			Clear();
			void			TypeText(const std::string& text);

	// Geometry for the ruler and the status box
			float			ColumnOriginX();
			int32			CaretPage();
			int32			TopPage();
			int32			PageAtViewY(float y);
			void			GoToPage(int32 page);
			float			PageViewLeft(int32 page);
			BRect			PageViewRect(int32 page);
			PageRenderer*	Renderer() { return fRenderer; }

			void			CopyToClipboard(const quill::Story& fragment);
			bool			StoryFromClipboard(quill::Story& fragment);

	static	float			kNormalLeft;

private:
			void			_Render(BView* view, BRect update);
			void			_RenderNormal(BView* view, BRect update);
			void			_RenderPages(BView* view, BRect update);
			void			_RenderSelection(BView* view, BRect update);
			void			_DrawCaret(BView* view);
			void			_PaintRows(PaintContext& context, BView* view,
								BRect update, int32 page);
			BRect			_CaretRect();
			void			_InvalidateCaret();

			BPoint			_ToView(int32 page, float x, float y);
			void			_FromView(BPoint where, int32* page, float* x,
								float* y);
			quill::Pos		_HitTest(BPoint where, bool* lineEnd);
			bool			_InSelectionBar(BPoint where);
			void			_UpdateScrollBars();
			BSize			_DocumentSize();
			bool			_EnsureOffscreen();
			void			_MoveCaret(quill::Pos pos, bool extend,
								bool lineEnd = false, bool keepGoal = false);
			void			_ExtendTracking(BPoint where);
			void			_LineRange(quill::Pos pos, bool lineEnd,
								quill::Pos* start, quill::Pos* end);
			void			_UnitRange(quill::Pos pos, bool lineEnd,
								quill::Pos* start, quill::Pos* end);
			void			_NextCell(bool backward);
			std::string		_SmartQuote(uint32 c);
			void			_Notify(bool content);
			void			_SetCursorFor(BPoint where);
			void			_DrawOutlineSymbols(BView* view, BRect update);
			void			_StartDrag();
			void			_Drop(BMessage* message);
			void			_ShowDropCaret(BPoint where, bool show);
			int32			_OutlineSymbolAt(BPoint where);

			DocViewListener* fListener;
			quill::Document* fDocument;
			quill::Story*	fStory;
			quill::Editor	fEditor;
			quill::DocLayout fLayout;
			PageRenderer*	fRenderer;
			PictureCache*	fPictures;
			bool			fPaginate;

			int				fMode;
			float			fZoom;
			bool			fShowAll;
			int32			fFieldPage;

			BBitmap*		fOffscreen;
			BView*			fOffscreenView;
			BScrollBar*		fHorizontal;
			BScrollBar*		fVertical;

			bool			fLineEnd;		// caret affinity at a wrap
			float			fGoalX;
			bool			fGoalValid;
			bool			fCaretOn;
			bool			fActive;

			bool			fTracking;
			int				fUnit;			// selection unit while dragging
			quill::Pos		fUnitAnchorStart;
			quill::Pos		fUnitAnchorEnd;
			BMessageRunner*	fAutoScroll;
			bool			fDragPending;
			BPoint			fDragStart;
			bool			fDropShown;
			quill::Pos		fDropPos;
			BPoint			fLastMouse;
			int				fCursorKind;
};

enum {
	kUnitChar = 0,
	kUnitWord,
	kUnitSentence,
	kUnitParagraph,
	kUnitLine,
	kUnitDocument
};

#endif	// QUILL_DOCVIEW_H
