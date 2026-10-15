/*
 * Quill - panes, Print Preview and printing.
 */
#include "Panes.h"

#include "Constants.h"
#include "Controls.h"
#include "Icons.h"

#include <LayoutBuilder.h>
#include <PrintJob.h>
#include <ScrollBar.h>
#include <StringView.h>
#include <Window.h>

#include <algorithm>
#include <cmath>

using namespace quill;


// #pragma mark - StoryPane


static BMessage*
PaneMessage(uint32 what)
{
	BMessage* message = new BMessage(what);
	message->AddBool("pane", true);
	return message;
}


StoryPane::StoryPane(Document* document, PictureCache* pictures)
	:
	BGroupView("pane", B_VERTICAL, 0),
	fDocument(document),
	fPictures(pictures),
	fView(nullptr),
	fScroll(nullptr),
	fKind(kPaneNone)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetFlags(Flags() | B_WILL_DRAW);
	fTitle = new BStringView("title", "Header");
	BFont font(be_bold_font);
	fTitle->SetFont(&font);
	fPageNumber = new IconButton("pageNumber", kIconPageNumber,
		PaneMessage(kMsgInsertPageNumber), "Page number");
	fDate = new IconButton("date", kIconDate, PaneMessage(kMsgInsertDate),
		"Date");
	fTime = new IconButton("time", kIconTime, PaneMessage(kMsgInsertTime),
		"Time");
	fViewGroup = new BGroupView(B_HORIZONTAL, 0);

	BLayoutBuilder::Group<>(this)
		.AddGroup(B_HORIZONTAL, 3)
			.SetInsets(6, 4, 4, 3)
			.Add(fTitle)
			.AddGlue()
			.Add(fPageNumber)
			.Add(fDate)
			.Add(fTime)
			.Add(new SeparatorBar())
			.Add(new IconButton("close", kIconClose, new BMessage(kMsgClosePane),
				"Close pane"))
		.End()
		.Add(fViewGroup);
	SetExplicitMinSize(BSize(B_SIZE_UNSET, 90));
}


void
StoryPane::Draw(BRect update)
{
	BGroupView::Draw(update);
	SetHighColor(0, 0, 0);
	StrokeLine(Bounds().LeftTop(), Bounds().RightTop());
	SetHighColor(255, 255, 255);
	StrokeLine(Bounds().LeftTop() + BPoint(0, 1),
		Bounds().RightTop() + BPoint(0, 1));
}


void
StoryPane::SetStory(Story* story, const char* title, int kind,
	DocViewListener* listener)
{
	fKind = kind;
	fTitle->SetText(title);
	bool fields = kind != kPaneFootnote;
	if (fields) {
		if (fPageNumber->IsHidden(fPageNumber)) {
			fPageNumber->Show();
			fDate->Show();
			fTime->Show();
		}
	} else {
		if (!fPageNumber->IsHidden(fPageNumber)) {
			fPageNumber->Hide();
			fDate->Hide();
			fTime->Hide();
		}
	}
	if (fView != nullptr) {
		fView->RemoveSelf();
		fScroll->RemoveSelf();
		delete fView;
		delete fScroll;
		fView = nullptr;
		fScroll = nullptr;
	}
	if (story == nullptr)
		return;
	fView = new DocView("paneView", fDocument, story, false, fPictures);
	fView->SetExplicitMinSize(BSize(50, 40));
	fScroll = new BScrollBar("paneScroll", fView, 0, 0, B_VERTICAL);
	fView->SetScrollBars(nullptr, fScroll);
	fView->SetListener(listener);
	fViewGroup->GroupLayout()->AddView(fView);
	fViewGroup->GroupLayout()->AddView(fScroll);
}


// #pragma mark - PreviewView


static const float kPreviewGap = 16;


PreviewView::PreviewView(Document* document, PageRenderer* renderer)
	:
	BView("preview", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
	fDocument(document),
	fRenderer(renderer),
	fLayout(nullptr),
	fFirstPage(0),
	fTwoPages(true),
	fMargins(false),
	fMagnified(false)
{
	SetViewColor(140, 140, 140);
}


void
PreviewView::SetLayout(DocLayout* layout)
{
	fLayout = layout;
	Invalidate();
}


void
PreviewView::SetFirstPage(int32 page)
{
	if (fLayout == nullptr)
		return;
	int32 count = fLayout->PageCount();
	page = std::max<int32>(0, std::min<int32>(page, count - 1));
	if (page != fFirstPage) {
		fFirstPage = page;
		Invalidate();
	}
}


void
PreviewView::SetTwoPages(bool two)
{
	fTwoPages = two;
	Invalidate();
}


void
PreviewView::SetMargins(bool show)
{
	fMargins = show;
	Invalidate();
}


void
PreviewView::SetMagnified(bool magnified)
{
	fMagnified = magnified;
	Invalidate();
}


float
PreviewView::_Zoom()
{
	const DocSettings& settings = fDocument->settings;
	BRect bounds = Bounds();
	int pages = fTwoPages ? 2 : 1;
	float byHeight = (bounds.Height() - 2 * kPreviewGap)
		/ settings.PaperHeight();
	float byWidth = (bounds.Width() - (pages + 1) * kPreviewGap)
		/ (pages * settings.PaperWidth());
	return std::max(0.05f, std::min(byHeight, byWidth));
}


void
PreviewView::Draw(BRect update)
{
	SetHighColor(ViewColor());
	FillRect(update);
	if (fLayout == nullptr)
		return;
	const DocSettings& settings = fDocument->settings;
	float zoom = _Zoom();
	int pages = fTwoPages ? 2 : 1;
	float width = settings.PaperWidth() * zoom;
	float height = settings.PaperHeight() * zoom;
	BRect bounds = Bounds();
	float total = pages * width + (pages - 1) * kPreviewGap;
	float left = floorf((bounds.Width() - total) / 2);
	float top = floorf((bounds.Height() - height) / 2);
	for (int i = 0; i < pages; i++) {
		int32 page = fFirstPage + i;
		if (page >= fLayout->PageCount())
			break;
		BPoint origin(left + i * (width + kPreviewGap), top);
		PageRenderer::Options options;
		options.zoom = zoom;
		options.printing = true;
		fRenderer->RenderPage(this, *fLayout, page, origin, update, options);
		if (fMargins) {
			float textLeft = fLayout->PageTextLeft(page);
			SetHighColor(0, 0, 160);
			BRect paper(origin.x, origin.y, origin.x + width,
				origin.y + height);
			float x0 = origin.x + textLeft * zoom;
			float x1 = origin.x + (textLeft + settings.TextWidth()) * zoom;
			float y0 = origin.y + settings.marginTop * zoom;
			float y1 = origin.y + height - settings.marginBottom * zoom;
			StrokeDotted(this, BPoint(paper.left, y0), BPoint(paper.right, y0),
				3);
			StrokeDotted(this, BPoint(paper.left, y1), BPoint(paper.right, y1),
				3);
			StrokeDotted(this, BPoint(x0, paper.top), BPoint(x0, paper.bottom),
				3);
			StrokeDotted(this, BPoint(x1, paper.top), BPoint(x1, paper.bottom),
				3);
			float headerY = origin.y + settings.headerY * zoom;
			float footerY = origin.y + height - settings.footerY * zoom;
			SetHighColor(160, 0, 0);
			StrokeDotted(this, BPoint(x0, headerY), BPoint(x1, headerY), 3);
			StrokeDotted(this, BPoint(x0, footerY), BPoint(x1, footerY), 3);
		}
	}
}


void
PreviewView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	Invalidate();
}


void
PreviewView::KeyDown(const char* bytes, int32 numBytes)
{
	int32 step = fTwoPages ? 2 : 1;
	switch ((unsigned char)bytes[0]) {
		case B_PAGE_DOWN:
		case B_DOWN_ARROW:
		case B_RIGHT_ARROW:
			SetFirstPage(fFirstPage + step);
			break;
		case B_PAGE_UP:
		case B_UP_ARROW:
		case B_LEFT_ARROW:
			SetFirstPage(fFirstPage - step);
			break;
		case B_HOME:
			SetFirstPage(0);
			break;
		case B_END:
			if (fLayout != nullptr)
				SetFirstPage(fLayout->PageCount() - 1);
			break;
		case B_ESCAPE:
			Window()->PostMessage(kMsgPrintPreview);
			break;
		default:
			BView::KeyDown(bytes, numBytes);
			break;
	}
	Window()->PostMessage(kMsgSelectionChanged);
}


void
PreviewView::MouseDown(BPoint where)
{
	(void)where;
	MakeFocus(true);
}


// #pragma mark - printing


namespace {

class PrintView : public BView {
public:
	PrintView(BRect frame, Document* document, DocLayout& body,
		PictureCache* pictures)
		:
		BView(frame, "print", B_FOLLOW_NONE, B_WILL_DRAW),
		fBody(body),
		fRenderer(document, pictures),
		fPage(0)
	{
	}

	void SetPage(int32 page) { fPage = page; }

	virtual void Draw(BRect update)
	{
		PageRenderer::Options options;
		options.zoom = 1;
		options.printing = true;
		options.paper = false;
		options.gridlines = false;
		fRenderer.RenderPage(this, fBody, fPage, BPoint(0, 0), update,
			options);
	}

private:
	DocLayout&		fBody;
	PageRenderer	fRenderer;
	int32			fPage;
};

}	// namespace


status_t
PrintDocument(Document* document, DocLayout& body, PictureCache* pictures,
	BMessage** settings, const char* name)
{
	BPrintJob job(name);
	if (*settings != nullptr)
		job.SetSettings(new BMessage(**settings));
	status_t status = job.ConfigJob();
	if (status != B_OK)
		return status;
	delete *settings;
	*settings = job.Settings();

	BRect paper = job.PaperRect();
	BRect printable = job.PrintableRect();
	int32 count = body.PageCount();
	int32 first = std::max<int32>(1, job.FirstPage());
	int32 last = std::min<int32>(count, job.LastPage());
	if (last < first)
		last = count;

	const DocSettings& s = document->settings;
	BRect frame(0, 0, std::max(s.PaperWidth(), paper.Width()),
		std::max(s.PaperHeight(), paper.Height()));
	BWindow* window = new BWindow(frame, "print", B_NO_BORDER_WINDOW_LOOK,
		B_NORMAL_WINDOW_FEEL, B_AVOID_FOCUS);
	PrintView* view = new PrintView(frame, document, body, pictures);
	window->AddChild(view);
	window->Run();

	job.BeginJob();
	BRect area = printable.OffsetByCopy(-paper.left, -paper.top);
	for (int32 page = first; page <= last; page++) {
		if (!job.CanContinue())
			break;
		if (window->Lock()) {
			view->SetPage(page - 1);
			job.DrawView(view, area, BPoint(0, 0));
			window->Unlock();
		}
		job.SpoolPage();
	}
	if (job.CanContinue())
		job.CommitJob();
	else
		status = B_CANCELED;
	if (window->Lock())
		window->Quit();
	return status;
}
