/*
 * Quill - formats in BMessages.
 */
#include "FormatMessages.h"

using namespace quill;


void
AddCharFormat(BMessage* message, const CharFormat& format)
{
	message->AddString("cf:font", format.font.c_str());
	message->AddFloat("cf:size", format.size);
	message->AddBool("cf:bold", format.bold);
	message->AddBool("cf:italic", format.italic);
	message->AddBool("cf:strike", format.strike);
	message->AddBool("cf:outline", format.outline);
	message->AddBool("cf:shadow", format.shadow);
	message->AddBool("cf:smallCaps", format.smallCaps);
	message->AddBool("cf:allCaps", format.allCaps);
	message->AddBool("cf:hidden", format.hidden);
	message->AddInt32("cf:underline", format.underline);
	message->AddFloat("cf:position", format.position);
	message->AddFloat("cf:spacing", format.spacing);
	message->AddUInt32("cf:color", format.color);
}


void
FindCharFormat(const BMessage* message, CharFormat* format)
{
	const char* font;
	if (message->FindString("cf:font", &font) == B_OK)
		format->font = font;
	message->FindFloat("cf:size", &format->size);
	message->FindBool("cf:bold", &format->bold);
	message->FindBool("cf:italic", &format->italic);
	message->FindBool("cf:strike", &format->strike);
	message->FindBool("cf:outline", &format->outline);
	message->FindBool("cf:shadow", &format->shadow);
	message->FindBool("cf:smallCaps", &format->smallCaps);
	message->FindBool("cf:allCaps", &format->allCaps);
	message->FindBool("cf:hidden", &format->hidden);
	int32 underline;
	if (message->FindInt32("cf:underline", &underline) == B_OK)
		format->underline = (uint8)underline;
	message->FindFloat("cf:position", &format->position);
	message->FindFloat("cf:spacing", &format->spacing);
	uint32 color;
	if (message->FindUInt32("cf:color", &color) == B_OK)
		format->color = color;
}


void
AddBorder(BMessage* message, const ParaBorder& border)
{
	message->AddInt32("b:top", border.top);
	message->AddInt32("b:bottom", border.bottom);
	message->AddInt32("b:left", border.left);
	message->AddInt32("b:right", border.right);
	message->AddInt32("b:between", border.between);
	message->AddBool("b:shadow", border.shadow);
	message->AddInt32("b:shading", border.shading);
	message->AddFloat("b:space", border.space);
}


void
FindBorder(const BMessage* message, ParaBorder* border)
{
	int32 value;
	if (message->FindInt32("b:top", &value) == B_OK) border->top = value;
	if (message->FindInt32("b:bottom", &value) == B_OK) border->bottom = value;
	if (message->FindInt32("b:left", &value) == B_OK) border->left = value;
	if (message->FindInt32("b:right", &value) == B_OK) border->right = value;
	if (message->FindInt32("b:between", &value) == B_OK)
		border->between = value;
	message->FindBool("b:shadow", &border->shadow);
	if (message->FindInt32("b:shading", &value) == B_OK)
		border->shading = value;
	message->FindFloat("b:space", &border->space);
}


void
AddTabs(BMessage* message, const std::vector<TabStop>& tabs)
{
	for (const TabStop& tab : tabs) {
		message->AddFloat("pos", tab.pos);
		message->AddInt32("kind", tab.kind);
		message->AddInt32("leader", tab.leader);
	}
}


void
FindTabs(const BMessage* message, std::vector<TabStop>* tabs)
{
	tabs->clear();
	float pos;
	for (int32 i = 0; message->FindFloat("pos", i, &pos) == B_OK; i++) {
		TabStop tab;
		tab.pos = pos;
		int32 value;
		if (message->FindInt32("kind", i, &value) == B_OK)
			tab.kind = (uint8)value;
		if (message->FindInt32("leader", i, &value) == B_OK)
			tab.leader = (uint8)value;
		tabs->push_back(tab);
	}
}


void
AddParaFormat(BMessage* message, const ParaFormat& format)
{
	message->AddInt32("pf:align", format.align);
	message->AddFloat("pf:left", format.leftIndent);
	message->AddFloat("pf:right", format.rightIndent);
	message->AddFloat("pf:first", format.firstIndent);
	message->AddInt32("pf:lineRule", format.lineRule);
	message->AddFloat("pf:lineValue", format.lineValue);
	message->AddFloat("pf:before", format.spaceBefore);
	message->AddFloat("pf:after", format.spaceAfter);
	message->AddBool("pf:keepNext", format.keepWithNext);
	message->AddBool("pf:keepTogether", format.keepTogether);
	message->AddBool("pf:breakBefore", format.pageBreakBefore);
	message->AddBool("pf:noLineNumbers", format.noLineNumbers);
	message->AddInt32("pf:style", format.style);
	AddBorder(message, format.border);
	AddTabs(message, format.tabs);
}


void
FindParaFormat(const BMessage* message, ParaFormat* format)
{
	int32 value;
	if (message->FindInt32("pf:align", &value) == B_OK)
		format->align = (uint8)value;
	message->FindFloat("pf:left", &format->leftIndent);
	message->FindFloat("pf:right", &format->rightIndent);
	message->FindFloat("pf:first", &format->firstIndent);
	if (message->FindInt32("pf:lineRule", &value) == B_OK)
		format->lineRule = (uint8)value;
	message->FindFloat("pf:lineValue", &format->lineValue);
	message->FindFloat("pf:before", &format->spaceBefore);
	message->FindFloat("pf:after", &format->spaceAfter);
	message->FindBool("pf:keepNext", &format->keepWithNext);
	message->FindBool("pf:keepTogether", &format->keepTogether);
	message->FindBool("pf:breakBefore", &format->pageBreakBefore);
	message->FindBool("pf:noLineNumbers", &format->noLineNumbers);
	if (message->FindInt32("pf:style", &value) == B_OK)
		format->style = value;
	FindBorder(message, &format->border);
	if (message->HasFloat("pos"))
		FindTabs(message, &format->tabs);
}


void
AddSettings(BMessage* message, const DocSettings& s)
{
	message->AddFloat("s:pageWidth", s.pageWidth);
	message->AddFloat("s:pageHeight", s.pageHeight);
	message->AddBool("s:landscape", s.landscape);
	message->AddInt32("s:scale", s.scale);
	message->AddFloat("s:marginTop", s.marginTop);
	message->AddFloat("s:marginBottom", s.marginBottom);
	message->AddFloat("s:marginLeft", s.marginLeft);
	message->AddFloat("s:marginRight", s.marginRight);
	message->AddFloat("s:gutter", s.gutter);
	message->AddBool("s:mirror", s.mirrorMargins);
	message->AddFloat("s:defaultTab", s.defaultTab);
	message->AddBool("s:widowControl", s.widowControl);
	message->AddBool("s:printHidden", s.printHidden);
	message->AddInt32("s:footnotePos", s.footnotePos);
	message->AddInt32("s:footnoteStart", s.footnoteStart);
	message->AddBool("s:footnoteRestart", s.footnoteRestartEachPage);
	message->AddInt32("s:sectionStart", s.sectionStart);
	message->AddInt32("s:columns", s.columns);
	message->AddFloat("s:columnSpacing", s.columnSpacing);
	message->AddFloat("s:headerY", s.headerY);
	message->AddFloat("s:footerY", s.footerY);
	message->AddBool("s:titlePage", s.titlePage);
	message->AddInt32("s:pageNumberStart", s.pageNumberStart);
	message->AddInt32("s:pageNumberFormat", s.pageNumberFormat);
	message->AddBool("s:marginPageNumbers", s.marginPageNumbers);
	message->AddFloat("s:pageNumberTop", s.pageNumberTop);
	message->AddFloat("s:pageNumberRight", s.pageNumberRight);
	message->AddInt32("s:lineNumbering", s.lineNumbering);
	message->AddFloat("s:lineNumberDistance", s.lineNumberDistance);
}


void
FindSettings(const BMessage* message, DocSettings* s)
{
	int32 value;
	message->FindFloat("s:pageWidth", &s->pageWidth);
	message->FindFloat("s:pageHeight", &s->pageHeight);
	message->FindBool("s:landscape", &s->landscape);
	message->FindInt32("s:scale", &s->scale);
	message->FindFloat("s:marginTop", &s->marginTop);
	message->FindFloat("s:marginBottom", &s->marginBottom);
	message->FindFloat("s:marginLeft", &s->marginLeft);
	message->FindFloat("s:marginRight", &s->marginRight);
	message->FindFloat("s:gutter", &s->gutter);
	message->FindBool("s:mirror", &s->mirrorMargins);
	message->FindFloat("s:defaultTab", &s->defaultTab);
	message->FindBool("s:widowControl", &s->widowControl);
	message->FindBool("s:printHidden", &s->printHidden);
	if (message->FindInt32("s:footnotePos", &value) == B_OK)
		s->footnotePos = (uint8)value;
	message->FindInt32("s:footnoteStart", &s->footnoteStart);
	message->FindBool("s:footnoteRestart", &s->footnoteRestartEachPage);
	if (message->FindInt32("s:sectionStart", &value) == B_OK)
		s->sectionStart = (uint8)value;
	message->FindInt32("s:columns", &s->columns);
	message->FindFloat("s:columnSpacing", &s->columnSpacing);
	message->FindFloat("s:headerY", &s->headerY);
	message->FindFloat("s:footerY", &s->footerY);
	message->FindBool("s:titlePage", &s->titlePage);
	message->FindInt32("s:pageNumberStart", &s->pageNumberStart);
	if (message->FindInt32("s:pageNumberFormat", &value) == B_OK)
		s->pageNumberFormat = (uint8)value;
	message->FindBool("s:marginPageNumbers", &s->marginPageNumbers);
	message->FindFloat("s:pageNumberTop", &s->pageNumberTop);
	message->FindFloat("s:pageNumberRight", &s->pageNumberRight);
	message->FindInt32("s:lineNumbering", &s->lineNumbering);
	message->FindFloat("s:lineNumberDistance", &s->lineNumberDistance);
}
