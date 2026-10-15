/*
 * Quill - the Toolbar, the Ribbon and the Ruler of the document window.
 */
#ifndef QUILL_BARS_H
#define QUILL_BARS_H

#include "Format.h"

#include <GroupView.h>

#include <string>
#include <vector>

class IconButton;
class PopupField;
class SizeField;

class Toolbar : public BGroupView {
public:
							Toolbar();
	virtual	void			Draw(BRect updateRect);
			void			SetState(bool canUndo, bool hasSelection,
								bool showAll, int mode);
private:
			IconButton*		fUndo;
			IconButton*		fCut;
			IconButton*		fCopy;
			IconButton*		fShowAll;
			IconButton*		fNormal;
			IconButton*		fPage;
};

class Ribbon : public BGroupView {
public:
							Ribbon();
	virtual	void			Draw(BRect updateRect);
			void			SetState(const quill::CharFormat& format,
								uint32 same, bool showAll, int columns);
			void			UpdateFontList();
private:
			PopupField*		fFont;
			SizeField*		fSize;
			IconButton*		fBold;
			IconButton*		fItalic;
			IconButton*		fUnderline;
			IconButton*		fSuper;
			IconButton*		fSub;
			IconButton*		fShowAll;
			IconButton*		fColumns[3];
};

class OutlineBar : public BGroupView {
public:
							OutlineBar();
	virtual	void			Draw(BRect updateRect);
			void			SetState(int32 showLevel, bool firstLine,
								bool formatting);
private:
			IconButton*		fLevels[10];
			IconButton*		fFirstLine;
			IconButton*		fFormatting;
};

struct RulerState {
	quill::ParaFormat	format;
	uint32				same = 0;
	float				origin = 0;		// view x of the frame's zero
	float				zoom = 1;
	float				width = 432;	// frame width (column or cell)
	int					unit = quill::kUnitInch;
	float				defaultTab = 36;
	bool				pageView = false;
	float				pageLeft = 0;	// view x of the paper's left edge
	float				marginLeft = 90;
	float				marginRight = 90;
	float				paperWidth = 612;
	bool				inTable = false;
	quill::RowFormat	row;
	float				rowOrigin = 0;	// view x of the column's zero
	std::vector<std::string> styleNames;
	int32				style = 0;		// -1: mixed
};

class RulerStrip;

class Ruler : public BGroupView {
public:
							Ruler();
	virtual	void			Draw(BRect updateRect);
	virtual	void			MessageReceived(BMessage* message);
	virtual	void			AttachedToWindow();

			void			SetState(const RulerState& state);
			const RulerState& State() const { return fState; }
			int				TabKind() const { return fTabKind; }
			int				Scale() const { return fScale; }

private:
			RulerState		fState;
			int				fTabKind;
			int				fScale;		// 0 normal, 1 page, 2 table
			PopupField*		fStyle;
			IconButton*		fAlign[4];
			IconButton*		fSpacing[3];
			IconButton*		fParaSpacing[2];
			IconButton*		fTabs[5];
			IconButton*		fScaleButton;
			RulerStrip*		fStrip;
};

#endif	// QUILL_BARS_H
