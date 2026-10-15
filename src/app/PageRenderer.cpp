/*
 * Quill - page drawing.
 */
#include "PageRenderer.h"

#include "Fonts.h"
#include "Prefs.h"
#include "Tools.h"

#include <ctime>

using namespace quill;


PageRenderer::PageRenderer(Document* document, PictureCache* pictures)
	:
	fDocument(document),
	fPictures(pictures)
{
	Story* stories[4] = { &document->header, &document->footer,
		&document->firstHeader, &document->firstFooter };
	for (int i = 0; i < 4; i++) {
		fHeaders[i] = new DocLayout(&gMetrics);
		fHeaders[i]->SetStory(document, stories[i], false);
	}
}


PageRenderer::~PageRenderer()
{
	for (int i = 0; i < 4; i++)
		delete fHeaders[i];
}


void
PageRenderer::Invalidate()
{
	for (int i = 0; i < 4; i++)
		fHeaders[i]->Invalidate();
}


void
PageRenderer::_UpdateHeaders(DocLayout& body, int32 page, bool showHidden)
{
	const std::vector<PageInfo>& pages = body.Pages();
	LayoutOptions options;
	options.showHidden = showHidden;
	options.pageNumber = page < (int32)pages.size() ? pages[page].number
		: page + 1;
	options.pageCount = body.PageCount();
	time_t now = time(nullptr);
	options.date = FormatDate(now, false);
	options.time = FormatTime(now);
	for (int i = 0; i < 4; i++) {
		fHeaders[i]->SetOptions(options);
		fHeaders[i]->Update();
	}
}


void
PageRenderer::RenderPage(BView* view, DocLayout& body, int32 page,
	BPoint origin, BRect update, const Options& options)
{
	const DocSettings& settings = fDocument->settings;
	const float zoom = options.zoom;
	BRect rect(origin.x, origin.y, origin.x + settings.PaperWidth() * zoom,
		origin.y + settings.PaperHeight() * zoom);

	if (options.paper) {
		view->SetHighColor(70, 70, 70);
		view->FillRect(rect.OffsetByCopy(3, 3));
		view->SetHighColor(255, 255, 255);
		view->FillRect(rect);
		view->SetHighColor(0, 0, 0);
		view->StrokeRect(rect);
	}

	float textLeft = body.PageTextLeft(page);
	if (options.boundaries && !options.printing) {
		BRect text(rect.left + textLeft * zoom,
			rect.top + settings.marginTop * zoom,
			rect.left + (textLeft + settings.TextWidth()) * zoom,
			rect.bottom - settings.marginBottom * zoom);
		view->SetHighColor(185, 185, 185);
		StrokeDotted(view, text.LeftTop(), text.RightTop(), 2);
		StrokeDotted(view, text.LeftBottom(), text.RightBottom(), 2);
		StrokeDotted(view, text.LeftTop(), text.LeftBottom(), 2);
		StrokeDotted(view, text.RightTop(), text.RightBottom(), 2);
	}

	PaintContext context;
	context.view = view;
	context.document = fDocument;
	context.zoom = zoom;
	context.showAll = options.showAll;
	context.printing = options.printing;
	context.pictures = fPictures;

	// Headers and footers
	bool showHidden = options.printing ? settings.printHidden
		: (gPrefs.showHidden || options.showAll);
	_UpdateHeaders(body, page, showHidden);
	bool first = page == 0 && settings.titlePage;
	for (int part = 0; part < 2; part++) {
		DocLayout* layout = fHeaders[(first ? 2 : 0) + part];
		const Story& story = *layout->GetStory();
		float total = 0;
		for (int32 i = 0; i < layout->ParaCount(); i++)
			total += layout->Para(i).height;
		float y = part == 0 ? settings.headerY
			: settings.PaperHeight() - settings.footerY - total;
		for (int32 i = 0; i < layout->ParaCount() && i < (int32)story.size();
				i++) {
			const ParaLayout& paraLayout = layout->Para(i);
			for (int32 l = 0; l < (int32)paraLayout.lines.size(); l++) {
				PaintLine(context, story[i], paraLayout, l,
					BPoint(rect.left + textLeft * zoom,
						rect.top + (y + paraLayout.lines[l].top) * zoom));
			}
			y += paraLayout.height;
		}
	}

	// Table grids under the text
	const Story& story = *body.GetStory();
	for (const Block& block : body.Blocks()) {
		if (!block.row || block.page != page
			|| block.lastPara >= (int32)story.size())
			continue;
		PaintRowGrid(context, story[block.lastPara].row,
			rect.left + block.pageX * zoom, rect.top + block.pageTop * zoom,
			(block.pageBottom - block.pageTop) * zoom, body.LayoutWidth(),
			options.gridlines && gPrefs.tableGridlines);
	}

	// Body text
	const PageInfo& info = body.Pages()[page];
	const std::vector<PlacedLine>& lines = body.Lines();
	for (int32 l = info.firstLine; l < info.endLine
			&& l < (int32)lines.size(); l++) {
		const PlacedLine& placed = lines[l];
		if (story[placed.para].kind == kParaRowEnd
			&& (!options.showAll || options.printing))
			continue;
		float top = rect.top + placed.pageTop * zoom;
		float height = placed.height * zoom;
		if (top - 24 > update.bottom || top + height + 24 < update.top)
			continue;
		PaintLine(context, story[placed.para], body.Para(placed.para),
			placed.line, BPoint(rect.left + placed.pageX * zoom, top));
	}

	// Footnotes
	for (const FootnotePlace& place : body.FootnotePlaces()) {
		if (place.page != page)
			continue;
		float x = rect.left + place.pageX * zoom;
		float y = rect.top + place.pageTop * zoom;
		if (place.separator) {
			view->SetHighColor(0, 0, 0);
			float lineY = floorf(y + 6 * zoom);
			view->StrokeLine(BPoint(x, lineY), BPoint(x + 144 * zoom, lineY));
			continue;
		}
		const std::vector<ParaLayout>* layouts = body.FootnoteLayout(place.id);
		auto it = fDocument->footnotes.find(place.id);
		if (layouts == nullptr || it == fDocument->footnotes.end())
			continue;
		const Story& note = it->second;
		float noteY = place.pageTop;
		for (size_t i = 0; i < layouts->size() && i < note.size(); i++) {
			const ParaLayout& layout = (*layouts)[i];
			for (int32 l = 0; l < (int32)layout.lines.size(); l++) {
				PaintLine(context, note[i], layout, l, BPoint(x,
					rect.top + (noteY + layout.lines[l].top) * zoom));
			}
			noteY += layout.height;
		}
	}
}
